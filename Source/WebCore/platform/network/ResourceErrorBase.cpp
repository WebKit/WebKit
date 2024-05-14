/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "LocalizedStrings.h"
#include "Logging.h"

namespace WebCore {

const ASCIILiteral errorDomainWebKitInternal = "WebKitInternal"_s;
const ASCIILiteral errorDomainWebKitServiceWorker = "WebKitServiceWorker"_s;

inline const ResourceError& ResourceErrorBase::asResourceError() const
{
    return *static_cast<const ResourceError*>(this);
}

ResourceError ResourceErrorBase::isolatedCopy() const
{
    lazyInit();

    ResourceError errorCopy;
    errorCopy.m_domain = m_domain.isolatedCopy();
    errorCopy.m_errorCode = m_errorCode;
    errorCopy.m_failingURL = m_failingURL.isolatedCopy();
    errorCopy.m_localizedDescription = m_localizedDescription.isolatedCopy();
    errorCopy.m_type = m_type;

    errorCopy.doPlatformIsolatedCopy(asResourceError());

    return errorCopy;
}

void ResourceErrorBase::lazyInit() const
{
    const_cast<ResourceError*>(static_cast<const ResourceError*>(this))->platformLazyInit();
}

void ResourceErrorBase::setType(Type type)
{
    // setType should only be used to specialize the error type.
    ASSERT(m_type == type || m_type == Type::General || m_type == Type::Null || (m_type == Type::Cancellation && type == Type::AccessControl));
    m_type = type;
}

bool ResourceErrorBase::compare(const ResourceError& a, const ResourceError& b)
{
    if (a.isNull() && b.isNull())
        return true;

    if (a.type() != b.type())
        return false;

    if (a.domain() != b.domain())
        return false;

    if (a.errorCode() != b.errorCode())
        return false;

    if (a.failingURL() != b.failingURL())
        return false;

    if (a.localizedDescription() != b.localizedDescription())
        return false;

    return ResourceError::platformCompare(a, b);
}

ResourceError createInternalError(const URL& url, ASCIILiteral filename, uint32_t line, ASCIILiteral functionName)
{
    // Always print internal errors to stderr so we have some chance to figure out what went wrong
    // when an internal error occurs unexpectedly. Release logging is insufficient because internal
    // errors occur unexpectedly and we don't want to require manual logging configuration in order
    // to record them.
    WTFReportError(filename.characters(), line, functionName.characters(), "WebKit encountered an internal error. This is a WebKit bug.");

    return ResourceError("WebKitErrorDomain"_s, 300, url, WEB_UI_STRING("WebKit encountered an internal error", "WebKitErrorInternal description"));
}

ResourceError badResponseHeadersError(const URL& url)
{
    return { errorDomainWebKitInternal, 0, url, "Response contained invalid HTTP headers"_s, ResourceError::Type::General };
}

} // namespace WebCore
