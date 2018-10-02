/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Chrome.h"

#include "ChromeClient.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentType.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "FileList.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "Geolocation.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "PageGroupLoadDeferrer.h"
#include "PopupOpeningObserver.h"
#include "RenderObject.h"
#include "ResourceHandle.h"
#include "Settings.h"
#include "ShareData.h"
#include "StorageNamespace.h"
#include "WindowFeatures.h"
#include <JavaScriptCore/VM.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(DATALIST_ELEMENT)
#include "DataListSuggestionPicker.h"
#endif

#if PLATFORM(MAC) && ENABLE(GRAPHICS_CONTEXT_3D)
#include "GraphicsContext3DManager.h"
#endif

namespace WebCore {

using namespace HTMLNames;

Chrome::Chrome(Page& page, ChromeClient& client)
    : m_page(page)
    , m_client(client)
{
}

Chrome::~Chrome()
{
    m_client.chromeDestroyed();
}

void Chrome::invalidateRootView(const IntRect& updateRect)
{
    m_client.invalidateRootView(updateRect);
}

void Chrome::invalidateContentsAndRootView(const IntRect& updateRect)
{
    m_client.invalidateContentsAndRootView(updateRect);
}

void Chrome::invalidateContentsForSlowScroll(const IntRect& updateRect)
{
    m_client.invalidateContentsForSlowScroll(updateRect);
}

void Chrome::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    m_client.scroll(scrollDelta, rectToScroll, clipRect);
    InspectorInstrumentation::didScroll(m_page);
}

IntPoint Chrome::screenToRootView(const IntPoint& point) const
{
    return m_client.screenToRootView(point);
}

IntRect Chrome::rootViewToScreen(const IntRect& rect) const
{
    return m_client.rootViewToScreen(rect);
}
    
#if PLATFORM(IOS)

IntPoint Chrome::accessibilityScreenToRootView(const IntPoint& point) const
{
    return m_client.accessibilityScreenToRootView(point);
}

IntRect Chrome::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return m_client.rootViewToAccessibilityScreen(rect);
}

#endif

PlatformPageClient Chrome::platformPageClient() const
{
    return m_client.platformPageClient();
}

void Chrome::contentsSizeChanged(Frame& frame, const IntSize& size) const
{
    m_client.contentsSizeChanged(frame, size);
}

void Chrome::scrollRectIntoView(const IntRect& rect) const
{
    m_client.scrollRectIntoView(rect);
}

void Chrome::setWindowRect(const FloatRect& rect) const
{
    m_client.setWindowRect(rect);
}

FloatRect Chrome::windowRect() const
{
    return m_client.windowRect();
}

FloatRect Chrome::pageRect() const
{
    return m_client.pageRect();
}

void Chrome::focus() const
{
    m_client.focus();
}

void Chrome::unfocus() const
{
    m_client.unfocus();
}

bool Chrome::canTakeFocus(FocusDirection direction) const
{
    return m_client.canTakeFocus(direction);
}

void Chrome::takeFocus(FocusDirection direction) const
{
    m_client.takeFocus(direction);
}

void Chrome::focusedElementChanged(Element* element) const
{
    m_client.focusedElementChanged(element);
}

void Chrome::focusedFrameChanged(Frame* frame) const
{
    m_client.focusedFrameChanged(frame);
}

Page* Chrome::createWindow(Frame& frame, const FrameLoadRequest& request, const WindowFeatures& features, const NavigationAction& action) const
{
    Page* newPage = m_client.createWindow(frame, request, features, action);
    if (!newPage)
        return nullptr;

    if (auto* oldSessionStorage = m_page.sessionStorage(false))
        newPage->setSessionStorage(oldSessionStorage->copy(newPage));
    if (auto* oldEphemeralLocalStorage = m_page.ephemeralLocalStorage(false))
        newPage->setEphemeralLocalStorage(oldEphemeralLocalStorage->copy(newPage));

    return newPage;
}

void Chrome::show() const
{
    m_client.show();
}

bool Chrome::canRunModal() const
{
    return m_client.canRunModal();
}

void Chrome::runModal() const
{
    // Defer callbacks in all the other pages in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    PageGroupLoadDeferrer deferrer(m_page, false);

    // JavaScript that runs within the nested event loop must not be run in the context of the
    // script that called showModalDialog. Null out entryScope to break the connection.
    SetForScope<JSC::VMEntryScope*> entryScopeNullifier { m_page.mainFrame().document()->vm().entryScope, nullptr };

    TimerBase::fireTimersInNestedEventLoop();
    m_client.runModal();
}

void Chrome::setToolbarsVisible(bool b) const
{
    m_client.setToolbarsVisible(b);
}

bool Chrome::toolbarsVisible() const
{
    return m_client.toolbarsVisible();
}

