/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef SecurityOriginHash_h
#define SecurityOriginHash_h

#include "SecurityOrigin.h"
#include <wtf/RefPtr.h>

namespace WebCore {

struct SecurityOriginHash {
    static unsigned hash(RefPtr<SecurityOrigin> origin)
    {
        unsigned hashCodes[3] = {
            origin->protocol().impl() ? origin->protocol().impl()->hash() : 0,
            origin->host().impl() ? origin->host().impl()->hash() : 0,
            origin->port()
        };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 3 * sizeof(unsigned) / sizeof(UChar));
    }
         
    static bool equal(RefPtr<SecurityOrigin> a, RefPtr<SecurityOrigin> b)
    {
        if (a == 0 || b == 0)
            return a == b;
        return a->equal(b.get());
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};


struct SecurityOriginTraits : WTF::GenericHashTraits<RefPtr<SecurityOrigin> > {
    static const bool emptyValueIsZero = true;
    static const RefPtr<SecurityOrigin>& deletedValue() 
    { 
        // Okay deleted value because file: protocols should always have port 0
        static const RefPtr<SecurityOrigin> securityOriginDeletedValue = SecurityOrigin::create("file", "", 1, 0);    
        return securityOriginDeletedValue; 
    }

    static SecurityOrigin* emptyValue() 
    { 
        return 0;
    }
};

} // namespace WebCore

#endif
