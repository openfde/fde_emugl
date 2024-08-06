/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "NativeSubWindow.h"
#include <unordered_map>
#include <thread>
#include <chrono>
#include <mutex>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg) {
    if (e->type == MapNotify && e->xmap.window == (Window)arg) {
        return 1;
    }
    return 0;
}

static Bool WaitForConfigureNotify(Display *d, XEvent *e, char *arg) {
    if (e->type == ConfigureNotify && e->xmap.window == (Window)arg) {
        return 1;
    }
    return 0;
}

static std::mutex gWinMapMtx;
static std::unordered_map<Window, Window> gWinMap;
static std::thread* gWatchThread = nullptr;

typedef struct {
    unsigned long windId;
    int32_t type;// 0 -> leave event; 1 -> enter event
    int32_t posX;
    int32_t posY;
}EventMsg;

static void WatchLeaveEventThread(Display *display)
{
    syslog(LOG_DEBUG, "Start WatchLeaveEventThread...");

    XEvent event;
    EventMsg eventMsg;
    pid_t pid = getpid();
    sigval mysigval;
    mysigval.sival_ptr = &eventMsg;
    uint32_t sleepTime = 10;// ms

    while(true) {
        {
            std::lock_guard<std::mutex> lk(gWinMapMtx);
            sleepTime = (gWinMap.size() > 0) ? 10 : 1000;
        }
        while(XPending(display) > 0) {
            event.type = LASTEvent;
            XNextEvent(display, &event);
            if ((event.type == LeaveNotify) && (event.xcrossing.mode == NotifyNormal)) {
                Window parWin = 0;
                {
                    std::lock_guard<std::mutex> lk(gWinMapMtx);
                    if (gWinMap.count(event.xcrossing.window) > 0) {
                        parWin = gWinMap[event.xcrossing.window];
                    }
                }
                
                if (parWin > 0) {
                    eventMsg.type = 0;
                    eventMsg.windId = parWin;
                    eventMsg.posX = event.xcrossing.x_root;
                    eventMsg.posY = event.xcrossing.y_root;
                    
                    if (sigqueue(pid, SIGUSR1, mysigval) == -1) {
                        syslog(LOG_ERR, "Send signal 'SIGUSR1' failed!");
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    }
}

static Display *s_display = NULL;

EGLNativeWindowType createSubWindow(FBNativeWindowType p_window,
                                    int x,
                                    int y,
                                    int width,
                                    int height,
                                    SubWindowRepaintCallback repaint_callback,
                                    void* repaint_callback_param,
                                    int hideWindow) {
   // The call to this function is protected by a lock
   // in FrameBuffer so it is safe to check and initialize s_display here
   if (!s_display) {
       s_display = XOpenDisplay(NULL);
   }

    XSetWindowAttributes wa;
    wa.event_mask = StructureNotifyMask;
    wa.override_redirect = True;
    Window win = XCreateWindow(s_display,
                               p_window,
                               x,
                               y,
                               width,
                               height,
                               0,
                               CopyFromParent,
                               CopyFromParent,
                               CopyFromParent,
                               CWEventMask,
                               &wa);
    if (!hideWindow) {
        XMapWindow(s_display,win);
        XSetWindowBackground(s_display, win, BlackPixel(s_display, 0));
        XEvent e;
        XIfEvent(s_display, &e, WaitForMapNotify, (char *)win);
    }

    XSelectInput(s_display, win, LeaveWindowMask);
    {
        std::lock_guard<std::mutex> lk(gWinMapMtx);
        gWinMap[win] = p_window;
    }
    
    if (!gWatchThread) {
        gWatchThread = new std::thread(WatchLeaveEventThread, s_display);
        //detach this thread will cause x11 blocked occasionally on x100 platform, why ?
        //gWatchThread->detach();
    }

    return win;
}

void destroySubWindow(EGLNativeWindowType win) {
    if (!s_display) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(gWinMapMtx);
        gWinMap.erase(win);
    }
    //XDestroyWindow(s_display, win);
    //s_display = XOpenDisplay(NULL);//fixed next create window crash bug, when the previous window close; by huangcailong
}

int moveSubWindow(FBNativeWindowType p_parent_window,
                  EGLNativeWindowType p_sub_window,
                  int x,
                  int y,
                  int width,
                  int height) {
    // This value is set during create, so if it is still null, simply
    // return because the global state is corrupted
    if (!s_display) {
        return false;
    }

    // Make sure something has changed, otherwise XIfEvent will block and
    // freeze the emulator.
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(s_display, p_sub_window, &attrs)) {
        return false;
    }
    if (x == attrs.x && y == attrs.y &&
        width == attrs.width && height == attrs.height) {
        // Technically, resizing was a success because it was unneeded.
        return true;
    }

    // This prevents flicker on resize.
    XSetWindowBackgroundPixmap(s_display, p_sub_window, None);

    int ret = XMoveResizeWindow(
                s_display,
                p_sub_window,
                x,
                y,
                width,
                height);

    XEvent e;
    //XIfEvent(s_display, &e, WaitForConfigureNotify, (char *)p_sub_window);
    if (!XCheckIfEvent(s_display, &e, WaitForConfigureNotify, (char *)p_sub_window)) {
        syslog(LOG_WARNING,"[%s] Check 'ConfigureNotify' event failed!", __func__);
    }

    return ret;
}
