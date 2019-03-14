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

#include "CachedFontClient.h"
#include "CachedImageClient.h"
#include "CachedRawResourceClient.h"
#include "CachedResourceClient.h"
#include "CachedSVGDocumentClient.h"
#include "CachedStyleSheetClient.h"
#include "ExceptionOr.h"
#include <JavaScriptCore/InspectorAuditAgent.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CachedResource;
class Document;

class InspectorAuditResourcesObject : public RefCounted<InspectorAuditResourcesObject> {
public:
    static Ref<InspectorAuditResourcesObject> create(Inspector::InspectorAuditAgent& auditAgent)
    {
        return adoptRef(*new InspectorAuditResourcesObject(auditAgent));
    }

    ~InspectorAuditResourcesObject();

    struct Resource {
        String id;
        String url;
        String mimeType;
    };

    struct ResourceContent {
        String data;
        bool base64Encoded;
    };

    ExceptionOr<Vector<Resource>> getResources(Document&);
    ExceptionOr<ResourceContent> getResourceContent(Document&, const String& id);

private:
    explicit InspectorAuditResourcesObject(Inspector::InspectorAuditAgent&);

    CachedResourceClient& clientForResource(const CachedResource&);

    Inspector::InspectorAuditAgent& m_auditAgent;

    class InspectorAuditCachedResourceClient : public CachedResourceClient { };
    InspectorAuditCachedResourceClient m_cachedResourceClient;

    class InspectorAuditCachedFontClient : public CachedFontClient { };
    InspectorAuditCachedFontClient m_cachedFontClient;

    class InspectorAuditCachedImageClient : public CachedImageClient { };
    InspectorAuditCachedImageClient m_cachedImageClient;

    class InspectorAuditCachedRawResourceClient : public CachedRawResourceClient { };
    InspectorAuditCachedRawResourceClient m_cachedRawResourceClient;

    class InspectorAuditCachedSVGDocumentClient : public CachedSVGDocumentClient { };
    InspectorAuditCachedSVGDocumentClient m_cachedSVGDocumentClient;

    class InspectorAuditCachedStyleSheetClient : public CachedStyleSheetClient { };
    InspectorAuditCachedStyleSheetClient m_cachedStyleSheetClient;

    HashMap<String, CachedResource*> m_resources;
};

} // namespace WebCore
