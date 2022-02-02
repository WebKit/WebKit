/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ARKitInlinePreviewModelPlayerMac.h"

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)

#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/Model.h>
#import <pal/spi/mac/SystemPreviewSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/UUID.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVInlinePreview);

namespace WebKit {

Ref<ARKitInlinePreviewModelPlayerMac> ARKitInlinePreviewModelPlayerMac::create(WebPage& page, WebCore::ModelPlayerClient& client)
{
    return adoptRef(*new ARKitInlinePreviewModelPlayerMac(page, client));
}

ARKitInlinePreviewModelPlayerMac::ARKitInlinePreviewModelPlayerMac(WebPage& page, WebCore::ModelPlayerClient& client)
    : ARKitInlinePreviewModelPlayer(page, client)
{
}

ARKitInlinePreviewModelPlayerMac::~ARKitInlinePreviewModelPlayerMac()
{
    clearFile();
}

std::optional<ModelIdentifier> ARKitInlinePreviewModelPlayerMac::modelIdentifier()
{
    if (m_inlinePreview)
        return { { [m_inlinePreview uuid].UUIDString } };
    return { };
}

static String& sharedModelElementCacheDirectory()
{
    static NeverDestroyed<String> sharedModelElementCacheDirectory;
    return sharedModelElementCacheDirectory;
}

static const String& modelElementCacheDirectory()
{
    return sharedModelElementCacheDirectory();
}

void ARKitInlinePreviewModelPlayerMac::setModelElementCacheDirectory(const String& path)
{
    sharedModelElementCacheDirectory() = path;
}

void ARKitInlinePreviewModelPlayerMac::createFile(WebCore::Model& modelSource)
{
    // The need for a file is only temporary due to the nature of ASVInlinePreview,
    // https://bugs.webkit.org/show_bug.cgi?id=227567.

    clearFile();

    auto pathToDirectory = modelElementCacheDirectory();
    if (pathToDirectory.isEmpty())
        return;

    auto directoryExists = FileSystem::fileExists(pathToDirectory);
    if (directoryExists && FileSystem::fileTypeFollowingSymlinks(pathToDirectory) != FileSystem::FileType::Directory) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!directoryExists && !FileSystem::makeAllDirectories(pathToDirectory)) {
        ASSERT_NOT_REACHED();
        return;
    }

    // We need to support .reality files as well, https://bugs.webkit.org/show_bug.cgi?id=227568.
    auto fileName = FileSystem::encodeForFileName(createVersion4UUIDString()) + ".usdz";
    auto filePath = FileSystem::pathByAppendingComponent(pathToDirectory, fileName);
    auto file = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Write);
    if (file <= 0)
        return;

    FileSystem::writeToFile(file, modelSource.data()->makeContiguous()->data(), modelSource.data()->size());
    FileSystem::closeFile(file);
    m_filePath = filePath;
}

void ARKitInlinePreviewModelPlayerMac::clearFile()
{
    if (m_filePath.isEmpty())
        return;

    FileSystem::deleteFile(m_filePath);
    m_filePath = emptyString();
}

// MARK: - WebCore::ModelPlayer overrides.

void ARKitInlinePreviewModelPlayerMac::load(WebCore::Model& modelSource, WebCore::LayoutSize size)
{
    auto strongClient = client();
    if (!strongClient)
        return;

    RefPtr strongPage = page();
    if (!strongPage) {
        strongClient->didFailLoading(*this, WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, modelSource.url(), "WebPage destroyed"_s });
        return;
    }

    createFile(modelSource);

    m_inlinePreview = adoptNS([allocASVInlinePreviewInstance() initWithFrame:CGRectMake(0, 0, size.width(), size.height())]);
    LOG(ModelElement, "ARKitInlinePreviewModelPlayer::modelDidChange() created preview with UUID %s and size %f x %f.", ((String)[m_inlinePreview uuid].UUIDString).utf8().data(), size.width(), size.height());

    CompletionHandler<void(Expected<std::pair<String, uint32_t>, WebCore::ResourceError>)> completionHandler = [weakSelf = WeakPtr { *this }] (Expected<std::pair<String, uint32_t>, WebCore::ResourceError> result) mutable {
        RefPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        auto strongClient = strongSelf->client();
        if (!strongClient)
            return;

        if (!result) {
            LOG(ModelElement, "ARKitInlinePreviewModelPlayer::inlinePreviewDidObtainContextId() received error from UIProcess");
            strongClient->didFailLoading(*strongSelf, result.error());
            return;
        }

        auto& [uuid, contextId] = *result;
        String expectedUUID = [strongSelf->m_inlinePreview uuid].UUIDString;

        if (uuid != expectedUUID) {
            LOG(ModelElement, "ARKitInlinePreviewModelPlayer::inlinePreviewDidObtainContextId() UUID mismatch, received %s but expected %s.", uuid.utf8().data(), expectedUUID.utf8().data());
            strongClient->didFailLoading(*strongSelf, WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, { }, makeString("ARKitInlinePreviewModelPlayer::inlinePreviewDidObtainContextId() UUID mismatch, received ", uuid, " but expected ", expectedUUID, ".") });
            return;
        }

        [strongSelf->m_inlinePreview setRemoteContext:contextId];
        LOG(ModelElement, "ARKitInlinePreviewModelPlayer::inlinePreviewDidObtainContextId() successfully established remote connection for UUID %s.", uuid.utf8().data());

        strongClient->didFinishLoading(*strongSelf);
    };
    
    strongPage->sendWithAsyncReply(Messages::WebPageProxy::ModelElementDidCreatePreview(URL::fileURLWithFileSystemPath(m_filePath), [m_inlinePreview uuid].UUIDString, size), WTFMove(completionHandler));
}

PlatformLayer* ARKitInlinePreviewModelPlayerMac::layer()
{
    return [m_inlinePreview layer];
}

bool ARKitInlinePreviewModelPlayerMac::supportsMouseInteraction()
{
    return true;
}

bool ARKitInlinePreviewModelPlayerMac::supportsDragging()
{
    return false;
}

void ARKitInlinePreviewModelPlayerMac::handleMouseDown(const LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (auto* page = this->page())
        page->send(Messages::WebPageProxy::HandleMouseDownForModelElement([m_inlinePreview uuid].UUIDString, flippedLocationInElement, timestamp));
}

void ARKitInlinePreviewModelPlayerMac::handleMouseMove(const LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (auto* page = this->page())
        page->send(Messages::WebPageProxy::HandleMouseMoveForModelElement([m_inlinePreview uuid].UUIDString, flippedLocationInElement, timestamp));
}

void ARKitInlinePreviewModelPlayerMac::handleMouseUp(const LayoutPoint& flippedLocationInElement, MonotonicTime timestamp)
{
    if (auto* page = this->page())
        page->send(Messages::WebPageProxy::HandleMouseUpForModelElement([m_inlinePreview uuid].UUIDString, flippedLocationInElement, timestamp));
}

}

#endif
