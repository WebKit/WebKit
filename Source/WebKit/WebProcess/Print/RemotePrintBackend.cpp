/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemotePrintBackend.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Logging.h"
#include "PrintConnectionToWebProcess.h"
#include "RemoteDisplayListRecorder.h"
#include "RemotePrintBackendMessages.h"
#include "RemoteSnapshot.h"
#include "RemoteSnapshotRecorder.h"
#include "RemoteSnapshotRegistry.h"
#include "SharedFont.h"
#include "SharedVideoFrame.h"
#include "WebProcess.h"
#include <WebCore/ControlFactory.h>
#include <WebCore/Filter.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/FontPlatformData.h>
#include <WebCore/Gradient.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferAllocator.h>
#include <WebCore/NativeImage.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, m_streamConnection, message)
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_streamConnection, completion)

namespace WebKit {
using namespace WebCore;

namespace {

// The print process has no IOSurfaces, so filters draw into unaccelerated buffers whatever they ask for.
class UnacceleratedImageBufferAllocator final : public ImageBufferAllocator {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(UnacceleratedImageBufferAllocator);
public:
    RefPtr<ImageBuffer> createImageBuffer(const FloatSize& size, const ColorSpace& colorSpace, RenderingMode) const final
    {
        return ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
    }
};

} // namespace

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePrintBackend);

Ref<RemotePrintBackend> RemotePrintBackend::create(PrintConnectionToWebProcess& connection, RemoteRenderingBackendIdentifier identifier, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    Ref instance = adoptRef(*new RemotePrintBackend(connection, identifier, WTF::move(streamConnection)));
    instance->startListeningForIPC();
    return instance;
}

RemotePrintBackend::RemotePrintBackend(PrintConnectionToWebProcess& connection, RemoteRenderingBackendIdentifier identifier, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("RemotePrintBackend work queue"_s))
    , m_streamConnection(WTF::move(streamConnection))
    , m_identifier(identifier)
    , m_webProcessIdentifier(connection.webProcessIdentifier())
    , m_sharedPreferencesForWebProcess(connection.sharedPreferencesForWebProcessValue())
    , m_isLockdownModeEnabled(connection.isLockdownModeEnabled())
    , m_snapshotRegistry(WebProcess::singleton().snapshotRegistry())
{
    ASSERT(RunLoop::isMain());
}

RemotePrintBackend::~RemotePrintBackend() = default;

void RemotePrintBackend::startListeningForIPC()
{
    m_workQueue->dispatch([protectedThis = Ref { *this }] {
        protectedThis->workQueueInitialize();
    });
}

void RemotePrintBackend::stopListeningForIPC()
{
    m_workQueue->stopAndWaitForCompletion([protectedThis = Ref { *this }] {
        protectedThis->workQueueUninitialize();
    });
}

void RemotePrintBackend::workQueueInitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->open(*this, m_workQueue.get());
    m_streamConnection->startReceivingMessages(*this, Messages::RemotePrintBackend::messageReceiverName(), m_identifier.toUInt64());
}

void RemotePrintBackend::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    m_remoteDisplayListRecorders.clear();
    m_remoteSnapshotRecorders.clear();
    // Released on the work queue, since it is populated there.
    m_remoteResourceCache.releaseAllResources();

    Ref streamConnection = m_streamConnection;
    streamConnection->stopReceivingMessages(Messages::RemotePrintBackend::messageReceiverName(), m_identifier.toUInt64());
    streamConnection->invalidate();
}

void RemotePrintBackend::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, "Received an invalid message %s from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName), m_webProcessIdentifier.toUInt64());
    callOnMainRunLoop([webProcessIdentifier = m_webProcessIdentifier, messageName] {
        PrintConnectionToWebProcess::terminateWebProcess(webProcessIdentifier, messageName);
    });
}

