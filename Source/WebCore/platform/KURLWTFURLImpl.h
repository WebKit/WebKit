/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef KURLWTFURLImpl_h
#define KURLWTFURLImpl_h

#if USE(WTFURL)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#include <wtf/url/api/ParsedURL.h>

namespace WebCore {

class KURLWTFURLImpl : public RefCounted<KURLWTFURLImpl> {
public:
    WTF::ParsedURL m_parsedURL;
    String m_invalidUrlString;

    PassRefPtr<KURLWTFURLImpl> copy() const;
};

inline PassRefPtr<KURLWTFURLImpl> KURLWTFURLImpl::copy() const
{
    RefPtr<KURLWTFURLImpl> clone = adoptRef(new KURLWTFURLImpl);
    clone->m_parsedURL = m_parsedURL.isolatedCopy();
    clone->m_invalidUrlString = m_invalidUrlString.isolatedCopy();
    return clone.release();
}

} // namespace WebCore

#endif // USE(WTFURL)

#endif // KURLWTFURLImpl_h
