/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"
#include "ContextMenuClientImpl.h"

#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "ContextMenu.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "MediaError.h"
#include "PlatformString.h"
#include "RenderWidget.h"
#include "TextBreakIterator.h"
#include "Widget.h"

#include "WebContextMenuData.h"
#include "WebDataSourceImpl.h"
#include "WebFrameImpl.h"
#include "WebMenuItemInfo.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPoint.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLResponse.h"
#include "WebVector.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNodeType::NONE if the URL could not
// be determined for some reason.
static WebURL urlFromFrame(Frame* frame)
{
    if (frame) {
        DocumentLoader* dl = frame->loader()->documentLoader();
        if (dl) {
            WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
            if (ds)
                return ds->hasUnreachableURL() ? ds->unreachableURL() : ds->request().url();
        }
    }
    return WebURL();
}

// Helper function to determine whether text is a single word.
static bool isASingleWord(const String& text)
{
    TextBreakIterator* it = wordBreakIterator(text.characters(), text.length());
    return it && textBreakNext(it) == static_cast<int>(text.length());
}

// Helper function to get misspelled word on which context menu
// is to be evolked. This function also sets the word on which context menu
// has been evoked to be the selected word, as required. This function changes
// the selection only when there were no selected characters on OS X.
static String selectMisspelledWord(const ContextMenu* defaultMenu, Frame* selectedFrame)
{
    // First select from selectedText to check for multiple word selection.
    String misspelledWord = selectedFrame->editor()->selectedText().stripWhiteSpace();

    // If some texts were already selected, we don't change the selection.
    if (!misspelledWord.isEmpty()) {
        // Don't provide suggestions for multiple words.
        if (!isASingleWord(misspelledWord))
            return String();
        return misspelledWord;
    }

    // Selection is empty, so change the selection to the word under the cursor.
    HitTestResult hitTestResult = selectedFrame->eventHandler()->
        hitTestResultAtPoint(defaultMenu->hitTestResult().point(), true);
    Node* innerNode = hitTestResult.innerNode();
    VisiblePosition pos(innerNode->renderer()->positionForPoint(
        hitTestResult.localPoint()));

    if (pos.isNull())
        return misspelledWord; // It is empty.

    WebFrameImpl::selectWordAroundPosition(selectedFrame, pos);
    misspelledWord = selectedFrame->editor()->selectedText().stripWhiteSpace();

#if OS(DARWIN)
    // If misspelled word is still empty, then that portion should not be
    // selected. Set the selection to that position only, and do not expand.
    if (misspelledWord.isEmpty())
        selectedFrame->selection()->setSelection(VisibleSelection(pos));
#else
    // On non-Mac, right-click should not make a range selection in any case.
    selectedFrame->selection()->setSelection(VisibleSelection(pos));
#endif
    return misspelledWord;
}