std::unique_ptr<ImageBufferAllocator> RemotePrintBackend::createFilterImageBufferAllocator() const
{
    return makeUnique<UnacceleratedImageBufferAllocator>();
}

Ref<ControlFactory> RemotePrintBackend::createControlFactory() const
{
    // The controls are drawn on the work queue, so without NSViews: see WebProcess::prepareForPrinting().
    return ControlFactory::create();
}

#if ENABLE(VIDEO)
std::unique_ptr<SharedVideoFrameReader> RemotePrintBackend::createSharedVideoFrameReader() const
{
    // There are no video frames shared with the print process, so they all come through shared memory,
    // and are read without IOSurfaces.
    return makeUnique<SharedVideoFrameReader>();
}
#endif

void RemotePrintBackend::createSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier rootFrameIdentifier, const FloatSize& size, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_snapshotRegistry->createSnapshot(snapshotIdentifier, rootFrameIdentifier, size, m_webProcessIdentifier);
    completionHandler();
}

void RemotePrintBackend::createSnapshotRecorder(RemoteSnapshotRecorderIdentifier identifier, RemoteSnapshotIdentifier snapshotIdentifier)
{
    assertIsCurrent(workQueue());
    Ref snapshot = m_snapshotRegistry->snapshotForRecorder(snapshotIdentifier);
    auto result = m_remoteSnapshotRecorders.add(identifier, RemoteSnapshotRecorder::create(identifier, snapshot, *this));
    MESSAGE_CHECK(result.isNewEntry, "Recorder already created");
}

void RemotePrintBackend::sinkSnapshotRecorderIntoSnapshotFrame(RemoteSnapshotRecorderIdentifier identifier, FrameIdentifier frameIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto recorder = m_remoteSnapshotRecorders.take(identifier);
    MESSAGE_CHECK_COMPLETION(recorder.get(), completionHandler(false));
    Ref snapshot = recorder->snapshot();
    // FIXME: using global identifiers (frameIdentifier) is not secure. Do not follow this pattern.
    snapshot->setFrame(frameIdentifier, recorder->takeDisplayList(), workQueue());
    completionHandler(true);
}

void RemotePrintBackend::createDisplayListRecorder(RemoteDisplayListRecorderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto result = m_remoteDisplayListRecorders.add(identifier, RemoteDisplayListRecorder::create(identifier, *this));
    MESSAGE_CHECK(result.isNewEntry, "Recorder already created");
}

void RemotePrintBackend::sinkDisplayListRecorderIntoDisplayList(RemoteDisplayListRecorderIdentifier identifier, RemoteDisplayListIdentifier displayListIdentifier)
{
    assertIsCurrent(workQueue());
    auto recorder = m_remoteDisplayListRecorders.take(identifier);
    MESSAGE_CHECK(recorder.get(), "Recorder sunk into display list before being created");
    Ref displayList = recorder->takeDisplayList();
    bool success = m_remoteResourceCache.cacheDisplayList(displayListIdentifier, WTF::move(displayList));
    MESSAGE_CHECK(success, "Display list already created");
}

void RemotePrintBackend::releaseDisplayList(RemoteDisplayListIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseDisplayList(identifier);
    MESSAGE_CHECK(success, "Display list released before being cached");
}

void RemotePrintBackend::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier imageIdentifier)
{
    assertIsCurrent(workQueue());
    auto bitmap = ShareableBitmap::create(WTF::move(handle));
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes));
    if (!image)
        return;

    bool success = m_remoteResourceCache.cacheNativeImage(imageIdentifier, image.releaseNonNull());
    MESSAGE_CHECK(success, "NativeImage already cached.");
}

void RemotePrintBackend::releaseNativeImage(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseNativeImage(identifier);
    MESSAGE_CHECK(success, "NativeImage released before being cached.");
}

void RemotePrintBackend::cachePathImpl(Ref<PathImpl>&& path, RemotePathImplIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.cachePathImpl(identifier, WTF::move(path));
    MESSAGE_CHECK(success, "Path already cached.");
}

