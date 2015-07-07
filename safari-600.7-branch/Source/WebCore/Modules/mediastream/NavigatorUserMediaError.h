/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigatorUserMediaError_h
#define NavigatorUserMediaError_h

#include "DOMError.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

class NavigatorUserMediaError : public DOMError {
public:
    static PassRefPtr<NavigatorUserMediaError> create(const String& name, const String& constraintName)
    {
        return adoptRef(new NavigatorUserMediaError(name, constraintName));
    }

    virtual ~NavigatorUserMediaError() { }

    const String& constraintName() const { return m_constraintName; }

    static const AtomicString& permissionDeniedErrorName();
    static const AtomicString& constraintNotSatisfiedErrorName();

private:
    NavigatorUserMediaError(const String& name, const String& constraintName)
        : DOMError(name)
        , m_constraintName(constraintName)
    {
    }

    String m_constraintName;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // NavigatorUserMediaError_h
