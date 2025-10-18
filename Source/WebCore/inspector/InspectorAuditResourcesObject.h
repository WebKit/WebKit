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
#include <JavaScriptCore/InspectorAuditAgent.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RobinHoodHashMap.h>

namespace WebCore {

class CachedResource;
class Document;
template<typename> class ExceptionOr;

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
    CheckedRef<CachedResourceClient> checkedClientForResource(const CachedResource& resource) { return clientForResource(resource); }

    Inspector::InspectorAuditAgent& m_auditAgent;

    class InspectorAuditCachedResourceClient final : public CachedResourceClient, public CanMakeCheckedPtr<InspectorAuditCachedResourceClient> {
        WTF_MAKE_TZONE_ALLOCATED(InspectorAuditCachedResourceClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedResourceClient);
    public:
        // CachedResourceClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedResourceClient m_cachedResourceClient;

    class InspectorAuditCachedFontClient final : public CachedFontClient, public CanMakeCheckedPtr<InspectorAuditCachedResourceClient> {
        WTF_MAKE_TZONE_ALLOCATED(InspectorAuditCachedFontClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedFontClient);
    public:
        // CachedFontClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedFontClient m_cachedFontClient;

    class InspectorAuditCachedImageClient final : public CachedImageClient, public CanMakeCheckedPtr<InspectorAuditCachedImageClient> {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(InspectorAuditCachedImageClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedImageClient);
    public:
        // CachedImageClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedImageClient m_cachedImageClient;

    class InspectorAuditCachedRawResourceClient final : public CachedRawResourceClient, public CanMakeCheckedPtr<InspectorAuditCachedRawResourceClient> {
        WTF_MAKE_TZONE_ALLOCATED(InspectorAuditCachedRawResourceClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedRawResourceClient);
    public:
        // CachedRawResourceClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedRawResourceClient m_cachedRawResourceClient;

    class InspectorAuditCachedSVGDocumentClient final : public CachedSVGDocumentClient, public CanMakeCheckedPtr<InspectorAuditCachedRawResourceClient> {
        WTF_MAKE_TZONE_ALLOCATED(InspectorAuditCachedSVGDocumentClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedSVGDocumentClient);
    public:
        // CachedSVGDocumentClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedSVGDocumentClient m_cachedSVGDocumentClient;

    class InspectorAuditCachedStyleSheetClient final : public CachedStyleSheetClient, public CanMakeCheckedPtr<InspectorAuditCachedRawResourceClient> {
        WTF_MAKE_TZONE_ALLOCATED(InspectorAuditCachedStyleSheetClient);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorAuditCachedStyleSheetClient);
    public:
        // CachedStyleSheetClient.
        USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
        uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    };
    InspectorAuditCachedStyleSheetClient m_cachedStyleSheetClient;

    MemoryCompactRobinHoodHashMap<String, CachedResource*> m_resources;
};

} // namespace WebCore
