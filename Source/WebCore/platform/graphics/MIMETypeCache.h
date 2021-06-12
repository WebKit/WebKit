/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "MediaPlayerEnums.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class ContentType;

class WEBCORE_EXPORT MIMETypeCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MIMETypeCache() = default;
    virtual ~MIMETypeCache() = default;

    virtual bool isAvailable() const;
    virtual MediaPlayerEnums::SupportsType canDecodeType(const String&);
    virtual HashSet<String, ASCIICaseInsensitiveHash>& supportedTypes();

    bool isEmpty() const;
    bool supportsContainerType(const String&);

protected:
    void addSupportedTypes(const Vector<String>&);

private:
    virtual bool isStaticContainerType(StringView);
    virtual bool isUnsupportedContainerType(const String&);
    virtual void initializeCache(HashSet<String, ASCIICaseInsensitiveHash>&);
    virtual bool canDecodeExtendedType(const ContentType&);

    bool shouldOverrideExtendedType(const ContentType&);

    std::optional<HashSet<String, ASCIICaseInsensitiveHash>> m_supportedTypes;
    std::optional<HashMap<String, MediaPlayerEnums::SupportsType, ASCIICaseInsensitiveHash>> m_cachedResults;
};

} // namespace WebCore
