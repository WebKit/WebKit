/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaConstraintsMock.h"

#include "MediaConstraints.h"

namespace WebCore {

bool isSupported(const String& constraint)
{
    return notFound != constraint.find("_and_supported_");
}

bool isValid(const String& constraint)
{
    return isSupported(constraint) || notFound != constraint.find("valid_");
}

String MediaConstraintsMock::verifyConstraints(PassRefPtr<MediaConstraints> prpConstraints)
{
    RefPtr<MediaConstraints> constraints = prpConstraints;

    Vector<MediaConstraint> mandatoryConstraints;
    constraints->getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const MediaConstraint& curr = mandatoryConstraints[i];
            if (!isSupported(curr.m_name) || curr.m_value != "1")
                return curr.m_name;
        }
    }

    Vector<MediaConstraint> optionalConstraints;
    constraints->getOptionalConstraints(optionalConstraints);
    if (optionalConstraints.size()) {
        for (size_t i = 0; i < optionalConstraints.size(); ++i) {
            const MediaConstraint& curr = optionalConstraints[i];
            if (!isValid(curr.m_name) || curr.m_value != "0")
                return curr.m_name;
        }
    }

    return emptyString();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
