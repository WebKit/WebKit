/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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

#include "AppHighlight.h"
#include "BarcodeDetectorInterface.h"
#include "ChromeClient.h"
#include "ContactInfo.h"
#include "ContactsRequestData.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentType.h"
#include "FaceDetectorInterface.h"
#include "FileList.h"
#include "FloatRect.h"
#include "FrameTree.h"
#include "Geolocation.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "InspectorInstrumentation.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Page.h"
#include "PageGroupLoadDeferrer.h"
#include "PopupOpeningObserver.h"
#include "RenderObject.h"
#include "Settings.h"
#include "ShareData.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "TextDetectorInterface.h"
#include "WebGPU.h"
#include "WindowFeatures.h"
#include "WorkerClient.h"
#include <JavaScriptCore/VM.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#if ENABLE(DATALIST_ELEMENT)
#include "DataListSuggestionPicker.h"
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
#include "DateTimeChooser.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Chrome);

using namespace HTMLNames;

Chrome::Chrome(Page& page, UniqueRef<ChromeClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

Chrome::~Chrome()
{
    m_client->chromeDestroyed();
}

Ref<Page> Chrome::protectedPage() const
{
    return m_page.get();
}

void Chrome::invalidateRootView(const IntRect& updateRect)
{
    m_client->invalidateRootView(updateRect);
}

void Chrome::invalidateContentsAndRootView(const IntRect& updateRect)
{
    m_client->invalidateContentsAndRootView(updateRect);
}

void Chrome::invalidateContentsForSlowScroll(const IntRect& updateRect)
{
    m_client->invalidateContentsForSlowScroll(updateRect);
}

void Chrome::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    m_client->scroll(scrollDelta, rectToScroll, clipRect);
    InspectorInstrumentation::didScroll(protectedPage());
}

IntPoint Chrome::screenToRootView(const IntPoint& point) const
{
    return m_client->screenToRootView(point);
}

IntRect Chrome::rootViewToScreen(const IntRect& rect) const
{
    return m_client->rootViewToScreen(rect);
}
    
IntPoint Chrome::accessibilityScreenToRootView(const IntPoint& point) const
{
    return m_client->accessibilityScreenToRootView(point);
}

IntRect Chrome::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return m_client->rootViewToAccessibilityScreen(rect);
}

#if PLATFORM(IOS_FAMILY)
void Chrome::relayAccessibilityNotification(const String& notificationName, const RetainPtr<NSData>& notificationData) const
{
    return m_client->relayAccessibilityNotification(notificationName, notificationData);
}
#endif

PlatformPageClient Chrome::platformPageClient() const
{
    return m_client->platformPageClient();
}

void Chrome::contentsSizeChanged(LocalFrame& frame, const IntSize& size) const
{
    m_client->contentsSizeChanged(frame, size);
}

void Chrome::scrollContainingScrollViewsToRevealRect(const IntRect& rect) const
{
    m_client->scrollContainingScrollViewsToRevealRect(rect);
}

void Chrome::scrollMainFrameToRevealRect(const IntRect& rect) const
{
    m_client->scrollMainFrameToRevealRect(rect);
}

void Chrome::setWindowRect(const FloatRect& rect)
{
    m_client->setWindowRect(rect);
}

FloatRect Chrome::windowRect() const
{
    return m_client->windowRect();
}

FloatRect Chrome::pageRect() const
{
    return m_client->pageRect();
}

void Chrome::focus()
{
    m_client->focus();
}

void Chrome::unfocus()
{
    m_client->unfocus();
}

bool Chrome::canTakeFocus(FocusDirection direction) const
{
    return m_client->canTakeFocus(direction);
}

void Chrome::takeFocus(FocusDirection direction)
{
    m_client->takeFocus(direction);
}

void Chrome::focusedElementChanged(Element* element)
{
    m_client->focusedElementChanged(element);
}

void Chrome::focusedFrameChanged(Frame* frame)
{
    m_client->focusedFrameChanged(frame);
}

RefPtr<Page> Chrome::createWindow(LocalFrame& frame, const String& openedMainFrameName, const WindowFeatures& features, const NavigationAction& action)
{
    RefPtr newPage = m_client->createWindow(frame, openedMainFrameName, features, action);
    if (!newPage)
        return nullptr;

    return newPage;
}

void Chrome::show()
{
    m_client->show();
}

bool Chrome::canRunModal() const
{
    return m_client->canRunModal();
}

void Chrome::runModal()
{
    // Defer callbacks in all the other pages in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    PageGroupLoadDeferrer deferrer(m_page, false);

    // JavaScript that runs within the nested event loop must not be run in the context of the
    // script that called showModalDialog. Null out entryScope to break the connection.

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;

    SetForScope entryScopeNullifier { localMainFrame->document()->vm().entryScope, nullptr };

    TimerBase::fireTimersInNestedEventLoop();
    m_client->runModal();
}

void Chrome::setToolbarsVisible(bool b)
{
    m_client->setToolbarsVisible(b);
}

bool Chrome::toolbarsVisible() const
{
    return m_client->toolbarsVisible();
}

