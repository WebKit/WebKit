/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Copyright (C) 2016 Akamai Technologies Inc. All rights reserved.
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

#pragma once

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedFontClient.h"
#include <WebCore/CachedImage.h>
#include <WebCore/CachedImageClient.h>
#include <WebCore/CachedRawResource.h>
#include <WebCore/CachedRawResourceClient.h>
#include <WebCore/CachedResourceHandle.h>
#include "CachedScript.h"
#include <WebCore/CachedStyleSheetClient.h>
#include "CachedTextTrack.h"
#include <wtf/CheckedRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LinkLoader;

class LinkPreloadResourceClient {
public:
    virtual ~LinkPreloadResourceClient() = default;

    void triggerEvents(const CachedResource&);

    virtual void clear() = 0;

protected:
    LinkPreloadResourceClient(LinkLoader&, CachedResource&);

    void addResource(CachedResourceClient& client)
    {
        m_resource->addClient(client);
    }

    void clearResource(CachedResourceClient& client)
    {
        if (!m_resource)
            return;

        m_resource->removeClient(client);
        m_resource = nullptr;
    }

    CachedResource* ownedResource() { return m_resource.get(); }

private:
    SingleThreadWeakPtr<LinkLoader> m_loader;
    CachedResourceHandle<CachedResource> m_resource;
};

class LinkPreloadDefaultResourceClient final : public LinkPreloadResourceClient, CachedResourceClient, public CanMakeCheckedPtr<LinkPreloadDefaultResourceClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LinkPreloadDefaultResourceClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LinkPreloadDefaultResourceClient);
public:
    LinkPreloadDefaultResourceClient(LinkLoader& loader, CachedResource& resource)
        : LinkPreloadResourceClient(loader, resource)
    {
        addResource(*this);
    }

    // CachedResourceClient.
    USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
    uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

private:
    void notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final { triggerEvents(resource); }
    void clear() final { clearResource(*this); }
    bool shouldMarkAsReferenced() const final { return false; }
};

class LinkPreloadStyleResourceClient final : public LinkPreloadResourceClient, public CachedStyleSheetClient, public CanMakeCheckedPtr<LinkPreloadStyleResourceClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LinkPreloadStyleResourceClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LinkPreloadStyleResourceClient);
public:
    LinkPreloadStyleResourceClient(LinkLoader& loader, CachedCSSStyleSheet& resource)
        : LinkPreloadResourceClient(loader, resource)
    {
        addResource(*this);
    }

    // CachedStyleSheetClient.
    USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
    uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

private:
    void setCSSStyleSheet(const String&, const URL&, ASCIILiteral, const CachedCSSStyleSheet* resource) final
    {
        ASSERT(resource);
        ASSERT(ownedResource() == resource);
        triggerEvents(*resource);
    }

    void clear() final { clearResource(*this); }
    bool shouldMarkAsReferenced() const final { return false; }
};

class LinkPreloadImageResourceClient : public LinkPreloadResourceClient, public CachedImageClient, public CanMakeCheckedPtr<LinkPreloadImageResourceClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LinkPreloadImageResourceClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LinkPreloadImageResourceClient);
public:
    LinkPreloadImageResourceClient(LinkLoader& loader, CachedImage& resource)
        : LinkPreloadResourceClient(loader, resource)
    {
        addResource(*this);
    }

    // CachedImageClient.
    USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
    uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

private:
    void notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final { triggerEvents(resource); }
    void clear() final { clearResource(*this); }
    bool shouldMarkAsReferenced() const final { return false; }
};

class LinkPreloadFontResourceClient final : public LinkPreloadResourceClient, public CachedFontClient, public CanMakeCheckedPtr<LinkPreloadFontResourceClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LinkPreloadFontResourceClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LinkPreloadFontResourceClient);
public:
    LinkPreloadFontResourceClient(LinkLoader& loader, CachedFont& resource)
        : LinkPreloadResourceClient(loader, resource)
    {
        addResource(*this);
    }

    // CachedFontClient.
    USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
    uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

private:
    void fontLoaded(CachedFont& resource) final
    {
        ASSERT(ownedResource() == &resource);
        triggerEvents(resource);
    }

    void clear() final { clearResource(*this); }
    bool shouldMarkAsReferenced() const final { return false; }
};

class LinkPreloadRawResourceClient : public LinkPreloadResourceClient, public CachedRawResourceClient, public CanMakeCheckedPtr<LinkPreloadRawResourceClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LinkPreloadRawResourceClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LinkPreloadRawResourceClient);
public:
    LinkPreloadRawResourceClient(LinkLoader& loader, CachedRawResource& resource)
        : LinkPreloadResourceClient(loader, resource)
    {
        addResource(*this);
    }

    // CachedRawResourceClient.
    USING_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
    uint32_t virtualCheckedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t virtualCheckedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void virtualIncrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void virtualDecrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

private:
    void notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final { triggerEvents(resource); }
    void clear() final { clearResource(*this); }
    bool shouldMarkAsReferenced() const final { return false; }
};

}