void Chrome::setStatusbarVisible(bool b) const
{
    m_client.setStatusbarVisible(b);
}

bool Chrome::statusbarVisible() const
{
    return m_client.statusbarVisible();
}

void Chrome::setScrollbarsVisible(bool b) const
{
    m_client.setScrollbarsVisible(b);
}

bool Chrome::scrollbarsVisible() const
{
    return m_client.scrollbarsVisible();
}

void Chrome::setMenubarVisible(bool b) const
{
    m_client.setMenubarVisible(b);
}

bool Chrome::menubarVisible() const
{
    return m_client.menubarVisible();
}

void Chrome::setResizable(bool b) const
{
    m_client.setResizable(b);
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client.canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, Frame& frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    return m_client.runBeforeUnloadConfirmPanel(message, frame);
}

void Chrome::closeWindowSoon()
{
    m_client.closeWindowSoon();
}

void Chrome::runJavaScriptAlert(Frame& frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    notifyPopupOpeningObservers();
    String displayMessage = frame.displayStringModifiedByEncoding(message);

    m_client.runJavaScriptAlert(frame, displayMessage);
}

bool Chrome::runJavaScriptConfirm(Frame& frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    notifyPopupOpeningObservers();
    return m_client.runJavaScriptConfirm(frame, frame.displayStringModifiedByEncoding(message));
}

bool Chrome::runJavaScriptPrompt(Frame& frame, const String& prompt, const String& defaultValue, String& result)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    notifyPopupOpeningObservers();
    String displayPrompt = frame.displayStringModifiedByEncoding(prompt);

    bool ok = m_client.runJavaScriptPrompt(frame, displayPrompt, frame.displayStringModifiedByEncoding(defaultValue), result);
    if (ok)
        result = frame.displayStringModifiedByEncoding(result);

    return ok;
}

void Chrome::setStatusbarText(Frame& frame, const String& status)
{
    m_client.setStatusbarText(frame.displayStringModifiedByEncoding(status));
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    if (result.innerNode() && result.innerNode()->document().isDNSPrefetchEnabled())
        m_page.mainFrame().loader().client().prefetchDNS(result.absoluteLinkURL().host().toString());
    m_client.mouseDidMoveOverElement(result, modifierFlags);

    InspectorInstrumentation::mouseDidMoveOverElement(m_page, result, modifierFlags);
}

void Chrome::setToolTip(const HitTestResult& result)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    TextDirection toolTipDirection;
    String toolTip = result.spellingToolTip(toolTipDirection);

    // Next priority is a toolTip from a URL beneath the mouse (if preference is set to show those).
    if (toolTip.isEmpty() && m_page.settings().showsURLsInToolTips()) {
        if (Element* element = result.innerNonSharedElement()) {
            // Get tooltip representing form action, if relevant
            if (is<HTMLInputElement>(*element)) {
                HTMLInputElement& input = downcast<HTMLInputElement>(*element);
                if (input.isSubmitButton()) {
                    if (HTMLFormElement* form = input.form()) {
                        toolTip = form->action();
                        if (form->renderer())
                            toolTipDirection = form->renderer()->style().direction();
                        else
                            toolTipDirection = TextDirection::LTR;
                    }
                }
            }
        }

        // Get tooltip representing link's URL
        if (toolTip.isEmpty()) {
            // FIXME: Need to pass this URL through userVisibleString once that's in WebCore
            toolTip = result.absoluteLinkURL().string();
            // URL always display as LTR.
            toolTipDirection = TextDirection::LTR;
        }
    }

    // Next we'll consider a tooltip for element with "title" attribute
    if (toolTip.isEmpty())
        toolTip = result.title(toolTipDirection);

    if (toolTip.isEmpty() && m_page.settings().showsToolTipOverTruncatedText())
        toolTip = result.innerTextIfTruncated(toolTipDirection);

    // Lastly, for <input type="file"> that allow multiple files, we'll consider a tooltip for the selected filenames
    if (toolTip.isEmpty()) {
        if (Element* element = result.innerNonSharedElement()) {
            if (is<HTMLInputElement>(*element)) {
                toolTip = downcast<HTMLInputElement>(*element).defaultToolTip();

                // FIXME: We should obtain text direction of tooltip from
                // ChromeClient or platform. As of October 2011, all client
                // implementations don't use text direction information for
                // ChromeClient::setToolTip. We'll work on tooltip text
                // direction during bidi cleanup in form inputs.
                toolTipDirection = TextDirection::LTR;
            }
        }
    }

    m_client.setToolTip(toolTip, toolTipDirection);
}

bool Chrome::print(Frame& frame)
{
    // FIXME: This should have PageGroupLoadDeferrer, like runModal() or runJavaScriptAlert(), because it's no different from those.

    if (frame.document()->isSandboxed(SandboxModals)) {
        frame.document()->domWindow()->printErrorMessage("Use of window.print is not allowed in a sandboxed frame when the allow-modals flag is not set.");
        return false;
    }

    m_client.print(frame);
    return true;
}

