/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_ELEMENT)

#include "CachedRawResource.h"
#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "HTMLElement.h"
#include "IDLTypes.h"
#include "SharedBuffer.h"
#include <wtf/UniqueRef.h>

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
#include "PlatformLayer.h"
OBJC_CLASS ASVInlinePreview;
#endif

namespace WebCore {

class Model;

template<typename IDLType> class DOMPromiseProxyWithResolveCallback;

class HTMLModelElement final : public HTMLElement, private CachedRawResourceClient {
    WTF_MAKE_ISO_ALLOCATED(HTMLModelElement);
public:
    static Ref<HTMLModelElement> create(const QualifiedName&, Document&);
    virtual ~HTMLModelElement();

    void sourcesChanged();
    const URL& currentSrc() const { return m_sourceURL; }

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<HTMLModelElement>>;
    ReadyPromise& ready() { return m_readyPromise.get(); }

    RefPtr<SharedBuffer> modelData() const;
    RefPtr<Model> model() const;

#if HAVE(ARKIT_INLINE_PREVIEW)
    WEBCORE_EXPORT static void setModelElementCacheDirectory(const String&);
    WEBCORE_EXPORT static const String& modelElementCacheDirectory();
#endif

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    PlatformLayer* platformLayer() const;
    WEBCORE_EXPORT void inlinePreviewDidObtainContextId(const String& uuid, uint32_t contextId);
#endif

    void enterFullscreen();

private:
    HTMLModelElement(const QualifiedName&, Document&);

    void setSourceURL(const URL&);
    HTMLModelElement& readyPromiseResolve();

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    void clearFile();
    void createFile();
    void modelDidChange();
#endif

    // DOM overrides.
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    // Rendering overrides.
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    // CachedRawResourceClient overrides.
    void dataReceived(CachedResource&, const uint8_t* data, int dataLength) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;

    URL m_sourceURL;
    CachedResourceHandle<CachedRawResource> m_resource;
    RefPtr<SharedBuffer> m_data;
    RefPtr<Model> m_model;
    UniqueRef<ReadyPromise> m_readyPromise;
    bool m_dataComplete { false };

#if HAVE(ARKIT_INLINE_PREVIEW_MAC)
    String m_filePath;
    RetainPtr<ASVInlinePreview> m_inlinePreview;
#endif
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
