/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef AuthenticationCF_h
#define AuthenticationCF_h

typedef struct _CFURLAuthChallenge* CFURLAuthChallengeRef;
typedef struct _CFURLCredential* CFURLCredentialRef;
typedef struct _CFURLProtectionSpace* CFURLProtectionSpaceRef;

namespace WebCore {

class AuthenticationChallenge;
class Credential;
class ProtectionSpace;

CFURLAuthChallengeRef createCF(const AuthenticationChallenge&);
CFURLCredentialRef createCF(const Credential&);
CFURLProtectionSpaceRef createCF(const ProtectionSpace&);

Credential core(CFURLCredentialRef);
ProtectionSpace core(CFURLProtectionSpaceRef);

class WebCoreCredentialStorage {
public:
    static void set(CFURLProtectionSpaceRef protectionSpace, CFURLCredentialRef credential)
    {
        if (!m_storage)
            m_storage = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(m_storage, protectionSpace, credential);
    }

    static CFURLCredentialRef get(CFURLProtectionSpaceRef protectionSpace)
    {
        if (!m_storage)
            return 0;
        return (CFURLCredentialRef)CFDictionaryGetValue(m_storage, protectionSpace);
    }

private:
    static CFMutableDictionaryRef m_storage;
};

}

#endif
