/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteDisplayListIdentifier.h"
#include "RemoteGradientIdentifier.h"
#include "RemotePathImplIdentifier.h"
#include "RenderingUpdateID.h"
#include <WebCore/FilterFunction.h>
#include <WebCore/Gradient.h>
#include <WebCore/NativeImage.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
class Filter;
class Font;
struct FontCustomPlatformData;
}

namespace WebKit {

class RemoteImageBufferProxy;
class RemoteNativeImageProxy;
class RemoteRenderingBackendProxy;

class RemoteResourceCacheProxy final : private WebCore::RenderingResourceObserver,  public CanMakeCheckedPtr<RemoteResourceCacheProxy> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteResourceCacheProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteResourceCacheProxy);

public:
    using WeakValueType = RemoteResourceCacheProxy;
    static UniqueRef<RemoteResourceCacheProxy> create(RemoteRenderingBackendProxy&);
    ~RemoteResourceCacheProxy();

    Ref<RemoteNativeImageProxy> createNativeImage(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha);

    [[nodiscard]] bool recordNativeImageUse(WebCore::NativeImage&, const WebCore::DestinationColorSpace&);
    RemotePathImplIdentifier recordPathImplUse(const WebCore::PathImpl&);
    void recordFontUse(WebCore::Font&);
    RemoteGradientIdentifier recordGradientUse(WebCore::Gradient&);
    void recordFilterUse(WebCore::Filter&);
    void recordFontCustomPlatformDataUse(const WebCore::FontCustomPlatformData&);
    RemoteDisplayListIdentifier recordDisplayListUse(const WebCore::DisplayList::DisplayList&);
    void didPaintLayers();

    void disconnect();
    void releaseMemory();

    void willDestroyRemoteNativeImageProxy(const RemoteNativeImageProxy&);
    WebCore::PlatformImagePtr platformImage(const RemoteNativeImageProxy&);

    // CheckedPtr interface.
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    unsigned nativeImageCountForTesting() const { return m_nativeImages.size(); }

private:
    RemoteResourceCacheProxy(RemoteRenderingBackendProxy&);

    // WebCore::RenderingResourceObserver overrides.
    void willDestroyNativeImage(const WebCore::NativeImage&) override;
    void willDestroyGradient(const WebCore::Gradient&) override;
    void willDestroyPathImpl(const WebCore::PathImpl&) override;
    void willDestroyFilter(WebCore::RenderingResourceIdentifier) override;
    void willDestroyDisplayList(const WebCore::DisplayList::DisplayList&) override;

    void finalizeRenderingUpdateForFonts();
    void prepareForNextRenderingUpdate();
    void releaseFonts();
    void releaseFontCustomPlatformDatas();

    struct NativeImageEntry {
        RefPtr<WebCore::ShareableBitmap> bitmap; // Reused across GPUP crashes, held through the associated NativeImage lifetime.
        bool existsInRemote = true;
    };
    HashMap<CheckedPtr<const WebCore::NativeImage>, NativeImageEntry> m_nativeImages;
    HashMap<CheckedPtr<const WebCore::Gradient>, RemoteGradientIdentifier> m_gradients;
    HashMap<CheckedPtr<const WebCore::PathImpl>, RemotePathImplIdentifier> m_paths;
    HashSet<WebCore::RenderingResourceIdentifier> m_filters;
    HashMap<CheckedPtr<const WebCore::DisplayList::DisplayList>, RemoteDisplayListIdentifier> m_displayLists;
    WeakPtrFactory<WebCore::RenderingResourceObserver> m_resourceObserverWeakFactory;
    WeakPtrFactory<WebCore::RenderingResourceObserver> m_nativeImageResourceObserverWeakFactory;
    WeakPtrFactory<RemoteResourceCacheProxy> m_remoteNativeImageProxyWeakFactory;

    using FontHashMap = HashMap<WebCore::RenderingResourceIdentifier, uint64_t>;
    FontHashMap m_fonts;
    FontHashMap m_fontCustomPlatformDatas;

    unsigned m_numberOfFontsUsedInCurrentRenderingUpdate { 0 };
    unsigned m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate { 0 };

    const CheckedRef<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    uint64_t m_renderingUpdateID;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
