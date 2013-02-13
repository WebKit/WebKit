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

#if ENABLE_WEBRTC
#include "MockConstraints.h"

#include <public/WebMediaConstraints.h>

using namespace WebKit;

namespace WebTestRunner {

namespace {

bool isSupported(const WebString& constraint)
{
    return constraint == "valid_and_supported_1" || constraint == "valid_and_supported_2";
}

bool isValid(const WebString& constraint)
{
    return isSupported(constraint) || constraint == "valid_but_unsupported_1" || constraint == "valid_but_unsupported_2";
}

}

bool MockConstraints::verifyConstraints(const WebMediaConstraints& constraints)
{
    WebVector<WebMediaConstraint> mandatoryConstraints;
    constraints.getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const WebMediaConstraint& curr = mandatoryConstraints[i];
            if (!isSupported(curr.m_name) || curr.m_value != "1")
                return false;
        }
    }

    WebVector<WebMediaConstraint> optionalConstraints;
    constraints.getOptionalConstraints(optionalConstraints);
    if (optionalConstraints.size()) {
        for (size_t i = 0; i < optionalConstraints.size(); ++i) {
            const WebMediaConstraint& curr = optionalConstraints[i];
            if (!isValid(curr.m_name) || curr.m_value != "0")
                return false;
        }
    }

    return true;
}

}

#endif // ENABLE_WEBRTC
