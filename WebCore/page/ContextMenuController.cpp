/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ContextMenuController.h"

#include "Chrome.h"
#include "ContextMenu.h"
#include "ContextMenuClient.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "MouseEvent.h"
#include "Node.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "markup.h"

namespace WebCore {

using namespace EventNames;

ContextMenuController::ContextMenuController(Page* page, ContextMenuClient* client)
    : m_page(page)
    , m_client(client)
    , m_contextMenu(0)
{
}

ContextMenuController::~ContextMenuController()
{
    m_client->contextMenuDestroyed();
}

void ContextMenuController::handleContextMenuEvent(Event* event)
{
    ASSERT(event->type() == contextmenuEvent);
    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    HitTestResult result(IntPoint(mouseEvent->pageX(), mouseEvent->pageY()));

    if (RenderObject* renderer = event->target()->renderer())
        if (RenderLayer* layer = renderer->enclosingLayer())
            layer->hitTest(HitTestRequest(false, true), result);

    if (!result.innerNonSharedNode())
        return;

    m_contextMenu.set(new ContextMenu(result));
    m_contextMenu->populate();
    m_client->addCustomContextMenuItems(m_contextMenu.get());
    m_contextMenu->show();

    event->setDefaultHandled();
}

static String makeGoogleSearchURL(String searchString)
{
    searchString.stripWhiteSpace();
    DeprecatedString encoded = KURL::encode_string(searchString.deprecatedString());
    encoded.replace(DeprecatedString("%20"), DeprecatedString("+"));
    
    String url("http://www.google.com/search?client=safari&q=");
    url.append(String(encoded));
    url.append("&ie=UTF-8&oe=UTF-8");
    return url;
}

void ContextMenuController::contextMenuItemSelected(ContextMenuItem* item)
{
    ASSERT(item->menu() == contextMenu());
    ASSERT(item->type() == ActionType);

    if (item->action() >= ContextMenuItemBaseApplicationTag) {
        m_client->contextMenuItemSelected(item);
        return;
    }

    Frame* frame = m_contextMenu->hitTestResult().innerNonSharedNode()->document()->frame();
    if (!frame)
        return;
    ASSERT(m_page == frame->page());
    
    switch (item->action()) {
        case ContextMenuItemTagOpenLinkInNewWindow: {
            ResourceRequest request = ResourceRequest(m_contextMenu->hitTestResult().absoluteLinkURL());
            String referrer = frame->loader()->referrer();
            m_page->chrome()->createWindow(FrameLoadRequest(request, referrer));
            break;
        }
        case ContextMenuItemTagDownloadLinkToDisk:
            // FIXME: Some day we should be able to do this from within WebCore.
            m_client->downloadURL(m_contextMenu->hitTestResult().absoluteLinkURL());
            break;
        case ContextMenuItemTagCopyLinkToClipboard:
            // FIXME: The Pasteboard class is not written yet. This is what we should be able to do some day:
            // generalPasteboard()->copy(m_contextMenu->hitTestResult().absoluteLinkURL(), 
            //      m_contextMenu->hitTestResult.textContent());
            // For now, call into the client. This is temporary!
            m_client->copyLinkToClipboard(m_contextMenu->hitTestResult());
            break;
        case ContextMenuItemTagOpenImageInNewWindow: {
            ResourceRequest request = ResourceRequest(m_contextMenu->hitTestResult().absoluteImageURL());
            String referrer = frame->loader()->referrer();
            m_page->chrome()->createWindow(FrameLoadRequest(request, referrer));
            break;
        }
        case ContextMenuItemTagDownloadImageToDisk:
            // FIXME: Some day we should be able to do this from within WebCore.
            m_client->downloadURL(m_contextMenu->hitTestResult().absoluteImageURL());
            break;
        case ContextMenuItemTagCopyImageToClipboard:
            // FIXME: The Pasteboard class is not written yet
            // For now, call into the client. This is temporary!
            m_client->copyImageToClipboard(m_contextMenu->hitTestResult());
            break;
        case ContextMenuItemTagOpenFrameInNewWindow: {
            // FIXME: The DocumentLoader is all-Mac right now
#if PLATFORM(MAC)
            KURL unreachableURL = frame->loader()->documentLoader()->unreachableURL();
            if (frame && unreachableURL.isEmpty())
                unreachableURL = frame->loader()->documentLoader()->URL();
            ResourceRequest request = ResourceRequest(unreachableURL);
            String referrer = frame->loader()->referrer();
            if (m_page)
                m_page->chrome()->createWindow(FrameLoadRequest(request, referrer));
#endif
            break;
        }
        case ContextMenuItemTagCopy:
            frame->editor()->copy();
            break;
        case ContextMenuItemTagGoBack:
            frame->loader()->goBackOrForward(-1);
            break;
        case ContextMenuItemTagGoForward:
            frame->loader()->goBackOrForward(1);
            break;
        case ContextMenuItemTagStop:
            frame->loader()->stop();
            break;
        case ContextMenuItemTagReload:
            frame->loader()->reload();
            break;
        case ContextMenuItemTagCut:
            frame->editor()->cut();
            break;
        case ContextMenuItemTagPaste:
            frame->editor()->paste();
            break;
        case ContextMenuItemTagSpellingGuess:
            ASSERT(frame->selectedText().length());
            if (frame->editor()->shouldInsertText(item->title(), frame->selectionController()->toRange().get(), EditorInsertActionPasted)) {
                Document* document = frame->document();
                applyCommand(new ReplaceSelectionCommand(document, createFragmentFromMarkup(document, item->title(), ""),
                    true, false, true));
                frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
            }
            break;
        case ContextMenuItemTagIgnoreSpelling:
            frame->ignoreSpelling();
            break;
        case ContextMenuItemTagLearnSpelling:
            frame->learnSpelling();
            break;
        case ContextMenuItemTagSearchInSpotlight:
#if PLATFORM(MAC)
            m_client->searchWithSpotlight();
#endif
            break;
        case ContextMenuItemTagSearchWeb: {
            String url = makeGoogleSearchURL(frame->selectedText());
            ResourceRequest request = ResourceRequest(url);
            frame->loader()->urlSelected(FrameLoadRequest(request), new Event());
            break;
        }
        case ContextMenuItemTagLookUpInDictionary:
            // FIXME: Some day we may be able to do this from within WebCore.
            m_client->lookUpInDictionary(frame);
            break;
        // PDF actions. Let's take care of this later. 
        case ContextMenuItemTagOpenWithDefaultApplication:
        case ContextMenuItemPDFActualSize:
        case ContextMenuItemPDFZoomIn:
        case ContextMenuItemPDFZoomOut:
        case ContextMenuItemPDFAutoSize:
        case ContextMenuItemPDFSinglePage:
        case ContextMenuItemPDFFacingPages:
        case ContextMenuItemPDFContinuous:
        case ContextMenuItemPDFNextPage:
        case ContextMenuItemPDFPreviousPage:
        default:
            break;
    }
}

} // namespace WebCore

