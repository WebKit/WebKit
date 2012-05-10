/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
#include "ContextMenuController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInImageElement.h"

#include "HistoryItem.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "MediaError.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "TextBreakIterator.h"
#include "Widget.h"

#include "WebContextMenuData.h"
#include "WebDataSourceImpl.h"
#include "WebFormElement.h"
#include "WebFrameImpl.h"
#include "WebMenuItemInfo.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "platform/WebPoint.h"
#include "WebSearchableFormData.h"
#include "WebSpellCheckClient.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebURLResponse.h"
#include "platform/WebVector.h"
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
        hitTestResultAtPoint(selectedFrame->page()->contextMenuController()->hitTestResult().point(), true);
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

    HitTestResult r = m_webView->page()->contextMenuController()->hitTestResult();
    Frame* selectedFrame = r.innerNonSharedNode()->document()->frame();

    WebContextMenuData data;
    data.mousePosition = selectedFrame->view()->contentsToWindow(r.roundedPoint());

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
            data.mediaFlags |= WebContextMenuData::MediaControlRootElement;
    } else if (r.innerNonSharedNode()->hasTagName(HTMLNames::objectTag)
               || r.innerNonSharedNode()->hasTagName(HTMLNames::embedTag)) {
        RenderObject* object = r.innerNonSharedNode()->renderer();
        if (object && object->isWidget()) {
            Widget* widget = toRenderWidget(object)->widget();
            if (widget && widget->isPluginContainer()) {
                data.mediaType = WebContextMenuData::MediaTypePlugin;
                WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(widget);
                WebString text = plugin->plugin()->selectionAsText();
                if (!text.isEmpty()) {
                    data.selectedText = text;
                    data.editFlags |= WebContextMenuData::CanCopy;
                }
                data.editFlags &= ~WebContextMenuData::CanTranslate;
                data.linkURL = plugin->plugin()->linkAtPosition(data.mousePosition);
                if (plugin->plugin()->supportsPaginatedPrint())
                    data.mediaFlags |= WebContextMenuData::MediaCanPrint;

                HTMLPlugInImageElement* pluginElement = static_cast<HTMLPlugInImageElement*>(r.innerNonSharedNode());
                data.srcURL = pluginElement->document()->completeURL(pluginElement->url());
                data.mediaFlags |= WebContextMenuData::MediaCanSave;

                // Add context menu commands that are supported by the plugin.
                if (plugin->plugin()->canRotateView())
                    data.mediaFlags |= WebContextMenuData::MediaCanRotate;
            }
        }
    }

    data.isImageBlocked =
        (data.mediaType == WebContextMenuData::MediaTypeImage) && !r.image();

    // If it's not a link, an image, a media element, or an image/media link,
    // show a selection menu or a more generic page menu.
    if (selectedFrame->document()->loader())
        data.frameEncoding = selectedFrame->document()->encoding();

    // Send the frame and page URLs in any case.
    data.pageURL = urlFromFrame(m_webView->mainFrameImpl()->frame());
    if (selectedFrame != m_webView->mainFrameImpl()->frame()) {
        data.frameURL = urlFromFrame(selectedFrame);
        RefPtr<HistoryItem> historyItem = selectedFrame->loader()->history()->currentItem();
        if (historyItem)
            data.frameHistoryItem = WebHistoryItem(historyItem);
    }

    if (r.isSelected()) {
        if (!r.innerNonSharedNode()->hasTagName(HTMLNames::inputTag) || !static_cast<HTMLInputElement*>(r.innerNonSharedNode())->isPasswordField())
            data.selectedText = selectedFrame->editor()->selectedText().stripWhiteSpace();
    }

    if (r.isContentEditable()) {
        data.isEditable = true;
#if ENABLE(INPUT_SPEECH)
        if (r.innerNonSharedNode()->hasTagName(HTMLNames::inputTag)) {
            data.isSpeechInputEnabled = 
                static_cast<HTMLInputElement*>(r.innerNonSharedNode())->isSpeechEnabled();
        }  
#endif
        // When Chrome enables asynchronous spellchecking, its spellchecker adds spelling markers to misspelled
        // words and attaches suggestions to these markers in the background. Therefore, when a user right-clicks
        // a mouse on a word, Chrome just needs to find a spelling marker on the word instread of spellchecking it.
        if (selectedFrame->settings() && selectedFrame->settings()->asynchronousSpellCheckingEnabled()) {
            RefPtr<Range> range = selectedFrame->selection()->toNormalizedRange();
            if (range.get()) {
                Vector<DocumentMarker*> markers = selectedFrame->document()->markers()->markersInRange(range.get(), DocumentMarker::Spelling);
                if (!markers.isEmpty()) {
                    Vector<String> suggestions;
                    for (size_t i = 0; i < markers.size(); ++i) {
                        if (!markers[i]->description().isEmpty()) {
                            Vector<String> descriptions;
                            markers[i]->description().split('\n', descriptions);
                            suggestions.append(descriptions);
                        }
                    }
                    data.dictionarySuggestions = suggestions;
                    data.misspelledWord = selectMisspelledWord(defaultMenu, selectedFrame);
                }
            }
        } else if (m_webView->focusedWebCoreFrame()->editor()->isContinuousSpellCheckingEnabled()) {
            data.isSpellCheckingEnabled = true;
            // Spellchecking might be enabled for the field, but could be disabled on the node.
            if (m_webView->focusedWebCoreFrame()->editor()->isSpellCheckingEnabledInFocusedNode()) {
                data.misspelledWord = selectMisspelledWord(defaultMenu, selectedFrame);
                if (m_webView->spellCheckClient()) {
                    int misspelledOffset, misspelledLength;
                    m_webView->spellCheckClient()->spellCheck(
                        data.misspelledWord, misspelledOffset, misspelledLength,
                        &data.dictionarySuggestions);
                    if (!misspelledLength)
                        data.misspelledWord.reset();
                }
            }
        }
        HTMLFormElement* form = selectedFrame->selection()->currentForm();
        if (form && form->checkValidity() && r.innerNonSharedNode()->hasTagName(HTMLNames::inputTag)) {
            HTMLInputElement* selectedElement = static_cast<HTMLInputElement*>(r.innerNonSharedNode());
            if (selectedElement) {
                WebSearchableFormData ws = WebSearchableFormData(WebFormElement(form), WebInputElement(selectedElement));
                if (ws.url().isValid())
                    data.keywordURL = ws.url();
            }
        }
    }

#if OS(DARWIN)
    if (selectedFrame->editor()->selectionHasStyle(CSSPropertyDirection, "ltr") != FalseTriState)
        data.writingDirectionLeftToRight |= WebContextMenuData::CheckableMenuItemChecked;
    if (selectedFrame->editor()->selectionHasStyle(CSSPropertyDirection, "rtl") != FalseTriState)
        data.writingDirectionRightToLeft |= WebContextMenuData::CheckableMenuItemChecked;
#endif // OS(DARWIN)

    // Now retrieve the security info.
    DocumentLoader* dl = selectedFrame->loader()->documentLoader();
    WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
    if (ds)
        data.securityInfo = ds->response().securityInfo();

    data.referrerPolicy = static_cast<WebReferrerPolicy>(selectedFrame->document()->referrerPolicy());

    // Filter out custom menu elements and add them into the data.
    populateCustomMenuItems(defaultMenu, &data);

    data.node = r.innerNonSharedNode();

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
