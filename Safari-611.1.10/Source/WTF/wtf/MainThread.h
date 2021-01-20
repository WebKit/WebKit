/*
 * Copyright (C) 2007, 2008, 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdint.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/ThreadingPrimitives.h>

namespace WTF {

class PrintStream;
class Thread;

// Must be called from the main thread.
WTF_EXPORT_PRIVATE void initializeMainThread();

WTF_EXPORT_PRIVATE void callOnMainThread(Function<void()>&&);
WTF_EXPORT_PRIVATE void callOnMainThreadAndWait(Function<void()>&&);

#if PLATFORM(COCOA)
WTF_EXPORT_PRIVATE void dispatchAsyncOnMainThreadWithWebThreadLockIfNeeded(void (^block)());
WTF_EXPORT_PRIVATE void callOnWebThreadOrDispatchAsyncOnMainThread(void (^block)());
#endif

WTF_EXPORT_PRIVATE bool isMainThread();

WTF_EXPORT_PRIVATE bool canCurrentThreadAccessThreadLocalData(Thread&);

WTF_EXPORT_PRIVATE bool isMainRunLoop();
WTF_EXPORT_PRIVATE void callOnMainRunLoop(Function<void()>&&);
WTF_EXPORT_PRIVATE void callOnMainRunLoopAndWait(Function<void()>&&);

#if USE(WEB_THREAD)
WTF_EXPORT_PRIVATE bool isWebThread();
WTF_EXPORT_PRIVATE bool isUIThread();
WTF_EXPORT_PRIVATE void initializeWebThread();
WTF_EXPORT_PRIVATE void initializeApplicationUIThread();
#else
inline bool isWebThread() { return isMainThread(); }
inline bool isUIThread() { return isMainThread(); }
#endif // USE(WEB_THREAD)

WTF_EXPORT_PRIVATE bool isMainThreadOrGCThread();

// NOTE: these functions are internal to the callOnMainThread implementation.
void initializeMainThreadPlatform();

} // namespace WTF

using WTF::callOnMainThread;
using WTF::callOnMainThreadAndWait;
using WTF::callOnMainRunLoop;
using WTF::callOnMainRunLoopAndWait;
using WTF::canCurrentThreadAccessThreadLocalData;
using WTF::isMainThread;
using WTF::isMainThreadOrGCThread;
using WTF::isUIThread;
using WTF::isWebThread;
#if PLATFORM(COCOA)
using WTF::dispatchAsyncOnMainThreadWithWebThreadLockIfNeeded;
using WTF::callOnWebThreadOrDispatchAsyncOnMainThread;
#endif
#if USE(WEB_THREAD)
using WTF::initializeWebThread;
using WTF::initializeApplicationUIThread;
#endif
