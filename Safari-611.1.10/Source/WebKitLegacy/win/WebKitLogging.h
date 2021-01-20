/*
 * Copyright (C) 2005, 2007, 2013 Apple Inc. All rights reserved.
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
#ifndef WebKitLogging_h
#define WebKitLogging_h

#include <wtf/Assertions.h>

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WebKitLog
#endif

#define WEBKIT_LOG_CHANNELS(M) \
    M(BackForward) \
    M(Bindings) \
    M(CacheSizes) \
    M(DocumentLoad) \
    M(Download) \
    M(Encoding) \
    M(Events) \
    M(FileDatabaseActivity) \
    M(FontCache) \
    M(FontSelection) \
    M(FontSubstitution) \
    M(FormDelegate) \
    M(History) \
    M(LiveConnect) \
    M(Loading) \
    M(PageCache) \
    M(PluginEvents) \
    M(Plugins) \
    M(Progress) \
    M(Redirect) \
    M(TextInput) \
    M(Timing) \
    M(View) \

WEBKIT_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

#undef DECLARE_LOG_CHANNEL

void WebKitInitializeLogChannelsIfNecessary(void);

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

#endif