PlatformMenuDescription ContextMenuClientImpl::getCustomMenuFromDefaultItems(
    ContextMenu* defaultMenu)
{
    // Displaying the context menu in this function is a big hack as we don't
    // have context, i.e. whether this is being invoked via a script or in
    // response to user input (Mouse event WM_RBUTTONDOWN,
    // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
    // in response to the above input events before popping up the context menu.
    if (!m_webView->contextMenuAllowed())
        return 0;

    HitTestResult r = defaultMenu->hitTestResult();
    Frame* selectedFrame = r.innerNonSharedNode()->document()->frame();

    WebContextMenuData data;
    data.mousePosition = selectedFrame->view()->contentsToWindow(r.point());

    // Compute edit flags.
    data.editFlags = WebContextMenuData::CanDoNone;
    if (m_webView->focusedWebCoreFrame()->editor()->canUndo())
        data.editFlags |= WebContextMenuData::CanUndo;
    if (m_webView->focusedWebCoreFrame()->editor()->canRedo())
        data.editFlags |= WebContextMenuData::CanRedo;
    if (m_webView->focusedWebCoreFrame()->editor()->canCut())
        data.editFlags |= WebContextMenuData::CanCut;
    if (m_webView->focusedWebCoreFrame()->editor()->canCopy())
        data.editFlags |= WebContextMenuData::CanCopy;
    if (m_webView->focusedWebCoreFrame()->editor()->canPaste())
        data.editFlags |= WebContextMenuData::CanPaste;
    if (m_webView->focusedWebCoreFrame()->editor()->canDelete())
        data.editFlags |= WebContextMenuData::CanDelete;
    // We can always select all...
    data.editFlags |= WebContextMenuData::CanSelectAll;
    data.editFlags |= WebContextMenuData::CanTranslate;

    // Links, Images, Media tags, and Image/Media-Links take preference over
    // all else.
    data.linkURL = r.absoluteLinkURL();

    if (!r.absoluteImageURL().isEmpty()) {
        data.srcURL = r.absoluteImageURL();
        data.mediaType = WebContextMenuData::MediaTypeImage;
    } else if (!r.absoluteMediaURL().isEmpty()) {
        data.srcURL = r.absoluteMediaURL();

        // We know that if absoluteMediaURL() is not empty, then this
        // is a media element.
        HTMLMediaElement* mediaElement =
            static_cast<HTMLMediaElement*>(r.innerNonSharedNode());
        if (mediaElement->hasTagName(HTMLNames::videoTag))
            data.mediaType = WebContextMenuData::MediaTypeVideo;
        else if (mediaElement->hasTagName(HTMLNames::audioTag))
            data.mediaType = WebContextMenuData::MediaTypeAudio;

        if (mediaElement->error())
            data.mediaFlags |= WebContextMenuData::MediaInError;
        if (mediaElement->paused())
            data.mediaFlags |= WebContextMenuData::MediaPaused;
        if (mediaElement->muted())
            data.mediaFlags |= WebContextMenuData::MediaMuted;
        if (mediaElement->loop())
            data.mediaFlags |= WebContextMenuData::MediaLoop;
        if (mediaElement->supportsSave())
            data.mediaFlags |= WebContextMenuData::MediaCanSave;
        if (mediaElement->hasAudio())
            data.mediaFlags |= WebContextMenuData::MediaHasAudio;
        if (mediaElement->hasVideo())
            data.mediaFlags |= WebContextMenuData::MediaHasVideo;
        if (mediaElement->controls())
            data.mediaFlags |= WebContextMenuData::MediaControls;
    } else if (r.innerNonSharedNode()->hasTagName(HTMLNames::objectTag)
               || r.innerNonSharedNode()->hasTagName(HTMLNames::embedTag)) {
        RenderObject* object = r.innerNonSharedNode()->renderer();
        if (object && object->isWidget()) {
            Widget* widget = toRenderWidget(object)->widget();
            if (widget && widget->isPluginContainer()) {
                WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(widget);
                WebString text = plugin->plugin()->selectionAsText();
                if (!text.isEmpty()) {
                    data.selectedText = text;
                    data.editFlags |= WebContextMenuData::CanCopy;
                }
                data.editFlags &= ~WebContextMenuData::CanTranslate;
                data.linkURL = plugin->plugin()->linkAtPosition(data.mousePosition);
            }
        }
    }

    data.isImageBlocked =
        (data.mediaType == WebContextMenuData::MediaTypeImage) && !r.image();

    // If it's not a link, an image, a media element, or an image/media link,
    // show a selection menu or a more generic page menu.
    data.frameEncoding = selectedFrame->loader()->writer()->encoding();

    // Send the frame and page URLs in any case.
    data.pageURL = urlFromFrame(m_webView->mainFrameImpl()->frame());
    if (selectedFrame != m_webView->mainFrameImpl()->frame())
        data.frameURL = urlFromFrame(selectedFrame);

    if (r.isSelected())
        data.selectedText = selectedFrame->editor()->selectedText().stripWhiteSpace();

    if (r.isContentEditable()) {
        data.isEditable = true;
        if (m_webView->focusedWebCoreFrame()->editor()->isContinuousSpellCheckingEnabled()) {
            data.isSpellCheckingEnabled = true;
            // Spellchecking might be enabled for the field, but could be disabled on the node.
            if (m_webView->focusedWebCoreFrame()->editor()->isSpellCheckingEnabledInFocusedNode())
                data.misspelledWord = selectMisspelledWord(defaultMenu, selectedFrame);
        }
    }

#if OS(DARWIN)
    ExceptionCode ec = 0;
    RefPtr<CSSStyleDeclaration> style = selectedFrame->document()->createCSSStyleDeclaration();
    style->setProperty(CSSPropertyDirection, "ltr", false, ec);
    if (selectedFrame->editor()->selectionHasStyle(style.get()) != FalseTriState)
        data.writingDirectionLeftToRight |= WebContextMenuData::CheckableMenuItemChecked;
    style->setProperty(CSSPropertyDirection, "rtl", false, ec);
    if (selectedFrame->editor()->selectionHasStyle(style.get()) != FalseTriState)
        data.writingDirectionRightToLeft |= WebContextMenuData::CheckableMenuItemChecked;
#endif // OS(DARWIN)

    // Now retrieve the security info.
    DocumentLoader* dl = selectedFrame->loader()->documentLoader();
    WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
    if (ds)
        data.securityInfo = ds->response().securityInfo();

    // Filter out custom menu elements and add them into the data.
    populateCustomMenuItems(defaultMenu, &data);

    WebFrame* selected_web_frame = WebFrameImpl::fromFrame(selectedFrame);
    if (m_webView->client())
        m_webView->client()->showContextMenu(selected_web_frame, data);

    return 0;
}

void ContextMenuClientImpl::populateCustomMenuItems(WebCore::ContextMenu* defaultMenu, WebContextMenuData* data)
{
    Vector<WebMenuItemInfo> customItems;
    for (size_t i = 0; i < defaultMenu->itemCount(); ++i) {
        ContextMenuItem* inputItem = defaultMenu->itemAtIndex(i, defaultMenu->platformDescription());
        if (inputItem->action() < ContextMenuItemBaseCustomTag || inputItem->action() >  ContextMenuItemLastCustomTag)
            continue;

        WebMenuItemInfo outputItem;
        outputItem.label = inputItem->title();
        outputItem.enabled = inputItem->enabled();
        outputItem.checked = inputItem->checked();
        outputItem.action = static_cast<unsigned>(inputItem->action() - ContextMenuItemBaseCustomTag);
        switch (inputItem->type()) {
        case ActionType:
            outputItem.type = WebMenuItemInfo::Option;
            break;
        case CheckableActionType:
            outputItem.type = WebMenuItemInfo::CheckableOption;
            break;
        case SeparatorType:
            outputItem.type = WebMenuItemInfo::Separator;
            break;
        case SubmenuType:
            outputItem.type = WebMenuItemInfo::Group;
            break;
        }
        customItems.append(outputItem);
    }

    WebVector<WebMenuItemInfo> outputItems(customItems.size());
    for (size_t i = 0; i < customItems.size(); ++i)
        outputItems[i] = customItems[i];
    data->customItems.swap(outputItems);
}

} // namespace WebKit