void Chrome::setStatusbarVisible(bool b)
{
    m_client->setStatusbarVisible(b);
}

bool Chrome::statusbarVisible() const
{
    return m_client->statusbarVisible();
}

void Chrome::setScrollbarsVisible(bool b)
{
    m_client->setScrollbarsVisible(b);
}

bool Chrome::scrollbarsVisible() const
{
    return m_client->scrollbarsVisible();
}

void Chrome::setMenubarVisible(bool b)
{
    m_client->setMenubarVisible(b);
}

bool Chrome::menubarVisible() const
{
    return m_client->menubarVisible();
}

void Chrome::setResizable(bool b)
{
    m_client->setResizable(b);
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, LocalFrame& frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    return m_client->runBeforeUnloadConfirmPanel(message, frame);
}

void Chrome::closeWindow()
{
    m_client->closeWindow();
}

void Chrome::runJavaScriptAlert(LocalFrame& frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    notifyPopupOpeningObservers();
    String displayMessage = frame.displayStringModifiedByEncoding(message);

    m_client->runJavaScriptAlert(frame, displayMessage);
}

bool Chrome::runJavaScriptConfirm(LocalFrame& frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(protectedPage(), true);

    notifyPopupOpeningObservers();
    return m_client->runJavaScriptConfirm(frame, frame.displayStringModifiedByEncoding(message));
}

bool Chrome::runJavaScriptPrompt(LocalFrame& frame, const String& prompt, const String& defaultValue, String& result)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(protectedPage(), true);

    notifyPopupOpeningObservers();
    String displayPrompt = frame.displayStringModifiedByEncoding(prompt);

    bool ok = m_client->runJavaScriptPrompt(frame, displayPrompt, frame.displayStringModifiedByEncoding(defaultValue), result);
    if (ok)
        result = frame.displayStringModifiedByEncoding(result);

    return ok;
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result, OptionSet<PlatformEventModifier> modifiers)
{
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame())) {
        if (result.innerNode() && result.innerNode()->document().isDNSPrefetchEnabled())
            localMainFrame->protectedLoader()->client().prefetchDNS(result.absoluteLinkURL().host().toString());
    }

    String toolTip;
    TextDirection toolTipDirection;
    getToolTip(result, toolTip, toolTipDirection);
    m_client->mouseDidMoveOverElement(result, modifiers, toolTip, toolTipDirection);

    InspectorInstrumentation::mouseDidMoveOverElement(protectedPage(), result, modifiers);
}

void Chrome::getToolTip(const HitTestResult& result, String& toolTip, TextDirection& toolTipDirection)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    toolTip = result.spellingToolTip(toolTipDirection);

    // Next priority is a toolTip from a URL beneath the mouse (if preference is set to show those).
    if (toolTip.isEmpty() && m_page->settings().showsURLsInToolTips()) {
        if (RefPtr element = result.innerNonSharedElement()) {
            // Get tooltip representing form action, if relevant
            if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element)) {
                if (input->isSubmitButton()) {
                    if (RefPtr form = input->form()) {
                        toolTip = form->action();
                        if (form->renderer())
                            toolTipDirection = form->renderer()->writingMode().computedTextDirection();
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

    if (toolTip.isEmpty() && m_page->settings().showsToolTipOverTruncatedText())
        toolTip = result.innerTextIfTruncated(toolTipDirection);

    // Lastly, for <input type="file"> that allow multiple files, we'll consider a tooltip for the selected filenames
    if (toolTip.isEmpty()) {
        if (RefPtr element = result.innerNonSharedElement()) {
            if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element)) {
                toolTip = input->defaultToolTip();

                // FIXME: We should obtain text direction of tooltip from
                // ChromeClient or platform. As of October 2011, all client
                // implementations don't use text direction information for
                // ChromeClient::mouseDidMoveOverElement. We'll work on tooltip text
                // direction during bidi cleanup in form inputs.
                toolTipDirection = TextDirection::LTR;
            }
        }
    }
}

bool Chrome::print(LocalFrame& frame)
{
    // FIXME: This should have PageGroupLoadDeferrer, like runModal() or runJavaScriptAlert(), because it's no different from those.

    if (frame.document()->isSandboxed(SandboxFlag::Modals)) {
        frame.document()->protectedWindow()->printErrorMessage("Use of window.print is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return false;
    }

    m_client->print(frame, frame.document()->titleWithDirection());
    return true;
}

void Chrome::enableSuddenTermination()
{
    m_client->enableSuddenTermination();
}

void Chrome::disableSuddenTermination()
{
    m_client->disableSuddenTermination();
}

#if ENABLE(INPUT_TYPE_COLOR)

RefPtr<ColorChooser> Chrome::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(client);
    UNUSED_PARAM(initialColor);
    return nullptr;
#else
    notifyPopupOpeningObservers();
    return m_client->createColorChooser(client, initialColor);
#endif
}

#endif

#if ENABLE(DATALIST_ELEMENT)

RefPtr<DataListSuggestionPicker> Chrome::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    notifyPopupOpeningObservers();
    return m_client->createDataListSuggestionPicker(client);
}

#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

