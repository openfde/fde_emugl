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
#include "RenderThread.h"

#include "ChannelStream.h"
#include "RingStream.h"
#include "UnixStream.h"
#include "ErrorLog.h"
#include "FrameBuffer.h"
#include "ReadBuffer.h"
#include "RenderControl.h"
#include "RendererImpl.h"
#include "RenderChannelImpl.h"
#include "RenderThreadInfo.h"

#include "OpenGLESDispatch/EGLDispatch.h"
#include "OpenGLESDispatch/GLESv2Dispatch.h"
#include "OpenGLESDispatch/GLESv1Dispatch.h"
#include "../../../shared/OpenglCodecCommon/ChecksumCalculatorThreadInfo.h"

#include "android/base/system/System.h"
#include "android/base/Tracing.h"
#include "android/base/files/StreamSerializing.h"
#include "android/utils/path.h"
#include "android/utils/file_io.h"

#define EMUGL_DEBUG_LEVEL 0
#include "emugl/common/crash_reporter.h"
#include "emugl/common/debug.h"

#include <assert.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <libgen.h>
#include <syslog.h>

using android::base::AutoLock;

namespace emugl {

struct RenderThread::SnapshotObjects {
    RenderThreadInfo* threadInfo;
    ChecksumCalculator* checksumCalc;
    ChannelStream* channelStream;
    RingStream* ringStream;
    ReadBuffer* readBuffer;
};

static bool getBenchmarkEnabledFromEnv() {
    auto threadEnabled = android::base::System::getEnvironmentVariable("ANDROID_EMUGL_RENDERTHREAD_STATS");
    if (threadEnabled == "1") return true;
    return false;
}

static uint64_t currTimeUs(bool enable) {
    if (enable) {
        return android::base::System::get()->getHighResTimeUs();
    } else {
        return 0;
    }
}

// Start with a smaller buffer to not waste memory on a low-used render threads.
static constexpr int kStreamBufferSize = 128 * 1024;

RenderThread::RenderThread(RenderChannelImpl* channel,
                           android::base::Stream* loadStream)
    : emugl::Thread(android::base::ThreadFlags::MaskSignals, 2 * 1024 * 1024),
      mChannel(channel) {
    if (loadStream) {
        const bool success = loadStream->getByte();
        if (success) {
            mStream.emplace(0);
            android::base::loadStream(loadStream, &*mStream);
            mState = SnapshotState::StartLoading;
        } else {
            mFinished.store(true, std::memory_order_relaxed);
        }
    }
}

RenderThread::RenderThread(
        struct asg_context context,
        android::emulation::asg::ConsumerCallbacks callbacks,
        android::base::Stream* loadStream)
    : emugl::Thread(android::base::ThreadFlags::MaskSignals, 2 * 1024 * 1024),
      mRingStream(
          new RingStream(context, callbacks, kStreamBufferSize)) {
    if (loadStream) {
        const bool success = loadStream->getByte();
        if (success) {
            mStream.emplace(0);
            android::base::loadStream(loadStream, &*mStream);
            mState = SnapshotState::StartLoading;
        } else {
            mFinished.store(true, std::memory_order_relaxed);
        }
    }
}

RenderThread::RenderThread(ChannelStream *stream, emugl::Mutex *lock) :
        emugl::Thread(),
        m_lock(lock),
        m_stream(stream) {}


RenderThread::~RenderThread() {
    delete m_stream;
}

void RenderThread::pausePreSnapshot() {
    AutoLock lock(mLock);
    assert(mState == SnapshotState::Empty);
    mStream.emplace();
    mState = SnapshotState::StartSaving;
    if (mChannel) mChannel->pausePreSnapshot();
    mCondVar.broadcastAndUnlock(&lock);
}

void RenderThread::resume() {
    AutoLock lock(mLock);
    // This function can be called for a thread from pre-snapshot loading
    // state; it doesn't need to do anything.
    if (mState == SnapshotState::Empty) {
        return;
    }
    waitForSnapshotCompletion(&lock);
    mStream.clear();
    mState = SnapshotState::Empty;
    if (mChannel) mChannel->resume();
    mCondVar.broadcastAndUnlock(&lock);
}

void RenderThread::save(android::base::Stream* stream) {
    bool success;
    {
        AutoLock lock(mLock);
        assert(mState == SnapshotState::StartSaving ||
               mState == SnapshotState::InProgress ||
               mState == SnapshotState::Finished);
        waitForSnapshotCompletion(&lock);
        success = mState == SnapshotState::Finished;
    }

    if (success) {
        assert(mStream);
        stream->putByte(1);
        android::base::saveStream(stream, *mStream);
    } else {
        stream->putByte(0);
    }
}

void RenderThread::waitForSnapshotCompletion(AutoLock* lock) {
    while (mState != SnapshotState::Finished &&
           !mFinished.load(std::memory_order_relaxed)) {
        mCondVar.wait(lock);
    }
}

template <class OpImpl>
void RenderThread::snapshotOperation(AutoLock* lock, OpImpl&& implFunc) {
    assert(isPausedForSnapshotLocked());
    mState = SnapshotState::InProgress;
    mCondVar.broadcastAndUnlock(lock);

    implFunc();

    lock->lock();

    mState = SnapshotState::Finished;
    mCondVar.broadcast();

    // Only return after we're allowed to proceed.
    while (isPausedForSnapshotLocked()) {
        mCondVar.wait(lock);
    }
}

void RenderThread::loadImpl(AutoLock* lock, const SnapshotObjects& objects) {
    snapshotOperation(lock, [this, &objects] {
        objects.readBuffer->onLoad(&*mStream);
        if (objects.channelStream) objects.channelStream->load(&*mStream);
        if (objects.ringStream) objects.ringStream->load(&*mStream);
        objects.checksumCalc->load(&*mStream);
        objects.threadInfo->onLoad(&*mStream);
    });
}

void RenderThread::saveImpl(AutoLock* lock, const SnapshotObjects& objects) {
    snapshotOperation(lock, [this, &objects] {
        objects.readBuffer->onSave(&*mStream);
        if (objects.channelStream) objects.channelStream->save(&*mStream);
        if (objects.ringStream) objects.ringStream->save(&*mStream);
        objects.checksumCalc->save(&*mStream);
        objects.threadInfo->onSave(&*mStream);
    });
}

bool RenderThread::isPausedForSnapshotLocked() const {
    return mState != SnapshotState::Empty;
}

bool RenderThread::doSnapshotOperation(const SnapshotObjects& objects,
                                       SnapshotState state) {
    AutoLock lock(mLock);
    if (mState == state) {
        switch (state) {
            case SnapshotState::StartLoading:
                loadImpl(&lock, objects);
                return true;
            case SnapshotState::StartSaving:
                saveImpl(&lock, objects);
                return true;
            default:
                return false;
        }
    }
    return false;
}

void RenderThread::setFinished() {
    // Make sure it never happens that we wait forever for the thread to
    // save to snapshot while it was not even going to.
    AutoLock lock(mLock);
    mFinished.store(true, std::memory_order_relaxed);
    if (mState != SnapshotState::Empty) {
        mCondVar.broadcastAndUnlock(&lock);
    }
}

void RenderThread::forceStop() {
    m_stream->forceStop();
}

unsigned long int RenderThread::gettid() {    
    return m_tid;
}

RenderThread* RenderThread::create(ChannelStream *stream, emugl::Mutex *lock) {
    return new RenderThread(stream, lock);
}

intptr_t RenderThread::main() {
    if (mFinished.load(std::memory_order_relaxed)) {
        DBG("Error: fail loading a RenderThread @%p\n", this);
        return 0;
    }

    RenderThreadInfo tInfo;
    ChecksumCalculatorThreadInfo tChecksumInfo;
    ChecksumCalculator& checksumCalc = tChecksumInfo.get();
    bool needRestoreFromSnapshot = false;

    //
    // initialize decoders
    //
    tInfo.m_glDec.initGL(gles1_dispatch_get_proc_func, nullptr);
    tInfo.m_gl2Dec.initGL(gles2_dispatch_get_proc_func, nullptr);
    initRenderControlContext(&tInfo.m_rcDec);
    //syslog(LOG_DEBUG," RenderThread::1111 ");
    
    ReadBuffer readBuf(kStreamBufferSize);

    m_tid = pthread_self();
    // Framebuffer initialization is asynchronous, so we need to make sure
    // it's completely initialized before running any GL commands.
    FrameBuffer::waitUntilInitialized();

    ////syslog(LOG_DEBUG," RenderThread::3333 waitUntilInitialized end...");
    int stats_totalBytes = 0;
    uint64_t stats_progressTimeUs = 0;
    auto stats_t0 = android::base::System::get()->getHighResTimeUs() / 1000;
    bool benchmarkEnabled = getBenchmarkEnabledFromEnv();

    while (1) {
        bool packetLen_error = false;
        // Let's make sure we read enough data for at least some processing
        int packetSize;
        if (readBuf.validData() >= 8) {
            // We know that packet size is the second int32_t from the start.
            packetSize = *(const int32_t*)(readBuf.buf() + 4);
            if (!packetSize) {
                // Emulator will get live-stuck here if packet size is read to be zero;
                // crash right away so we can see these events.
                emugl::emugl_crash_reporter(
                    "Guest should never send a size-0 GL packet\n");
            }
        } else {
            // Read enough data to at least be able to get the packet size next
            // time.
            packetSize = 8;
        }

        int stat = 0;
        if (packetSize > (int)readBuf.validData()) {
            stat = readBuf.getData(m_stream, packetSize);
            if (stat <= 0) {            
                D("Warning: render thread could not read data from stream");
                    break;
            }             
        }

        DD("render thread read %d bytes, op %d, packet size %d",
           (int)readBuf.validData(), *(int32_t*)readBuf.buf(),
           *(int32_t*)(readBuf.buf() + 4));

        //
        // log received bandwidth statistics
        //
        /*
        if (benchmarkEnabled) {
            stats_totalBytes += readBuf.validData();
            auto dt = android::base::System::get()->getHighResTimeUs() / 1000 - stats_t0;
            if (dt > 1000) {
                float dts = (float)dt / 1000.0f;
                printf("Used Bandwidth %5.3f MB/s, time in progress %f ms total %f ms\n", ((float)stats_totalBytes / dts) / (1024.0f*1024.0f),
                        stats_progressTimeUs / 1000.0f,
                        (float)dt);
                readBuf.printStats();
                stats_t0 = android::base::System::get()->getHighResTimeUs() / 1000;
                stats_progressTimeUs = 0;
                stats_totalBytes = 0;
            }
        }

        //
        // dump stream to file if needed
        //
        if (dumpFP) {
            int skip = readBuf.validData() - stat;
            fwrite(readBuf.buf() + skip, 1, readBuf.validData() - skip, dumpFP);
            fflush(dumpFP);
        }

        auto progressStart = currTimeUs(benchmarkEnabled);
        */
        bool progress;
        do {
            progress = false;
            m_lock->lock();
            // try to process some of the command buffer using the GLESv1
            // decoder
            //
            // DRIVER WORKAROUND:
            // On Linux with NVIDIA GPU's at least, we need to avoid performing
            // GLES ops while someone else holds the FrameBuffer write lock.
            //
            // To be more specific, on Linux with NVIDIA Quadro K2200 v361.xx,
            // we get a segfault in the NVIDIA driver when glTexSubImage2D
            // is called at the same time as glXMake(Context)Current.
            //
            // To fix, this driver workaround avoids calling
            // any sort of GLES call when we are creating/destroying EGL
            // contexts.
            {                
                FrameBuffer::getFB()->lockContextStructureRead();
            }
            int last;

            {                
                last = tInfo.m_glDec.decode(
                        readBuf.buf(), readBuf.validData(), m_stream, &checksumCalc);
                if (last > 0) {
                    progress = true;
                    readBuf.consume(last);
                }
                else if (last < 0)
                {
                    packetLen_error = true;
                }
                
            }

            //
            // try to process some of the command buffer using the GLESv2
            // decoder
            //
            {                
                last = tInfo.m_gl2Dec.decode(readBuf.buf(), readBuf.validData(),
                                             m_stream, &checksumCalc);

                if (last > 0) {
                    progress = true;
                    readBuf.consume(last);
                }
                else if (last < 0)
                {
                    packetLen_error = true;
                }
                
            }

            FrameBuffer::getFB()->unlockContextStructureRead();
            //
            // try to process some of the command buffer using the
            // renderControl decoder
            //
            {                
                last = tInfo.m_rcDec.decode(readBuf.buf(), readBuf.validData(),
                                            m_stream, &checksumCalc);
                if (last > 0) {
                    readBuf.consume(last);
                    progress = true;
                }
                else if (last < 0)
                {
                    packetLen_error = true;
                }
                
            }
            m_lock->unlock();

            #ifdef KY_ENABLE_VULKAN
            //
            // try to process some of the command buffer using the
            // Vulkan decoder
            //
            {                
                last = tInfo.m_vkDec.decode(readBuf.buf(), readBuf.validData(),
                                            ioStream);
                if (last > 0) {
                    readBuf.consume(last);
                    progress = true;
                }
            }
            #endif
        } while (progress);
        if(packetLen_error) {
            fprintf(stderr,"ERROR: RenderThread,readBuf get packetLen_error\n");
            syslog(LOG_DEBUG,"ERROR: RenderThread,readBuf get packetLen_error");
            break;
        }
    }
    /*if (dumpFP) {
        fclose(dumpFP);
    }*/

    // Don't check for snapshots here: if we're already exiting then snapshot
    // should not contain this thread information at all.
    if (!FrameBuffer::getFB()->isShuttingDown()) {
        // Release references to the current thread's context/surfaces if any
        FrameBuffer::getFB()->bindContext(0, 0, 0);
        if (tInfo.currContext || tInfo.currDrawSurf || tInfo.currReadSurf) {
            fprintf(stderr,
                    "ERROR: RenderThread exiting with current context/surfaces\n");
        }

        FrameBuffer::getFB()->drainWindowSurface();
        FrameBuffer::getFB()->drainRenderContext();
    }
    
    setFinished();

    DBG("Exited a RenderThread @%p\n", this);
    return 0;
}

}  // namespace emugl
