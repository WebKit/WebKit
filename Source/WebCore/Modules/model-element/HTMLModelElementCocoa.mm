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

#include "config.h"
#include "HTMLModelElement.h"

#if ENABLE(MODEL_ELEMENT) && ENABLE(ARKIT_INLINE_PREVIEW_MAC)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Logging.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "Settings.h"
#include <pal/spi/mac/SystemPreviewSPI.h>
#include <wtf/FileSystem.h>
#include <wtf/RetainPtr.h>
#include <wtf/SoftLinking.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>

SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVInlinePreview);

namespace WebCore {

void HTMLModelElement::createFile()
{
    // The need for a file is only temporary due to the nature of ASVInlinePreview,
    // https://bugs.webkit.org/show_bug.cgi?id=227567.

    clearFile();

    auto pathToDirectory = HTMLModelElement::modelElementCacheDirectory();
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
    auto fileName = FileSystem::encodeForFileName(createCanonicalUUIDString()) + ".usdz";
    auto filePath = FileSystem::pathByAppendingComponent(pathToDirectory, fileName);
    auto file = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Write);
    if (file <= 0)
        return;

    FileSystem::writeToFile(file, m_data->data(), m_data->size());
    FileSystem::closeFile(file);
    m_filePath = filePath;
}

void HTMLModelElement::clearFile()
{
    if (m_filePath.isEmpty())
        return;

    FileSystem::deleteFile(m_filePath);
    m_filePath = emptyString();
}

void HTMLModelElement::modelDidChange()
{
    createFile();

    auto* renderer = this->renderer();
    if (!renderer)
        return;

    auto size = renderer->absoluteBoundingBoxRect(false).size();

    m_inlinePreview = adoptNS([allocASVInlinePreviewInstance() initWithFrame:CGRectMake(0, 0, size.width(), size.height())]);
    LOG(ModelElement, "HTMLModelElement::modelDidChange() created preview with UUID %s and size %f x %f.", ((String)[m_inlinePreview uuid].UUIDString).utf8().data(), size.width(), size.height());

    if (auto* page = document().page())
        page->chrome().client().modelElementDidCreatePreview(*this, URL::fileURLWithFileSystemPath(m_filePath), [m_inlinePreview uuid].UUIDString, size);
}

void HTMLModelElement::inlinePreviewDidObtainContextId(const String& uuid, uint32_t contextId)
{
    if (!document().settings().modelElementEnabled())
        return;

    if (uuid != (String)[m_inlinePreview uuid].UUIDString) {
        LOG(ModelElement, "HTMLModelElement::inlinePreviewDidObtainContextId() UUID mismatch, received %s but expected %s.", uuid.utf8().data(), ((String)[m_inlinePreview uuid].UUIDString).utf8().data());
        return;
    }

    [m_inlinePreview setRemoteContext:contextId];
    LOG(ModelElement, "HTMLModelElement::inlinePreviewDidObtainContextId() successfully established remote connection for UUID %s.", uuid.utf8().data());

    if (auto* renderer = this->renderer())
        renderer->updateFromElement();
}

PlatformLayer* HTMLModelElement::platformLayer() const
{
    return [m_inlinePreview layer];
}

}

#endif // ENABLE(MODEL_ELEMENT) && ENABLE(ARKIT_INLINE_PREVIEW_MAC)
