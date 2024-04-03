/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "ExceptionOr.h"
#include "FetchBody.h"
#include "Supplementable.h"
#include <wtf/Forward.h>

namespace WebCore {

class CachedRawResource;
class Document;
class Navigator;
class ResourceError;

class NavigatorBeacon final : public Supplement<Navigator>, private CachedRawResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit NavigatorBeacon(Navigator&);
    ~NavigatorBeacon();
    static ExceptionOr<bool> sendBeacon(Navigator&, Document&, const String& url, std::optional<FetchBody::Init>&&);

    size_t inflightBeaconsCount() const { return m_inflightBeacons.size(); }

    WEBCORE_EXPORT static NavigatorBeacon* from(Navigator&);

private:
    ExceptionOr<bool> sendBeacon(Document&, const String& url, std::optional<FetchBody::Init>&&);

    static ASCIILiteral supplementName();

    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;
    void logError(const ResourceError&);

    Navigator& m_navigator;
    Vector<CachedResourceHandle<CachedRawResource>> m_inflightBeacons;
};

}
