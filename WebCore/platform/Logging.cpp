/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Logging.h"

namespace WebCore {

WTFLogChannel LogNotYetImplemented = { 1 << 0, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogFrames =            { 1 << 1, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogLoading =           { 1 << 2, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogPopupBlocking =     { 1 << 3, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogEvents =            { 1 << 4, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogEditing =           { 1 << 5, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogTextConversion =    { 1 << 6, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogIconDatabase =      { 1 << 7, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogSQLDatabase =       { 1 << 8, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogSpellingAndGrammar ={ 1 << 9, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogBackForward =       { 1 << 10, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogHistory =           { 1 << 11, "WebCoreLogLevel", WTFLogChannelOff };
WTFLogChannel LogPageCache =         { 1 << 12, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogNetwork =           { 1 << 13, "WebCoreLogLevel", WTFLogChannelOff };

WTFLogChannel LogResources =         { 1 << 14, "WebCoreLogLevel", WTFLogChannelOff };


}