RefPtr<DateTimeChooser> Chrome::createDateTimeChooser(DateTimeChooserClient& client)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(client);
    return nullptr;
#else
    notifyPopupOpeningObservers();
    return m_client->createDateTimeChooser(client);
#endif
}

#endif

void Chrome::runOpenPanel(LocalFrame& frame, FileChooser& fileChooser)
{
    notifyPopupOpeningObservers();
    m_client->runOpenPanel(frame, fileChooser);
}

void Chrome::showShareSheet(ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& callback)
{
    m_client->showShareSheet(shareData, WTFMove(callback));
}

void Chrome::showContactPicker(const ContactsRequestData& requestData, CompletionHandler<void(std::optional<Vector<ContactInfo>>&&)>&& callback)
{
    m_client->showContactPicker(requestData, WTFMove(callback));
}

void Chrome::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    m_client->loadIconForFiles(filenames, loader);
}

FloatSize Chrome::screenSize() const
{
    return m_client->screenSize();
}

FloatSize Chrome::availableScreenSize() const
{
    return m_client->availableScreenSize();
}

FloatSize Chrome::overrideScreenSize() const
{
    return m_client->overrideScreenSize();
}

FloatSize Chrome::overrideAvailableScreenSize() const
{
    return m_client->overrideAvailableScreenSize();
}

void Chrome::dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations) const
{
    m_client->dispatchDisabledAdaptationsDidChange(disabledAdaptations);
}

void Chrome::dispatchViewportPropertiesDidChange(const ViewportArguments& arguments) const
{
#if PLATFORM(IOS_FAMILY)
    if (m_isDispatchViewportDataDidChangeSuppressed)
        return;
#endif
    m_client->dispatchViewportPropertiesDidChange(arguments);
}

void Chrome::setCursor(const Cursor& cursor)
{
    m_client->setCursor(cursor);
}

void Chrome::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    m_client->setCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves);
}

RefPtr<ImageBuffer> Chrome::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, OptionSet<ImageBufferOptions> options) const
{
    return m_client->createImageBuffer(size, purpose, resolutionScale, colorSpace, pixelFormat, options);
}

RefPtr<ImageBuffer> Chrome::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    return m_client->sinkIntoImageBuffer(WTFMove(imageBuffer));
}

std::unique_ptr<WorkerClient> Chrome::createWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    return m_client->createWorkerClient(dispatcher);
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> Chrome::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
    return m_client->createGraphicsContextGL(attributes);
}
#endif
#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebGPU::GPU> Chrome::createGPUForWebGPU() const
{
    return m_client->createGPUForWebGPU();
}
#endif

RefPtr<ShapeDetection::BarcodeDetector> Chrome::createBarcodeDetector(const ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions) const
{
    return m_client->createBarcodeDetector(barcodeDetectorOptions);
}

void Chrome::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
    return m_client->getBarcodeDetectorSupportedFormats(WTFMove(completionHandler));
}

RefPtr<ShapeDetection::FaceDetector> Chrome::createFaceDetector(const ShapeDetection::FaceDetectorOptions& faceDetectorOptions) const
{
    return m_client->createFaceDetector(faceDetectorOptions);
}

RefPtr<ShapeDetection::TextDetector> Chrome::createTextDetector() const
{
    return m_client->createTextDetector();
}

PlatformDisplayID Chrome::displayID() const
{
    return m_page->displayID();
}

void Chrome::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFrameInterval)
{
    protectedPage()->windowScreenDidChange(displayID, nominalFrameInterval);
}

bool Chrome::selectItemWritingDirectionIsNatural()
{
    return m_client->selectItemWritingDirectionIsNatural();
}

bool Chrome::selectItemAlignmentFollowsMenuWritingDirection()
{
    return m_client->selectItemAlignmentFollowsMenuWritingDirection();
}

RefPtr<PopupMenu> Chrome::createPopupMenu(PopupMenuClient& client) const
{
    notifyPopupOpeningObservers();
    return m_client->createPopupMenu(client);
}

RefPtr<SearchPopupMenu> Chrome::createSearchPopupMenu(PopupMenuClient& client) const
{
    notifyPopupOpeningObservers();
    return m_client->createSearchPopupMenu(client);
}

bool Chrome::requiresFullscreenForVideoPlayback()
{
    return m_client->requiresFullscreenForVideoPlayback();
}

void Chrome::didReceiveDocType(LocalFrame& frame)
{
#if !PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(frame);
#else
    if (!frame.isMainFrame())
        return;

    auto* doctype = frame.document()->doctype();
    m_client->didReceiveMobileDocType(doctype && doctype->publicId().containsIgnoringASCIICase("xhtml mobile"_s));
#endif
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver& observer)
{
    m_popupOpeningObservers.append(observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver& observer)
{
    bool removed = m_popupOpeningObservers.removeFirst(WeakPtr { observer });
    ASSERT_UNUSED(removed, removed);
}

void Chrome::notifyPopupOpeningObservers() const
{
    auto observers = m_popupOpeningObservers;
    for (auto& observer : observers)
        observer->willOpenPopup();
}

} // namespace WebCore
