/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformBridge_h
#define PlatformBridge_h

#include "KURL.h"
#include "PlatformString.h"

#include <wtf/Vector.h>

namespace WebCore {

// An interface to the embedding layer, which has the ability to answer
// questions about the system and so on...
// This is very similar to ChromiumBridge and the two are likely to converge
// in the future.
//
// The methods in this class all need to reach across a JNI layer to the Java VM
// where the embedder runs. The JNI machinery is currently all in WebKit/android
// but the long term plan is to move to the WebKit API and share the bridge and its
// implementation with Chromium. The JNI machinery will then move outside of WebKit,
// similarly to how Chromium's IPC layer lives outside of WebKit.
class PlatformBridge {
public:
    // KeyGenerator
    static WTF::Vector<String> getSupportedKeyStrengthList();
    static String getSignedPublicKeyAndChallengeString(unsigned index, const String& challenge, const KURL&);
};
}
#endif // PlatformBridge_h
