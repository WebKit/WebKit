/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "EditableImageController.h"

#if HAVE(PENCILKIT)

#import "APIAttachment.h"
#import "EditableImageControllerMessages.h"
#import "PencilKitSPI.h"
#import "WKDrawingView.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/GraphicsLayer.h>
#import <wtf/RetainPtr.h>

namespace WebKit {

EditableImageController::EditableImageController(WebPageProxy& webPageProxy)
    : m_webPageProxy(makeWeakPtr(webPageProxy))
{
    if (auto* webPageProxy = m_webPageProxy.get())
        webPageProxy->process().addMessageReceiver(Messages::EditableImageController::messageReceiverName(), webPageProxy->pageID(), *this);
}

EditableImageController::~EditableImageController()
{
    if (auto* webPageProxy = m_webPageProxy.get())
        webPageProxy->process().removeMessageReceiver(Messages::EditableImageController::messageReceiverName(), webPageProxy->pageID());
}

EditableImage& EditableImageController::ensureEditableImage(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID)
{
    auto result = m_editableImages.ensure(embeddedViewID, [&] {
        std::unique_ptr<EditableImage> image = std::make_unique<EditableImage>();
        image->drawingView = adoptNS([[WKDrawingView alloc] initWithEmbeddedViewID:embeddedViewID webPageProxy:*m_webPageProxy]);
        return image;
    });
    return *result.iterator->value;
}

EditableImage* EditableImageController::editableImage(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID)
{
    auto drawingViewIter = m_editableImages.find(embeddedViewID);
    if (drawingViewIter == m_editableImages.end())
        return nil;
    return drawingViewIter->value.get();
}

void EditableImageController::didCreateEditableImage(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID)
{
    ensureEditableImage(embeddedViewID);
}

void EditableImageController::didDestroyEditableImage(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID)
{
    m_editableImages.remove(embeddedViewID);
}

void EditableImageController::associateWithAttachment(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID, const String& attachmentID)
{
    if (!m_webPageProxy)
        return;
    auto& page = *m_webPageProxy;

    page.registerAttachmentIdentifier(attachmentID);
    auto& attachment = *page.attachmentForIdentifier(attachmentID);

    auto& editableImage = ensureEditableImage(embeddedViewID);
    WeakObjCPtr<WKDrawingView> drawingView = editableImage.drawingView.get();
    editableImage.attachmentID = attachmentID;

    loadStrokesFromAttachment(editableImage, attachment);

    attachment.setFileWrapperGenerator([drawingView]() -> RetainPtr<NSFileWrapper> {
        if (!drawingView)
            return nil;
        NSData *data = [drawingView PNGRepresentation];
        if (!data)
            return nil;
        RetainPtr<NSFileWrapper> fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
        [fileWrapper setPreferredFilename:@"drawing.png"];
        return fileWrapper;
    });
    attachment.setContentType("public.png");
}

void EditableImageController::loadStrokesFromAttachment(EditableImage& editableImage, const API::Attachment& attachment)
{
    ASSERT(attachment.identifier() == editableImage.attachmentID);
    NSFileWrapper *fileWrapper = attachment.fileWrapper();
    if (!fileWrapper.isRegularFile)
        return;
    [editableImage.drawingView loadDrawingFromPNGRepresentation:fileWrapper.regularFileContents];
}

void EditableImageController::invalidateAttachmentForEditableImage(WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID)
{
    if (!m_webPageProxy)
        return;
    auto& page = *m_webPageProxy;

    auto editableImage = this->editableImage(embeddedViewID);
    if (!editableImage)
        return;

    auto attachment = page.attachmentForIdentifier(editableImage->attachmentID);
    if (!attachment)
        return;

    attachment->invalidateGeneratedFileWrapper();
}

WebPageProxy::ShouldUpdateAttachmentAttributes EditableImageController::willUpdateAttachmentAttributes(const API::Attachment& attachment)
{
    for (auto& editableImage : m_editableImages.values()) {
        if (editableImage->attachmentID != attachment.identifier())
            continue;

        loadStrokesFromAttachment(*editableImage, attachment);
        return WebPageProxy::ShouldUpdateAttachmentAttributes::No;
    }

    return WebPageProxy::ShouldUpdateAttachmentAttributes::Yes;
}

} // namespace WebKit

#endif // HAVE(PENCILKIT)