void Chrome::enableSuddenTermination()
{
    m_client.enableSuddenTermination();
}

void Chrome::disableSuddenTermination()
{
    m_client.disableSuddenTermination();
}

#if ENABLE(INPUT_TYPE_COLOR)

std::unique_ptr<ColorChooser> Chrome::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
#if PLATFORM(IOS)
    return nullptr;
#endif
    notifyPopupOpeningObservers();
    return m_client.createColorChooser(client, initialColor);
}

#endif

#if ENABLE(DATALIST_ELEMENT)

std::unique_ptr<DataListSuggestionPicker> Chrome::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    notifyPopupOpeningObservers();
    return m_client.createDataListSuggestionPicker(client);
}

#endif

void Chrome::runOpenPanel(Frame& frame, FileChooser& fileChooser)
{
    notifyPopupOpeningObservers();
    m_client.runOpenPanel(frame, fileChooser);
}

void Chrome::showShareSheet(ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& callback)
{
    m_client.showShareSheet(shareData, WTFMove(callback));
}

void Chrome::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    m_client.loadIconForFiles(filenames, loader);
}

FloatSize Chrome::screenSize() const
{
    return m_client.screenSize();
}

FloatSize Chrome::availableScreenSize() const
{
    return m_client.availableScreenSize();
}

FloatSize Chrome::overrideScreenSize() const
{
    return m_client.overrideScreenSize();
}

void Chrome::dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations) const
{
    m_client.dispatchDisabledAdaptationsDidChange(disabledAdaptations);
}

void Chrome::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
#if PLATFORM(IOS)
    if (m_isDispatchViewportDataDidChangeSuppressed)
        return;
#endif
    m_client.dispatchViewportPropertiesDidChange(arguments);
}

void Chrome::setCursor(const Cursor& cursor)
{
#if ENABLE(CURSOR_SUPPORT)
    m_client.setCursor(cursor);
#else
    UNUSED_PARAM(cursor);
#endif
}

void Chrome::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
#if ENABLE(CURSOR_SUPPORT)
    m_client.setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
#else
    UNUSED_PARAM(hiddenUntilMouseMoves);
#endif
}

PlatformDisplayID Chrome::displayID() const
{
    return m_displayID;
}

void Chrome::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (displayID == m_displayID)
        return;

    m_displayID = displayID;

    for (Frame* frame = &m_page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document())
            frame->document()->windowScreenDidChange(displayID);
    }

#if PLATFORM(MAC) && ENABLE(GRAPHICS_CONTEXT_3D)
    GraphicsContext3DManager::sharedManager().screenDidChange(displayID, this);
#endif
}

#if ENABLE(DASHBOARD_SUPPORT)
void ChromeClient::annotatedRegionsChanged()
{
}
#endif

bool ChromeClient::shouldReplaceWithGeneratedFileForUpload(const String&, String&)
{
    return false;
}

String ChromeClient::generateReplacementFile(const String&)
{
    ASSERT_NOT_REACHED();
    return String();
}

bool Chrome::selectItemWritingDirectionIsNatural()
{
    return m_client.selectItemWritingDirectionIsNatural();
}

bool Chrome::selectItemAlignmentFollowsMenuWritingDirection()
{
    return m_client.selectItemAlignmentFollowsMenuWritingDirection();
}

RefPtr<PopupMenu> Chrome::createPopupMenu(PopupMenuClient& client) const
{
    notifyPopupOpeningObservers();
    return m_client.createPopupMenu(client);
}

RefPtr<SearchPopupMenu> Chrome::createSearchPopupMenu(PopupMenuClient& client) const
{
    notifyPopupOpeningObservers();
    return m_client.createSearchPopupMenu(client);
}

bool Chrome::requiresFullscreenForVideoPlayback()
{
    return m_client.requiresFullscreenForVideoPlayback();
}

void Chrome::didReceiveDocType(Frame& frame)
{
#if !PLATFORM(IOS)
    UNUSED_PARAM(frame);
#else
    if (!frame.isMainFrame())
        return;

    auto* doctype = frame.document()->doctype();
    m_client.didReceiveMobileDocType(doctype && doctype->publicId().containsIgnoringASCIICase("xhtml mobile"));
#endif
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver& observer)
{
    m_popupOpeningObservers.append(&observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver& observer)
{
    bool removed = m_popupOpeningObservers.removeFirst(&observer);
    ASSERT_UNUSED(removed, removed);
}

void Chrome::notifyPopupOpeningObservers() const
{
    const Vector<PopupOpeningObserver*> observers(m_popupOpeningObservers);
    for (auto& observer : observers)
        observer->willOpenPopup();
}

} // namespace WebCore
