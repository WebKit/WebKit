/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef RuntimeApplicationChecks_h
#define RuntimeApplicationChecks_h

#include <wtf/Forward.h>

namespace WebCore {

WEBCORE_EXPORT bool applicationIsAOLInstantMessenger();
WEBCORE_EXPORT bool applicationIsAdobeInstaller();
WEBCORE_EXPORT bool applicationIsAperture();
WEBCORE_EXPORT bool applicationIsAppleMail();
WEBCORE_EXPORT bool applicationIsIBooks();
WEBCORE_EXPORT bool applicationIsITunes();
WEBCORE_EXPORT bool applicationIsMicrosoftMessenger();
WEBCORE_EXPORT bool applicationIsMicrosoftMyDay();
WEBCORE_EXPORT bool applicationIsMicrosoftOutlook();
bool applicationIsQuickenEssentials();
WEBCORE_EXPORT bool applicationIsSafari();
bool applicationIsSolidStateNetworksDownloader();
WEBCORE_EXPORT bool applicationIsVersions();
WEBCORE_EXPORT bool applicationIsHRBlock();
WEBCORE_EXPORT bool applicationIsHipChat();

WEBCORE_EXPORT void setApplicationBundleIdentifier(const String&);

} // namespace WebCore

#endif // RuntimeApplicationChecks_h
