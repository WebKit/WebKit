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

#include "config.h"
#include "ResourceError.h"

#if USE(CFNETWORK)

// FIXME: Once <rdar://problem/5050881> is fixed we can remove this extern "C"

extern "C" {
#include <CFNetwork/CFNetworkErrors.h>
}

#include <CoreFoundation/CFError.h>

namespace WebCore {

// FIXME: Once <rdar://problem/5050841> is fixed we can remove this constructor.
ResourceError::ResourceError(CFStreamError error)
    : m_errorCode(error.error)
    , m_isNull(false)
{
    switch(error.domain) {
    case kCFStreamErrorDomainCustom:
        m_domain ="NSCustomErrorDomain";
        break;
    case kCFStreamErrorDomainPOSIX:
        m_domain = "NSPOSIXErrorDomain";
        break;
    case kCFStreamErrorDomainMacOSStatus:
        m_domain = "NSOSStatusErrorDomain";
        break;
    }
}

ResourceError::ResourceError(CFErrorRef error)
    : m_errorCode(0)
    , m_isNull(!error)
{
    if (!error)
        return;

    m_errorCode = CFErrorGetCode(error);
    CFStringRef domain = CFErrorGetDomain(error);

    if (domain == kCFErrorDomainMach || domain == kCFErrorDomainCocoa)
        m_domain ="NSCustomErrorDomain";
    else if (domain == kCFErrorDomainCFNetwork)
        m_domain = "CFURLErrorDomain";
    else if (domain == kCFErrorDomainPOSIX)
        m_domain = "NSPOSIXErrorDomain";
    else if (domain == kCFErrorDomainOSStatus)
        m_domain = "NSOSStatusErrorDomain";
}

ResourceError::operator CFStreamError() const
{
    CFStreamError result;
    result.error = m_errorCode;

    if (m_domain == "NSCustomErrorDomain")
        result.domain = kCFStreamErrorDomainCustom;
    else if (m_domain == "NSPOSIXErrorDomain")
        result.domain = kCFStreamErrorDomainPOSIX;
    else if (m_domain == "NSOSStatusErrorDomain")
        result.domain = kCFStreamErrorDomainMacOSStatus;
    else
        ASSERT_NOT_REACHED();

    return result;
}

} // namespace WebCore

#endif // USE(CFNETWORK)