void RemotePrintBackend::releasePathImpl(RemotePathImplIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releasePathImpl(identifier);
    MESSAGE_CHECK(success, "Path released before being cached.");
}

void RemotePrintBackend::cacheFont(const FontInternalAttributes& fontAttributes, FontPlatformDataAttributes platformData, std::optional<RenderingResourceIdentifier> fontCustomPlatformDataIdentifier)
{
    assertIsCurrent(workQueue());

    RefPtr<FontCustomPlatformData> customPlatformData;
    if (fontCustomPlatformDataIdentifier) {
        customPlatformData = m_remoteResourceCache.cachedFontCustomPlatformData(*fontCustomPlatformDataIdentifier);
        MESSAGE_CHECK(customPlatformData, "CacheFont without caching custom data");
    }

    FontPlatformData platform = FontPlatformData::create(platformData, customPlatformData.get());

    Ref font = SharedFont::create(platform, fontAttributes.origin, fontAttributes.isInterstitial, fontAttributes.visibility, fontAttributes.isTextOrientationFallback, fontAttributes.renderingResourceIdentifier);

    bool success = m_remoteResourceCache.cacheFont(WTF::move(font));
    MESSAGE_CHECK(success, "Font already cached.");
}

void RemotePrintBackend::releaseFont(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFont(identifier);
    MESSAGE_CHECK(success, "Font released before being cached.");
}

void RemotePrintBackend::cacheFontCustomPlatformData(FontCustomPlatformSerializedData&& fontCustomPlatformSerializedData)
{
    assertIsCurrent(workQueue());

    auto customPlatformData = FontCustomPlatformData::tryMakeFromSerializationData(WTF::move(fontCustomPlatformSerializedData), shouldUseLockdownFontParser());
    MESSAGE_CHECK(customPlatformData.has_value(), "cacheFontCustomPlatformData couldn't deserialize FontCustomPlatformData");

    bool success = m_remoteResourceCache.cacheFontCustomPlatformData(WTF::move(customPlatformData.value()));
    MESSAGE_CHECK(success, "FontCustomPlatformData already cached.");
}

void RemotePrintBackend::releaseFontCustomPlatformData(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFontCustomPlatformData(identifier);
    MESSAGE_CHECK(success, "FontCustomPlatformData released before being cached.");
}

void RemotePrintBackend::cacheGradient(Ref<Gradient>&& gradient, RemoteGradientIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.cacheGradient(identifier, WTF::move(gradient));
    MESSAGE_CHECK(success, "Gradient already cached.");
}

void RemotePrintBackend::releaseGradient(RemoteGradientIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseGradient(identifier);
    MESSAGE_CHECK(success, "Gradient released before being cached.");
}

void RemotePrintBackend::cacheFilter(Ref<Filter>&& filter)
{
    assertIsCurrent(workQueue());
    MESSAGE_CHECK(filter->hasValidRenderingResourceIdentifier(), "Received a Filter without a valid resource identifier.");
    bool success = m_remoteResourceCache.cacheFilter(WTF::move(filter));
    MESSAGE_CHECK(success, "Filter already cached.");
}

void RemotePrintBackend::releaseFilter(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFilter(identifier);
    MESSAGE_CHECK(success, "Filter released before being cached.");
}

void RemotePrintBackend::releaseMemory()
{
    assertIsCurrent(workQueue());
    m_remoteResourceCache.releaseMemory();
}

// As RemoteRenderingBackend::shouldUseLockdownFontParser() does, from the web process's preferences
// and Lockdown Mode.
bool RemotePrintBackend::shouldUseLockdownFontParser() const
{
#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
    return (m_sharedPreferencesForWebProcess.lockdownFontParserEnabled && m_isLockdownModeEnabled) || m_sharedPreferencesForWebProcess.forceLockdownFontParserEnabled;
#else
    return false;
#endif
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
