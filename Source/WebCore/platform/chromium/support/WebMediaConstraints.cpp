/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include <public/WebMediaConstraints.h>

#include "MediaConstraints.h"

using namespace WebCore;

namespace WebKit {

WebMediaConstraints::WebMediaConstraints(const PassRefPtr<MediaConstraints>& constraints)
    : m_private(constraints)
{
}

WebMediaConstraints::WebMediaConstraints(MediaConstraints* constraints)
    : m_private(constraints)
{
}

void WebMediaConstraints::assign(const WebMediaConstraints& other)
{
    m_private = other.m_private;
}

void WebMediaConstraints::reset()
{
    m_private.reset();
}

void WebMediaConstraints::getMandatoryConstraintNames(WebVector<WebString>& names) const
{
    ASSERT(!isNull());
    Vector<String> constraintNames;
    m_private->getMandatoryConstraintNames(constraintNames);
    WebVector<WebString> result(constraintNames);
    names.swap(result);
}

void WebMediaConstraints::getOptionalConstraintNames(WebVector<WebString>& names) const
{
    ASSERT(!isNull());
    Vector<String> constraintNames;
    m_private->getOptionalConstraintNames(constraintNames);
    WebVector<WebString> result(constraintNames);
    names.swap(result);
}

bool WebMediaConstraints::getMandatoryConstraintValue(const WebString& name, WebString& value) const
{
    ASSERT(!isNull());
    String result;
    if (m_private->getMandatoryConstraintValue(name, result)) {
        value = result;
        return true;
    }
    return false;
}

bool WebMediaConstraints::getOptionalConstraintValue(const WebString& name, WebString& value) const
{
    ASSERT(!isNull());
    String result;
    if (m_private->getOptionalConstraintValue(name, result)) {
        value = result;
        return true;
    }
    return false;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

