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

#ifndef Nix_MediaConstraints_h
#define Nix_MediaConstraints_h

#include "Common.h"
#include "PrivatePtr.h"

#include <cstddef>
#include <string>
#include <vector>

namespace WebCore {
struct MediaConstraint;
class MediaConstraints;
}

namespace Nix {

struct MediaConstraint {
    MediaConstraint()
    {
    }

    MediaConstraint(std::string name, std::string value)
        : m_name(name)
        , m_value(value)
    {
    }

    std::string m_name;
    std::string m_value;
};

class MediaConstraints {
public:
    MediaConstraints() { }
    ~MediaConstraints() { reset(); }

    void assign(const MediaConstraints&);

    void reset();
    NIX_EXPORT bool isNull() const { return m_private.isNull(); }

    NIX_EXPORT void getMandatoryConstraints(std::vector<MediaConstraint>&) const;
    NIX_EXPORT void getOptionalConstraints(std::vector<MediaConstraint>&) const;

    NIX_EXPORT bool getMandatoryConstraintValue(const char* name, std::string& value) const;
    NIX_EXPORT bool getOptionalConstraintValue(const char* name, std::string& value) const;

#ifdef BUILDING_NIX__
    MediaConstraints(const WTF::PassRefPtr<WebCore::MediaConstraints>&);
    MediaConstraints(WebCore::MediaConstraints*);
#endif

private:
    PrivatePtr<WebCore::MediaConstraints> m_private;
};

} // namespace Nix

#endif // Nix_MediaConstraints_h

