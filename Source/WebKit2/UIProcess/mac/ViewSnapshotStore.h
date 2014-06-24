/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef ViewSnapshotStore_h
#define ViewSnapshotStore_h

#include <WebCore/Color.h>
#include <WebCore/IntSize.h>
#include <WebCore/IOSurface.h>
#include <chrono>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS CAContext;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
#define USE_JPEG_VIEW_SNAPSHOTS false
#define USE_IOSURFACE_VIEW_SNAPSHOTS true
#define USE_RENDER_SERVER_VIEW_SNAPSHOTS false
#elif PLATFORM(MAC)
// Mountain Lion and before do not support IOSurface purgeability.
#define USE_JPEG_VIEW_SNAPSHOTS true
#define USE_IOSURFACE_VIEW_SNAPSHOTS false
#define USE_RENDER_SERVER_VIEW_SNAPSHOTS false
#else
#define USE_JPEG_VIEW_SNAPSHOTS false
#define USE_IOSURFACE_VIEW_SNAPSHOTS false
#define USE_RENDER_SERVER_VIEW_SNAPSHOTS true
#endif

namespace WebKit {

class WebBackForwardListItem;
class WebPageProxy;

struct ViewSnapshot {
#if USE_JPEG_VIEW_SNAPSHOTS || USE_IOSURFACE_VIEW_SNAPSHOTS
    RefPtr<WebCore::IOSurface> surface;
    RetainPtr<CGImageRef> image;
#endif
#if USE_RENDER_SERVER_VIEW_SNAPSHOTS
    uint32_t slotID = 0;
#endif

    std::chrono::steady_clock::time_point creationTime;
    uint64_t renderTreeSize;
    float deviceScaleFactor;
    WebCore::IntSize size;
    size_t imageSizeInBytes = 0;
    WebCore::Color backgroundColor;

    void clearImage();
    bool hasImage() const;
    id asLayerContents();
};

class ViewSnapshotStore {
    WTF_MAKE_NONCOPYABLE(ViewSnapshotStore);
public:
    ViewSnapshotStore();
    ~ViewSnapshotStore();

    static ViewSnapshotStore& shared();

    void recordSnapshot(WebPageProxy&);
    bool getSnapshot(WebBackForwardListItem*, ViewSnapshot&);

    void disableSnapshotting() { m_enabled = false; }
    void enableSnapshotting() { m_enabled = true; }

    void discardSnapshots();

#if PLATFORM(IOS)
    static CAContext *snapshottingContext();
#endif

private:
    void pruneSnapshots(WebPageProxy&);
    void removeSnapshotImage(ViewSnapshot&);
    void reduceSnapshotMemoryCost(const String& uuid);

#if USE_JPEG_VIEW_SNAPSHOTS
    void didCompressSnapshot(const String& uuid, RetainPtr<CGImageRef> newImage, size_t newImageSize);
    dispatch_queue_t m_compressionQueue;
#endif

    HashMap<String, ViewSnapshot> m_snapshotMap;

    bool m_enabled;
    size_t m_snapshotCacheSize;
};

} // namespace WebKit

#endif // ViewSnapshotStore_h
