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
#include "RenderServer.h"
#include "FrameBuffer.h"
#include "RenderThread.h"
#include "UnixStream.h"
#include <signal.h>
#include <pthread.h>

#include "OpenglRender/render_api.h"

#include <set>

#include <string.h>

#include <syslog.h>

#include <assert.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <libgen.h>
namespace emugl {

//渲染管道字符匹配
#define PIPE_OPENGLES "pipe:opengles"

//前后台切换字符匹配
//为了保持读的长度，PIPE_TRANSFER 与 PIPE_OPENGLES 字符长度必须一致
#define PIPE_TRANSFER "pipe:transfer"

typedef std::set<RenderThread *> RenderThreadsSet;

RenderServer::RenderServer() :
    m_lock(),
    m_listenSock(NULL),
    m_exiting(false) {
}

RenderServer::~RenderServer() {
    delete m_listenSock;
}

emugl::Mutex * RenderServer::getmlock() {
    return &m_lock;
}
//extern "C" int gRendererStreamMode;

RenderServer *RenderServer::create(char* addr, size_t addrLen) {
    //syslog(LOG_DEBUG," RenderServer create");
    RenderServer *server = new RenderServer();
    if (!server) {
        return NULL;
    }   
     server->m_listenSock = new UnixStream();
    //syslog(LOG_DEBUG," RenderServer 222");
    char addrstr[ChannelStream::MAX_ADDRSTR_LEN];
    if (server->m_listenSock->listen(addrstr) < 0) {
        ERR("RenderServer::create failed to listen\n");
        delete server;
        return NULL;
    }
    //syslog(LOG_DEBUG," RenderServer 333");
    //syslog(LOG_DEBUG," rendererAddress = %s",addrstr);
    
    size_t len = strlen(addrstr) + 1;
    if (len > addrLen) {
        ERR("RenderServer address name too big for provided buffer: %zu > %zu\n",
                len, addrLen);
        delete server;
        return NULL;
    }
    //syslog(LOG_DEBUG," RenderServer 444");
    
    memcpy(addr, addrstr, len);        
    chmod(addrstr, 0777);
    chmod(dirname(addrstr), 0777);    
        
    return server;
}

intptr_t RenderServer::main() {
    RenderThreadsSet threads;
    char temp[128];

#ifndef _WIN32
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
#endif
    int count = 1;
    while(1) {
        ChannelStream *stream = m_listenSock->accept();
        if (!stream) {
            fprintf(stderr,"Error accepting gles connection, ignoring.\n");
            continue;
        }

        memset(temp, 0, 128);

        if(!stream->readFully(temp, strlen(PIPE_OPENGLES) + 1)) {
            fprintf(stderr,"Error reading header\n");
            delete stream;
            continue;
        }

        if(strncmp(temp, PIPE_OPENGLES, strlen(PIPE_OPENGLES)) != 0) {
            //PIPE_TRANSFER 与 PIPE_OPENGLES 字符长度必须一致
            //如果不是PIPE_OPENGLES 也不是PIPE_TRANSFER，跳过进行下一次
            if(strncmp(temp, PIPE_TRANSFER, strlen(PIPE_TRANSFER)) != 0)
            {
                fprintf(stderr,"it is not %s: %s\n", PIPE_OPENGLES, temp);
                delete stream;
                continue;
            }            
            char * result = "OK";
            unsigned char *tmpBuf = stream->alloc(strlen(result)+1);
            memcpy((char *)tmpBuf, result, strlen(result)+1);
            stream->flush();
        }        

        unsigned int clientFlags;
        if (!stream->readFully(&clientFlags, sizeof(unsigned int))) {
            fprintf(stderr,"Error reading clientFlags\n");
            delete stream;
            continue;
        }

        //DBG("RenderServer: Got new stream!\n");

        // check if we have been requested to exit while waiting on accept
        if ((clientFlags & IOSTREAM_CLIENT_EXIT_SERVER) != 0) {
            m_exiting = true;
            delete stream;
            break;
        }
        //syslog(LOG_DEBUG,"TTTT RenderThread::create count = %d",count);
        RenderThread *rt = RenderThread::create(stream, &m_lock);
        if (!rt) {
            fprintf(stderr,"Failed to create RenderThread\n");
            delete stream;
        } else if (!rt->start()) {
            fprintf(stderr,"Failed to start RenderThread\n");
            delete rt;
            delete stream;
        }

        //
        // remove from the threads list threads which are
        // no longer running
        //
        for (RenderThreadsSet::iterator n,t = threads.begin();
             t != threads.end();
             t = n) {
            // first find next iterator
            n = t;
            n++;

            // delete and erase the current iterator
            // if thread is no longer running
            if ((*t)->isFinished()) {
                pthread_t tid = (*t)->gettid();
                FrameBuffer *fb = FrameBuffer::getFB();
                fb->closePthreadAloneColorBuffer(tid);                
                delete (*t);
                threads.erase(t);
            }
        }

        // if the thread has been created and started, insert it to the list
        if (rt) {
            threads.insert(rt);
            //syslog(LOG_DEBUG,"UUUU threads.insert(rt) count = %d",count);
            //DBG("Started new RenderThread\n");
        }        
        if(count < 10000) {
            count ++;
            //syslog(LOG_DEBUG,"PPPP threads.insert(rt) count +1");
        }
            
    }

    //
    // Wait for all threads to finish
    //
    for (RenderThreadsSet::iterator t = threads.begin();
         t != threads.end();
         t++) {
        (*t)->forceStop();
        (*t)->wait(NULL);
        delete (*t);
    }
    threads.clear();

    return 0;
}

}  // namespace emugl
