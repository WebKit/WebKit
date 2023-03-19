/*
 * Copyright (C) 2007-2020 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "DragController.h"

#if ENABLE(DRAG_SUPPORT)

#import "DataTransfer.h"
#import "DeprecatedGlobalSettings.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DragClient.h"
#import "DragData.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Element.h"
#import "File.h"
#import "HTMLAttachmentElement.h"
#import "LocalFrame.h"
#import "LocalFrameView.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "Range.h"

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

const int DragController::MaxOriginalImageArea = 1500 * 1500;
const int DragController::DragIconRightInset = 7;
const int DragController::DragIconBottomInset = 3;

const float DragController::DragImageAlpha = 0.75f;

bool DragController::isCopyKeyDown(const DragData& dragData)
{
    return dragData.flags().contains(DragApplicationFlags::IsCopyKeyDown);
}
    
std::optional<DragOperation> DragController::dragOperation(const DragData& dragData)
{
    if (dragData.flags().contains(DragApplicationFlags::IsModal))
        return std::nullopt;

    bool mayContainURL;
    if (canLoadDataFromDraggingPasteboard())
        mayContainURL = dragData.containsURL();
    else
        mayContainURL = dragData.containsURLTypeIdentifier();

    if (!mayContainURL && !dragData.containsPromise())
        return std::nullopt;

    if (!m_documentUnderMouse || (!(dragData.flags().containsAll({ DragApplicationFlags::HasAttachedSheet, DragApplicationFlags::IsSource }))))
        return DragOperation::Copy;

    return std::nullopt;
}

const IntSize& DragController::maxDragImageSize()
{
    static const IntSize maxDragImageSize(400, 400);
    
    return maxDragImageSize;
}

void DragController::cleanupAfterSystemDrag()
{
#if PLATFORM(MAC)
    // Drag has ended, dragEnded *should* have been called, however it is possible
    // for the UIDelegate to take over the drag, and fail to send the appropriate
    // drag termination event.  As dragEnded just resets drag variables, we just
    // call it anyway to be on the safe side.
    // We don't want to do this for WebKit2, since the client call to start the drag
    // is asynchronous.
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page.mainFrame())) {
        if (localMainFrame->view()->platformWidget())
            dragEnded();
    }
#endif
}

#if PLATFORM(IOS_FAMILY)

DragOperation DragController::platformGenericDragOperation()
{
    // On iOS, UIKit skips the -performDrop invocation altogether if MOVE is forbidden.
    // Thus, if MOVE is not allowed in the drag source operation mask, fall back to only other allowable action, COPY.
    return DragOperation::Copy;
}

void DragController::updateSupportedTypeIdentifiersForDragHandlingMethod(DragHandlingMethod dragHandlingMethod, const DragData& dragData) const
{
    Vector<String> supportedTypes;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (dragHandlingMethod) {
    case DragHandlingMethod::PageLoad:
        supportedTypes.append(kUTTypeURL);
        break;
    case DragHandlingMethod::EditPlainText:
        supportedTypes.append(kUTTypeURL);
        supportedTypes.append(kUTTypePlainText);
        break;
    case DragHandlingMethod::EditRichText:
        if (DeprecatedGlobalSettings::attachmentElementEnabled()) {
            supportedTypes.append(WebArchivePboardType);
            supportedTypes.append(kUTTypeContent);
            supportedTypes.append(kUTTypeItem);
        } else {
            for (NSString *type in Pasteboard::supportedWebContentPasteboardTypes())
                supportedTypes.append(type);
        }
        break;
    case DragHandlingMethod::SetColor:
        supportedTypes.append(UIColorPboardType);
        break;
    default:
        for (NSString *type in Pasteboard::supportedFileUploadPasteboardTypes())
            supportedTypes.append(type);
        break;
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    auto context = dragData.createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->updateSupportedTypeIdentifiers(supportedTypes, dragData.pasteboardName(), context.get());
}

#endif // PLATFORM(IOS_FAMILY)

void DragController::declareAndWriteDragImage(DataTransfer& dataTransfer, Element& element, const URL& url, const String& label)
{
    client().declareAndWriteDragImage(dataTransfer.pasteboard().name(), element, url, label, element.document().frame());
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
