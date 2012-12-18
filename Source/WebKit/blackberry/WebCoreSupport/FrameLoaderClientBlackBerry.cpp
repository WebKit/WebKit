/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "FrameLoaderClientBlackBerry.h"

#include "AboutData.h"
#include "AutofillManager.h"
#include "BackForwardController.h"
#include "BackForwardListImpl.h"
#include "BackingStoreClient.h"
#include "BackingStore_p.h"
#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "ClientExtension.h"
#include "CookieManager.h"
#include "CredentialBackingStore.h"
#include "CredentialManager.h"
#include "CredentialTransformData.h"
#include "DumpRenderTreeClient.h"
#include "ExternalExtension.h"
#include "FrameLoadRequest.h"
#include "FrameNetworkingContextBlackBerry.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "IconDatabase.h"
#include "Image.h"
#include "InputHandler.h"
#include "MIMETypeRegistry.h"
#include "MainResourceLoader.h"
#include "NativeImageSkia.h"
#include "NetworkManager.h"
#include "NodeList.h"
#include "Page.h"
#include "PluginDatabase.h"
#include "PluginView.h"
#include "ProgressTracker.h"
#include "ProtectionSpace.h"
#include "ScopePointer.h"
#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
#include "Settings.h"
#endif
#include "SharedBuffer.h"
#include "SkData.h"
#include "SkImageEncoder.h"
#include "SkStream.h"
#include "TextEncoding.h"
#include "TouchEventHandler.h"
#if ENABLE(WEBDOM)
#include "WebDOMDocument.h"
#endif
#include "WebPageClient.h"

#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformScreen.h>
#include <JavaScriptCore/APICast.h>
#include <network/FilterStream.h>
#include <network/NetworkRequest.h>
#include <wtf/text/Base64.h>

using WTF::String;
using namespace WebCore;
using namespace BlackBerry::WebKit;
using BlackBerry::Platform::NetworkRequest;

// This was copied from file "WebKit/Source/WebKit/mac/Misc/WebKitErrors.h".
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
    WebKitErrorPluginWillHandleLoad =                           203
};

namespace WebCore {

FrameLoaderClientBlackBerry::FrameLoaderClientBlackBerry()
    : m_frame(0)
    , m_webPagePrivate(0)
    , m_sentReadyToRender(false)
    , m_pendingFragmentScrollPolicyFunction(0)
    , m_loadingErrorPage(false)
    , m_clientRedirectIsPending(false)
    , m_childFrameCreationSuppressed(false)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_cancelLoadOnNextData(false)
    , m_wasProvisionalLoadTriggeredByUserGesture(true) // To avoid affecting the first load.
{
}

FrameLoaderClientBlackBerry::~FrameLoaderClientBlackBerry()
{
}

int FrameLoaderClientBlackBerry::playerId() const
{
    return m_webPagePrivate ? m_webPagePrivate->playerID() : 0;
}

bool FrameLoaderClientBlackBerry::cookiesEnabled() const
{
    return m_webPagePrivate->m_webSettings->areCookiesEnabled();
}

void FrameLoaderClientBlackBerry::dispatchDidAddBackForwardItem(HistoryItem* item) const
{
    // Inform the client that the back/forward list has changed.
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidRemoveBackForwardItem(HistoryItem* item) const
{
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidChangeBackForwardIndex() const
{
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidChangeLocationWithinPage()
{
    if (!isMainFrame())
        return;

    String url = m_frame->document()->url().string();
    String token = m_frame->loader()->documentLoader()->request().token();

    m_webPagePrivate->m_client->notifyLoadToAnchor(url.characters(), url.length(), token.characters(), token.length());
}

void FrameLoaderClientBlackBerry::dispatchDidPushStateWithinPage()
{
    // FIXME: As a workaround we abuse anchor navigation to achieve history push. See PR #119779 for more details.
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidReplaceStateWithinPage()
{
    // FIXME: As a workaround we abuse anchor navigation to achieve history replace. See PR #119779 for more details.
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidPopStateWithinPage()
{
    // Not needed.
}

void FrameLoaderClientBlackBerry::dispatchDidCancelClientRedirect()
{
    m_clientRedirectIsPending = false;
}

void FrameLoaderClientBlackBerry::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didDispatchWillPerformClientRedirect();

    m_clientRedirectIsPending = true;
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForResponse(FramePolicyFunction function, const ResourceResponse& response, const ResourceRequest& request)
{
    // FIXME: What should we do for HTTP status code 204 and 205 and "application/zip"?
    PolicyAction policy = PolicyIgnore;

    if (contentDispositionType(response.httpHeaderField("Content-Disposition")) == ContentDispositionAttachment
        || request.forceDownload())
        policy = PolicyDownload;
    else if (canShowMIMEType(response.mimeType()))
        policy = PolicyUse;
    else if (ResourceRequest::TargetIsMainFrame == request.targetType() && m_webPagePrivate->m_client->downloadAllowed(request.url().string()))
        policy = PolicyDownload;

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didDecidePolicyForResponse(response);

    (m_frame->loader()->policyChecker()->*function)(policy);
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState>)
{
    PolicyAction decision = PolicyIgnore;

    const KURL& url = request.url();
    if (!url.isNull()) {
        // Fragment scrolls on the same page should always be handled internally.
        // (Only count as a fragment scroll if we are scrolling to a #fragment url, not back to the top, and reloading
        // the same url is not a fragment scroll even if it has a #fragment.)
        const KURL& currentUrl = m_frame->document()->url();
        bool isFragmentScroll = url.hasFragmentIdentifier() && url != currentUrl && equalIgnoringFragmentIdentifier(currentUrl, url);
        decision = decidePolicyForExternalLoad(request, isFragmentScroll);

        // Let the client have a chance to say whether this navigation should be ignored or not.
        NetworkRequest platformRequest;
        request.initializePlatformRequest(platformRequest, cookiesEnabled());
        if (!platformRequest.getUrlRef().empty()) { // Some invalid URLs will result in empty URL in platformRequest
            if (platformRequest.getTargetType() == NetworkRequest::TargetIsUnknown)
                platformRequest.setTargetType(isMainFrame() ? NetworkRequest::TargetIsMainFrame : NetworkRequest::TargetIsSubframe);

            if (!m_webPagePrivate->m_client->acceptNavigationRequest(platformRequest, BlackBerry::Platform::NavigationType(action.type()))) {
                decision = PolicyIgnore;
                if (isMainFrame()) {
                    if (action.type() == NavigationTypeFormSubmitted || action.type() == NavigationTypeFormResubmitted)
                        m_frame->loader()->resetMultipleFormSubmissionProtection();

                    if (action.type() == NavigationTypeLinkClicked && url.hasFragmentIdentifier()) {
                        ResourceRequest emptyRequest;
                        m_frame->loader()->activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
                    }
                }
            }
        }
    }

    // If we abort here, dispatchDidCancelClientRedirect will not be called.
    // So call it by hand.
    if (decision == PolicyIgnore)
        dispatchDidCancelClientRedirect();

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didDecidePolicyForNavigationAction(action, request, m_frame);

    (m_frame->loader()->policyChecker()->*function)(decision);
}

void FrameLoaderClientBlackBerry::delayPolicyCheckUntilFragmentExists(const String& fragment, FramePolicyFunction function)
{
    ASSERT(isMainFrame());

    if (m_webPagePrivate->loadState() < WebPagePrivate::Finished && !m_frame->document()->findAnchor(fragment)) {
        // Tell the client we need more data, in case the fragment exists but is being held back.
        m_webPagePrivate->m_client->needMoreData();
        m_pendingFragmentScrollPolicyFunction = function;
        m_pendingFragmentScroll = fragment;
        return;
    }

    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientBlackBerry::cancelPolicyCheck()
{
    m_pendingFragmentScrollPolicyFunction = 0;
    m_pendingFragmentScroll = String();
}

void FrameLoaderClientBlackBerry::doPendingFragmentScroll()
{
    if (m_pendingFragmentScroll.isNull())
        return;

    // Make sure to clear the pending members first to avoid recursion.
    String fragment = m_pendingFragmentScroll;
    m_pendingFragmentScroll = String();

    FramePolicyFunction function = m_pendingFragmentScrollPolicyFunction;
    m_pendingFragmentScrollPolicyFunction = 0;

    delayPolicyCheckUntilFragmentExists(fragment, function);
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction&, const ResourceRequest& request, PassRefPtr<FormState>, const String& frameName)
{
    if (ScriptController::processingUserGesture() && !m_webPagePrivate->m_pluginMayOpenNewTab) {
        (m_frame->loader()->policyChecker()->*function)(PolicyIgnore);
        return;
    }

    // A new window can never be a fragment scroll.
    PolicyAction decision = decidePolicyForExternalLoad(request, false);
    (m_frame->loader()->policyChecker()->*function)(decision);
}

void FrameLoaderClientBlackBerry::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // The structure of this code may seem...a bit odd. It's structured with two checks on the state
    // of m_pluginView because it's actually receivedData that may cause the request to re-direct data
    // to a PluginView. This is because receivedData may decide to create a PluginDocument containing
    // a PluginView. The PluginView will request that the main resource for the frame be redirected
    // to the PluginView. So after receivedData has been called, this code needs to check whether
    // re-direction to a PluginView has been requested and pass the same data on to the PluginView.
    // Thereafter, all data will be re-directed to the PluginView; i.e., no additional data will go
    // to receivedData.

    if (!m_pluginView) {
        const String& textEncoding = loader->response().textEncodingName();
        receivedData(data, length, textEncoding);
    }

    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }

        if (!m_pluginView)
            return;

        m_pluginView->didReceiveData(data, length);
    }
}

PassRefPtr<Widget> FrameLoaderClientBlackBerry::createPlugin(const IntSize& pluginSize,
    HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames,
    const Vector<String>& paramValues, const String& mimeTypeIn, bool loadManually)
{
    String mimeType(mimeTypeIn);
    if (mimeType.isEmpty()) {
        mimeType = MIMETypeRegistry::getMIMETypeForPath(url.path());
        mimeType = MIMETypeRegistry::getNormalizedMIMEType(mimeType);
        if (mimeType != "application/x-shockwave-flash")
            mimeType = mimeTypeIn;
    }

    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return PluginView::create(m_frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    // This is not a plugin type that is currently supported or enabled. Try
    // to load the url directly. This check is performed to allow video and
    // audio to be referenced as the source of embed or object elements.
    // For media of this kind the mime type passed into this function
    // will generally be a valid media mime type, or it may be null. We
    // explicitly check for Flash content so it does not get rendered as
    // text at this point, producing garbled characters.
    if (mimeType != "application/x-shockwave-flash" && m_frame->loader() && m_frame->loader()->subframeLoader() && !url.isNull())
        m_frame->loader()->subframeLoader()->requestFrame(element, url, String());

    return 0;
}

void FrameLoaderClientBlackBerry::redirectDataToPlugin(Widget* pluginWidget)
{
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    if (pluginWidget)
        m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientBlackBerry::receivedData(const char* data, int length, const String& textEncoding)
{
    if (!m_frame)
        return;

    if (m_cancelLoadOnNextData) {
        m_frame->loader()->activeDocumentLoader()->stopLoading();
        m_frame->loader()->documentLoader()->writer()->end();
        m_cancelLoadOnNextData = false;
        return;
    }

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = m_frame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    m_frame->loader()->documentLoader()->writer()->setEncoding(encoding, userChosen);
    m_frame->loader()->documentLoader()->writer()->addData(data, length);
}

void FrameLoaderClientBlackBerry::finishedLoading(DocumentLoader*)
{
    if (m_pluginView) {
        if (m_hasSentResponseToPlugin)
            m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

PassRefPtr<DocumentLoader> FrameLoaderClientBlackBerry::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    // Make a copy of the request with the token from the original request for this frame
    // (unless it already has a token, in which case the request came from the client).
    ResourceRequest newRequest(request);
    if (m_frame && m_frame->loader() && m_frame->loader()->documentLoader()) {
        const ResourceRequest& originalRequest = m_frame->loader()->documentLoader()->originalRequest();
        if (request.token().isNull() && !originalRequest.token().isNull())
            newRequest.setToken(originalRequest.token());
    }

    SubstituteData substituteDataLocal = substituteData;
    if (isMainFrame()) {
        String source;
        if (request.url().protocolIs("about")) {
            // The first 6 letters is "about:"
            String aboutWhat = request.url().string().substring(6);
            source = aboutData(aboutWhat);
        }

        if (!source.isEmpty()) {
            // Always ignore existing substitute data if any.
            WTF::RefPtr<SharedBuffer> buffer = SharedBuffer::create(source.is8Bit() ? reinterpret_cast<const char*>(source.characters8()) : source.latin1().data(), source.length());
            substituteDataLocal = SubstituteData(buffer, "text/html", "latin1", KURL());
        }
    }

    // FIXME: This should probably be shared.
    RefPtr<DocumentLoader> loader = DocumentLoader::create(newRequest, substituteDataLocal);
    if (substituteDataLocal.isValid())
        loader->setDeferMainResourceDataLoad(false);
    return loader.release();
}

void FrameLoaderClientBlackBerry::frameLoaderDestroyed()
{
    delete this;
}

void FrameLoaderClientBlackBerry::transitionToCommittedForNewPage()
{
    m_cancelLoadOnNextData = false;

    // In Frame::createView, Frame's FrameView object is set to 0 and recreated.
    // This operation is not atomic, and an attempt to blit contents might happen
    // in the backing store from another thread (see BackingStorePrivate::blitVisibleContents method),
    // so we suspend and resume screen update to make sure we do not get a invalid FrameView
    // state.
    if (isMainFrame() && m_webPagePrivate->backingStoreClient()) {
        // FIXME: Do we really need to suspend/resume both backingstore and screen here?
        m_webPagePrivate->backingStoreClient()->backingStore()->d->suspendBackingStoreUpdates();
        m_webPagePrivate->backingStoreClient()->backingStore()->d->suspendScreenUpdates();
    }

    // We are navigating away from this document, so clean up any footprint we might have.
    if (m_frame->document())
        m_webPagePrivate->clearDocumentData(m_frame->document());

    Color backgroundColor(m_webPagePrivate->m_webSettings->backgroundColor());

    m_frame->createView(m_webPagePrivate->viewportSize(),      /* viewport */
        backgroundColor,                       /* background color */
        backgroundColor.hasAlpha(),            /* is transparent */
        m_webPagePrivate->actualVisibleSize(), /* fixed reported size */
        m_webPagePrivate->fixedLayoutSize(),   /* fixed layout size */
        IntRect(),                             /* fixed visible content rect */
        m_webPagePrivate->useFixedLayout(),    /* use fixed layout */
        ScrollbarAlwaysOff,                    /* hor mode */
        true,                                  /* lock the mode */
        ScrollbarAlwaysOff,                    /* ver mode */
        true);                                 /* lock the mode */

    if (isMainFrame() && m_webPagePrivate->backingStoreClient()) {
        // FIXME: Do we really need to suspend/resume both backingstore and screen here?
        m_webPagePrivate->backingStoreClient()->backingStore()->d->resumeBackingStoreUpdates();
        m_webPagePrivate->backingStoreClient()->backingStore()->d->resumeScreenUpdates(BackingStore::None);
    }

    m_frame->view()->updateCanHaveScrollbars();

    if (isMainFrame()) {
        // Since the mainframe has a tiled backingstore request to receive all update
        // rects instead of the default which just sends update rects for currently
        // visible viewport.
        m_frame->view()->setPaintsEntireContents(true);
    }
}

String FrameLoaderClientBlackBerry::userAgent(const KURL&)
{
    return m_webPagePrivate->m_webSettings->userAgentString();
}

bool FrameLoaderClientBlackBerry::canHandleRequest(const ResourceRequest&) const
{
    // FIXME: Stub.
    return true;
}

bool FrameLoaderClientBlackBerry::canShowMIMEType(const String& mimeTypeIn) const
{
    // Get normalized type.
    String mimeType = MIMETypeRegistry::getNormalizedMIMEType(mimeTypeIn);

    // FIXME: Seems no other port checks empty MIME type in this function. Should we do that?
    return MIMETypeRegistry::canShowMIMEType(mimeType)
        || WebSettings::isSupportedObjectMIMEType(mimeType)
        || (mimeType == "application/x-shockwave-flash");
}

bool FrameLoaderClientBlackBerry::canShowMIMETypeAsHTML(const String&) const
{
    // FIXME: Stub.
    return true;
}

bool FrameLoaderClientBlackBerry::isMainFrame() const
{
    return m_frame == m_webPagePrivate->m_mainFrame;
}

void FrameLoaderClientBlackBerry::dispatchDidStartProvisionalLoad()
{
    if (isMainFrame())
        m_webPagePrivate->setLoadState(WebPagePrivate::Provisional);

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didStartProvisionalLoadForFrame(m_frame);

    m_wasProvisionalLoadTriggeredByUserGesture = ScriptController::processingUserGesture();
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didReceiveResponseForFrame(m_frame, response);
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    if (isMainFrame())
        m_webPagePrivate->m_client->setPageTitle(title.string().characters(), title.string().length());

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didReceiveTitleForFrame(title.string(), m_frame);
}

void FrameLoaderClientBlackBerry::setTitle(const StringWithDirection& /*title*/, const KURL& /*url*/)
{
    // Used by Apple WebKit to update the title of an existing history item.
    // QtWebKit doesn't accomodate this on history items. If it ever does,
    // it should be privateBrowsing-aware. For now, we are just passing
    // globalhistory layout tests.
    // FIXME: Use direction of title.
    notImplemented();
}

void FrameLoaderClientBlackBerry::dispatchDidCommitLoad()
{
    // FIXME: Do we need to find a replacement for m_frame->document()->setExtraLayoutDelay(250);?

    if (isMainFrame()) {
        m_webPagePrivate->setLoadState(WebPagePrivate::Committed);

        String originalUrl = m_frame->loader()->documentLoader()->originalRequest().url().string();
        String url = m_frame->loader()->documentLoader()->request().url().string();
        String token = m_frame->loader()->documentLoader()->request().token();

        // Notify the client that the load succeeded or failed (if it failed, this
        // is actually committing the error page, which was set through
        // SubstituteData in dispatchDidFailProvisionalLoad).
        if (m_loadingErrorPage) {
            m_loadingErrorPage = false;
            if (HistoryItem* item = m_frame->loader()->history()->currentItem())
                item->viewState().shouldSaveViewState = false;
            m_webPagePrivate->m_client->notifyLoadFailedBeforeCommit(
                originalUrl.characters(), originalUrl.length(),
                    url.characters(), url.length(), token.characters(), token.length());
        } else {
            m_webPagePrivate->m_client->notifyLoadCommitted(
                originalUrl.characters(), originalUrl.length(),
                    url.characters(), url.length(), token.characters(), token.length());
            HistoryItem* currentItem = m_frame->loader()->history()->currentItem();
            if (currentItem && currentItem->isInPageCache())
                dispatchDidReceiveIcon();
        }
    }

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didCommitLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidHandleOnloadEvents()
{
    m_webPagePrivate->m_client->notifyDocumentOnLoad(isMainFrame());
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didHandleOnloadEventsForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidFinishLoad()
{
    didFinishOrFailLoading(ResourceError());

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didFinishLoadForFrame(m_frame);

    if (!isMainFrame() || m_webPagePrivate->m_webSettings->isEmailMode()
        || !m_frame->document() || !m_frame->document()->head())
        return;

    HTMLHeadElement* headElement = m_frame->document()->head();
    // FIXME: Handle NOSCRIPT special case?

    // Process document metadata.
    RefPtr<NodeList> nodeList = headElement->getElementsByTagName(HTMLNames::metaTag.localName());
    unsigned size = nodeList->length();
    ScopeArray<BlackBerry::Platform::String> headers;

    // This may allocate more space than needed since not all meta elements will be http-equiv.
    headers.reset(new BlackBerry::Platform::String[2 * size]);
    unsigned headersLength = 0;

    for (unsigned i = 0; i < size; ++i) {
        HTMLMetaElement* metaElement = static_cast<HTMLMetaElement*>(nodeList->item(i));
        if (WTF::equalIgnoringCase(metaElement->name(), "apple-mobile-web-app-capable")
            && WTF::equalIgnoringCase(metaElement->content().stripWhiteSpace(), "yes"))
            m_webPagePrivate->m_client->setWebAppCapable();
        else {
            String httpEquiv = metaElement->httpEquiv().stripWhiteSpace();
            String content = metaElement->content().stripWhiteSpace();

            if (!httpEquiv.isNull() && !content.isNull()) {
                headers[headersLength++] = httpEquiv;
                headers[headersLength++] = content;
            }
        }
    }

    if (headersLength > 0)
        m_webPagePrivate->m_client->setMetaHeaders(headers, headersLength);

    nodeList = headElement->getElementsByTagName(HTMLNames::linkTag.localName());
    size = nodeList->length();

    for (unsigned i = 0; i < size; ++i) {
        HTMLLinkElement* linkElement = static_cast<HTMLLinkElement*>(nodeList->item(i));
        String href = linkElement->href().string();
        if (!href.isEmpty()) {
            String title = linkElement->title();

            if (WTF::equalIgnoringCase(linkElement->rel(), "apple-touch-icon"))
                m_webPagePrivate->m_client->setLargeIcon(href.latin1().data());
            else if (WTF::equalIgnoringCase(linkElement->rel(), "search")) {
                if (WTF::equalIgnoringCase(linkElement->type(), "application/opensearchdescription+xml"))
                    m_webPagePrivate->m_client->setSearchProviderDetails(title, href);
            } else if (WTF::equalIgnoringCase(linkElement->rel(), "alternate")
                && (WTF::equalIgnoringCase(linkElement->type(), "application/rss+xml")
                || WTF::equalIgnoringCase(linkElement->type(), "application/atom+xml")))
                m_webPagePrivate->m_client->setAlternateFeedDetails(title, href);
        }
    }

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    if (m_webPagePrivate->m_webSettings->isCredentialAutofillEnabled()
        && !m_webPagePrivate->m_webSettings->isPrivateBrowsingEnabled())
        credentialManager().autofillPasswordForms(m_frame->document()->forms());
#endif
}

void FrameLoaderClientBlackBerry::dispatchDidFinishDocumentLoad()
{
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didFinishDocumentLoadForFrame(m_frame);
    notImplemented();
}

void FrameLoaderClientBlackBerry::dispatchDidFailLoad(const ResourceError& error)
{
    didFinishOrFailLoading(error);
    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didFailLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::didFinishOrFailLoading(const ResourceError& error)
{
    // FIXME: Do we need to find a replacement for m_frame->document()->setExtraLayoutDelay(0);?

    // If we have finished loading a page through history navigation, any
    // attempt to go back to that page through an automatic redirect should be
    // denied to avoid redirect loops. So save the history navigation urls to
    // check later. (If this was not a history navigation,
    // m_historyNavigationSourceURLs will be empty, and we should save that
    // too.)
    m_redirectURLsToSkipDueToHistoryNavigation.swap(m_historyNavigationSourceURLs);

    // History navigation is finished so clear the history navigation url.
    m_historyNavigationSourceURLs.clear();

    if (isMainFrame()) {
        m_loadError = error;
        m_webPagePrivate->setLoadState(error.isNull() ? WebPagePrivate::Finished : WebPagePrivate::Failed);
    }
}

void FrameLoaderClientBlackBerry::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    if (isMainFrame()) {
        m_loadError = error;
        m_webPagePrivate->setLoadState(WebPagePrivate::Failed);

        if (error.domain() == ResourceError::platformErrorDomain
            && (error.errorCode() == BlackBerry::Platform::FilterStream::StatusErrorAlreadyHandled)) {
            // Error has already been displayed by client.
            return;
        }

        if (error.domain().isEmpty() && !error.errorCode() && error.failingURL().isEmpty() && error.localizedDescription().isEmpty()) {
            // Don't try to display empty errors returned from the unimplemented error functions in FrameLoaderClientBlackBerry - there's nothing to display anyway.
            return;
        }
    }

    if (m_webPagePrivate->m_dumpRenderTree)
        m_webPagePrivate->m_dumpRenderTree->didFailProvisionalLoadForFrame(m_frame);

    if (!isMainFrame())
        return;

    if (error.domain() == ResourceError::platformErrorDomain
        && (error.errorCode() == BlackBerry::Platform::FilterStream::StatusDeniedByApplication)) {
        // Do not display error page for loading DENYed by application.
        return;
    }

    // Make sure we're still in the provisionalLoad state - getErrorPage runs a
    // nested event loop while it's waiting for client resources to load so
    // there's a small window for the user to hit stop.
    if (!m_frame->loader()->provisionalDocumentLoader())
        return;

    ResourceRequest originalRequest = m_frame->loader()->provisionalDocumentLoader()->originalRequest();

    // Do not show error page for a failed download.
    if (originalRequest.forceDownload())
        return;

    String errorPage = m_webPagePrivate->m_client->getErrorPage(error.errorCode(), error.localizedDescription(), error.failingURL());
    SubstituteData errorData(utf8Buffer(errorPage), "text/html", "utf-8", KURL(KURL(), error.failingURL()));

    // Loading using SubstituteData will replace the original request with our
    // error data. This must be done within dispatchDidFailProvisionalLoad,
    // and do NOT call stopAllLoaders first, because the loader checks the
    // provisionalDocumentLoader to decide the load type; if called any other
    // way, the error page is added to the end of the history instead of
    // replacing the failed load.
    //
    // If this comes from a back/forward navigation, we need to save the current viewstate
    // to original historyitem, and prevent the restore of view state to the error page.
    if (isBackForwardLoadType(m_frame->loader()->loadType())) {
        m_frame->loader()->history()->saveScrollPositionAndViewStateToItem(m_frame->loader()->history()->currentItem());
        ASSERT(m_frame->loader()->history()->provisionalItem());
        m_frame->loader()->history()->provisionalItem()->viewState().shouldSaveViewState = false;
    }

    m_loadingErrorPage = true;
    m_frame->loader()->load(FrameLoadRequest(m_frame, originalRequest, errorData));
}

void FrameLoaderClientBlackBerry::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState>)
{
    // FIXME: Stub.
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientBlackBerry::dispatchWillSendSubmitEvent(PassRefPtr<FormState> prpFormState)
{
    if (!m_webPagePrivate->m_webSettings->isPrivateBrowsingEnabled()) {
        if (m_webPagePrivate->m_webSettings->isFormAutofillEnabled())
            m_webPagePrivate->m_autofillManager->saveTextFields(prpFormState->form());
#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
        if (m_webPagePrivate->m_webSettings->isCredentialAutofillEnabled())
            credentialManager().saveCredentialIfConfirmed(m_webPagePrivate, CredentialTransformData(prpFormState->form(), true));
#endif
    }
}

PassRefPtr<Frame> FrameLoaderClientBlackBerry::createFrame(const KURL& url, const String& name
    , HTMLFrameOwnerElement* ownerElement, const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    if (!m_webPagePrivate)
        return 0;

    if (m_childFrameCreationSuppressed)
        return 0;

    FrameLoaderClientBlackBerry* frameLoaderClient = new FrameLoaderClientBlackBerry();
    RefPtr<Frame> childFrame = Frame::create(m_frame->page(), ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(childFrame.get(), m_webPagePrivate);

    // Initialize FrameView.
    RefPtr<FrameView> frameView = FrameView::create(childFrame.get());
    childFrame->setView(frameView.get());
    if (!allowsScrolling)
        frameView->setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
    if (marginWidth != -1)
        frameView->setMarginWidth(marginWidth);
    if (marginHeight != -1)
        frameView->setMarginHeight(marginHeight);

    childFrame->tree()->setName(name);
    m_frame->tree()->appendChild(childFrame);
    childFrame->init();

    if (!childFrame->tree()->parent())
        return 0;

    m_frame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());

    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

ObjectContentType FrameLoaderClientBlackBerry::objectContentType(const KURL& url, const String& mimeTypeIn, bool shouldPreferPlugInsForImages)
{
    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty())
        mimeType = MIMETypeRegistry::getMIMETypeForPath(url.path());

    // Get mapped type.
    mimeType = MIMETypeRegistry::getNormalizedMIMEType(mimeType);

    ObjectContentType defaultType = FrameLoader::defaultObjectContentType(url, mimeType, shouldPreferPlugInsForImages);
    if (defaultType != ObjectContentNone)
        return defaultType;

    if (WebSettings::isSupportedObjectMIMEType(mimeType))
        return ObjectContentOtherPlugin;

    return ObjectContentNone;
}

void FrameLoaderClientBlackBerry::dispatchWillClose()
{
    m_webPagePrivate->frameUnloaded(m_frame);
}

void FrameLoaderClientBlackBerry::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;

    m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientBlackBerry::postProgressStartedNotification()
{
    if (!isMainFrame())
        return;

    // New load started, so clear the error.
    m_loadError = ResourceError();
    m_sentReadyToRender = false;
    m_webPagePrivate->m_client->notifyLoadStarted();
}

void FrameLoaderClientBlackBerry::postProgressEstimateChangedNotification()
{
    if (!isMainFrame() || !m_frame->page())
        return;

    m_webPagePrivate->m_client->notifyLoadProgress(m_frame->page()->progress()->estimatedProgress() * 100);
}

void FrameLoaderClientBlackBerry::dispatchDidLayout(LayoutMilestones milestones)
{
    if (!isMainFrame())
        return;

    if (milestones & DidFirstVisuallyNonEmptyLayout) {
        BBLOG(BlackBerry::Platform::LogLevelInfo, "dispatchDidFirstVisuallyNonEmptyLayout");

        readyToRender(true);

        // For FrameLoadTypeSame or FrameLoadTypeStandard load, the layout timer can be fired which can call
        // dispatchDidFirstVisuallyNonEmptyLayout() after the load Finished state, in which case the web page
        // will have no chance to zoom to initial scale. So we should give it a chance, otherwise the scale of
        // the web page can be incorrect.
        FrameLoadType frameLoadType = m_frame->loader()->loadType();
        if (m_webPagePrivate->loadState() == WebPagePrivate::Finished && (frameLoadType == FrameLoadTypeSame || frameLoadType == FrameLoadTypeStandard))
            m_webPagePrivate->setShouldZoomToInitialScaleAfterLoadFinished(true);

        if (m_webPagePrivate->shouldZoomToInitialScaleOnLoad()) {
            BackingStorePrivate* backingStorePrivate = m_webPagePrivate->m_backingStore->d;
            m_webPagePrivate->zoomToInitialScaleOnLoad(); // Set the proper zoom level first.
            backingStorePrivate->renderAndBlitVisibleContentsImmediately();
        }

        m_webPagePrivate->m_client->notifyFirstVisuallyNonEmptyLayout();
    }
}

void FrameLoaderClientBlackBerry::postProgressFinishedNotification()
{
    if (!isMainFrame())
        return;

    // Empty pages will never have called
    // dispatchDidFirstVisuallyNonEmptyLayout, since they're visually empty, so
    // we may need to call readyToRender now.
    readyToRender(false);

    // FIXME: Send up a real status code.
    m_webPagePrivate->m_client->notifyLoadFinished(m_loadError.isNull() ? 0 : -1);

    // Notify plugins that are waiting for the page to fully load before starting that
    // the load has completed.
    m_webPagePrivate->notifyPageOnLoad();
}

void FrameLoaderClientBlackBerry::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    // Provide the extension object first in case the client or others want to use it.
    if (m_webPagePrivate->m_enableQnxJavaScriptObject)
        attachExtensionObjectToFrame(m_frame, m_webPagePrivate->m_client);

    attachExternalExtensionObjectToFrame(m_frame);

    m_webPagePrivate->m_client->notifyWindowObjectCleared();

    if (m_webPagePrivate->m_dumpRenderTree) {
        JSGlobalContextRef context = toGlobalRef(m_frame->script()->globalObject(mainThreadNormalWorld())->globalExec());
        JSObjectRef windowObject = toRef(m_frame->script()->globalObject(mainThreadNormalWorld()));
        ASSERT(windowObject);
        m_webPagePrivate->m_dumpRenderTree->didClearWindowObjectInWorld(world, context, windowObject);
    }
}

bool FrameLoaderClientBlackBerry::shouldGoToHistoryItem(HistoryItem*) const
{
    return true;
}

bool FrameLoaderClientBlackBerry::shouldStopLoadingForHistoryItem(HistoryItem*) const
{
    return true;
}

void FrameLoaderClientBlackBerry::invalidateBackForwardList() const
{
    notifyBackForwardListChanged();
}

void FrameLoaderClientBlackBerry::notifyBackForwardListChanged() const
{
    BackForwardListImpl* backForwardList = static_cast<BackForwardListImpl*>(m_webPagePrivate->m_page->backForward()->client());
    ASSERT(backForwardList);

    unsigned listSize = backForwardList->entries().size();
    unsigned currentIndex = backForwardList->backListCount();
    m_webPagePrivate->m_client->resetBackForwardList(listSize, currentIndex);
}

Frame* FrameLoaderClientBlackBerry::dispatchCreatePage(const NavigationAction& navigation)
{
    UNUSED_PARAM(navigation);
    WebPage* webPage = m_webPagePrivate->m_client->createWindow(0, 0, -1, -1, WebPageClient::FlagWindowDefault, BlackBerry::Platform::String::emptyString(), BlackBerry::Platform::String::emptyString());
    if (!webPage)
        return 0;

    return webPage->d->m_page->mainFrame();
}

void FrameLoaderClientBlackBerry::detachedFromParent2()
{
    if (m_frame->document())
        m_webPagePrivate->clearDocumentData(m_frame->document());

    m_webPagePrivate->frameUnloaded(m_frame);
    m_webPagePrivate->m_client->notifyFrameDetached(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchWillSendRequest(DocumentLoader* docLoader, long unsigned, ResourceRequest& request, const ResourceResponse&)
{
    // If the request is being loaded by the provisional document loader, then
    // it is a new top level request which has not been commited.
    bool isMainResourceLoad = docLoader && docLoader == docLoader->frameLoader()->provisionalDocumentLoader();

    // TargetType for subresource loads should have been set in CachedResource::load().
    if (isMainResourceLoad && request.targetType() == ResourceRequest::TargetIsUnspecified)
        request.setTargetType(isMainFrame() ? ResourceRequest::TargetIsMainFrame : ResourceRequest::TargetIsSubframe);

    // Any processing which is done for all loads (both main and subresource) should go here.
    NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, cookiesEnabled());
    m_webPagePrivate->m_client->populateCustomHeaders(platformRequest);
    const NetworkRequest::HeaderList& headerLists = platformRequest.getHeaderListRef();
    for (NetworkRequest::HeaderList::const_iterator it = headerLists.begin(); it != headerLists.end(); ++it) {
        BlackBerry::Platform::String headerString = it->first;
        BlackBerry::Platform::String headerValueString = it->second;
        request.setHTTPHeaderField(String::fromUTF8WithLatin1Fallback(headerString.data(), headerString.length()), String::fromUTF8WithLatin1Fallback(headerValueString.data(), headerValueString.length()));
    }
    if (!isMainResourceLoad) {
        // Do nothing for now.
        // Any processing which is done only for subresources should go here.
        return;
    }

    // All processing beyond this point is done only for main resource loads.

    if (m_clientRedirectIsPending && isMainFrame()) {
        String originalUrl = m_frame->document()->url().string();
        String finalUrl = request.url().string();

        m_webPagePrivate->m_client->notifyClientRedirect(originalUrl.characters(), originalUrl.length(),
            finalUrl.characters(), finalUrl.length());
    }

    FrameLoader* loader = m_frame->loader();
    ASSERT(loader);
    if (isBackForwardLoadType(loader->loadType())) {
        // Do not use the passed DocumentLoader because it is the loader that
        // will be used for the new request (the DESTINATION of the history
        // navigation - we want to use the current DocumentLoader to record the
        // SOURCE).
        DocumentLoader* docLoader = m_frame->loader()->documentLoader();
        ASSERT(docLoader);
        m_historyNavigationSourceURLs.add(docLoader->url());
        m_historyNavigationSourceURLs.add(docLoader->originalURL());
    }
}

bool FrameLoaderClientBlackBerry::shouldUseCredentialStorage(DocumentLoader* loader, long unsigned identifier)
{
#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
    if (m_frame->page()->settings()->privateBrowsingEnabled())
        return false;
    return true;
#else
    return false;
#endif
}

void FrameLoaderClientBlackBerry::loadIconExternally(const String& originalPageUrl, const String& finalPageUrl, const String& iconUrl)
{
    m_webPagePrivate->m_client->setIconForUrl(originalPageUrl, finalPageUrl, iconUrl);
}

void FrameLoaderClientBlackBerry::saveViewStateToItem(HistoryItem* item)
{
    if (!isMainFrame())
        return;

    ASSERT(item);
    HistoryItemViewState& viewState = item->viewState();
    if (viewState.shouldSaveViewState) {
        viewState.orientation = m_webPagePrivate->mainFrame()->orientation();
        viewState.isZoomToFitScale = m_webPagePrivate->currentScale() == m_webPagePrivate->zoomToFitScale();
        viewState.scale = m_webPagePrivate->currentScale();
        viewState.shouldReflowBlock = m_webPagePrivate->m_shouldReflowBlock;
        viewState.minimumScale = m_webPagePrivate->m_minimumScale;
        viewState.maximumScale = m_webPagePrivate->m_maximumScale;
        viewState.isUserScalable = m_webPagePrivate->m_userScalable;
        viewState.webPageClientState = m_webPagePrivate->m_client->serializePageCacheState();
    }
}

void FrameLoaderClientBlackBerry::restoreViewState()
{
    if (!isMainFrame())
        return;

    HistoryItem* currentItem = m_frame->loader()->history()->currentItem();
    ASSERT(currentItem);

    if (!currentItem)
        return;

    HistoryItemViewState& viewState = currentItem->viewState();
    if (!viewState.shouldSaveViewState)
        return;

    m_webPagePrivate->m_client->deserializePageCacheState(viewState.webPageClientState);

    // WebPagePrivate is messing up FrameView::wasScrolledByUser() by sending
    // scroll events that look like they were user generated all the time.
    //
    // Even if the user did not scroll, FrameView is gonna think they did.
    // So we use our own bookkeeping code to keep track of whether we were
    // actually scrolled by the user during load.
    //
    // If the user did scroll though, all are going to be in agreement about
    // that, and the worst thing that could happen is that
    // HistoryController::restoreScrollPositionAndViewState calls
    // setScrollPosition with the the same point, which is a NOOP.
    IntSize contentsSize = currentItem->contentsSize();
    IntPoint scrollPosition = currentItem->scrollPoint();
    if (m_webPagePrivate->m_userPerformedManualScroll)
        scrollPosition = m_webPagePrivate->scrollPosition();

    // We need to reset this variable after the view state has been restored.
    m_webPagePrivate->m_didRestoreFromPageCache = false;

    // Restore the meta first.
    m_webPagePrivate->m_minimumScale = viewState.minimumScale;
    m_webPagePrivate->m_maximumScale = viewState.maximumScale;
    m_webPagePrivate->m_userScalable = viewState.isUserScalable;

    // Also, try to keep the users zoom if any.
    double scale = viewState.scale;

    bool shouldReflowBlock = viewState.shouldReflowBlock;
    if (m_webPagePrivate->m_userPerformedManualZoom) {
        scale = m_webPagePrivate->currentScale();
        shouldReflowBlock = m_webPagePrivate->m_shouldReflowBlock;
    }

    bool scrollChanged = scrollPosition != m_webPagePrivate->scrollPosition();
    bool scaleChanged = scale != m_webPagePrivate->currentScale();
    bool reflowChanged = shouldReflowBlock != m_webPagePrivate->m_shouldReflowBlock;
    bool orientationChanged = viewState.orientation % 180 != m_webPagePrivate->mainFrame()->orientation() % 180;

    if (!scrollChanged && !scaleChanged && !reflowChanged && !orientationChanged)
        return;

    // When rotate happens, only zoom when previous page was zoomToFitScale and didn't have virtual viewport, otherwise keep old scale.
    if (orientationChanged && viewState.isZoomToFitScale && !m_webPagePrivate->hasVirtualViewport())
        scale = BlackBerry::Platform::Graphics::Screen::primaryScreen()->width() * scale / static_cast<double>(BlackBerry::Platform::Graphics::Screen::primaryScreen()->height());

    // Don't flash checkerboard before WebPagePrivate::restoreHistoryViewState() finished.
    // This call will be balanced by BackingStorePrivate::resumeScreenAndBackingStoreUpdates() in WebPagePrivate::restoreHistoryViewState().
    // FIXME: Do we really need to suspend/resume both backingstore and screen here?
    m_webPagePrivate->m_backingStore->d->suspendBackingStoreUpdates();
    m_webPagePrivate->m_backingStore->d->suspendScreenUpdates();

    // It is not safe to render the page at this point. So we post a message instead. Messages have higher priority than timers.
    BlackBerry::Platform::webKitThreadMessageClient()->dispatchMessage(BlackBerry::Platform::createMethodCallMessage(
        &WebPagePrivate::restoreHistoryViewState, m_webPagePrivate, contentsSize, scrollPosition, scale, viewState.shouldReflowBlock));
}

PolicyAction FrameLoaderClientBlackBerry::decidePolicyForExternalLoad(const ResourceRequest& request, bool isFragmentScroll)
{
#if 0
    // FIXME: Enable these commented out when WebPageClient::handleStringPattern is implemented
    // and exposed to client. Before that, don't return PolicyIgnore so we can continue to
    // create new window and get to dispatchDecidePolicyForNavigationAction() where the client
    // is given a chance to decide how to handle patterns such as 'mailto:'.
    const KURL& url = request.url();
    String pattern = m_webPagePrivate->findPatternStringForUrl(url);
    if (!pattern.isEmpty()) {
        m_webPagePrivate->m_client->handleStringPattern(pattern.characters(), pattern.length());
        return PolicyIgnore;
    }
#endif

    if (m_webPagePrivate->m_webSettings->areLinksHandledExternally()
        && isMainFrame()
        && !request.mustHandleInternally()
        && !isFragmentScroll) {
        NetworkRequest platformRequest;
        request.initializePlatformRequest(platformRequest, cookiesEnabled());
        if (platformRequest.getTargetType() == NetworkRequest::TargetIsUnknown)
            platformRequest.setTargetType(isMainFrame() ? NetworkRequest::TargetIsMainFrame : NetworkRequest::TargetIsSubframe);

        m_webPagePrivate->m_client->handleExternalLink(platformRequest, request.anchorText().characters(), request.anchorText().length(), m_clientRedirectIsPending);
        return PolicyIgnore;
    }

    return PolicyUse;
}

void FrameLoaderClientBlackBerry::willDeferLoading()
{
    if (!isMainFrame())
        return;

    m_webPagePrivate->willDeferLoading();
}

void FrameLoaderClientBlackBerry::didResumeLoading()
{
    if (!isMainFrame())
        return;

    m_webPagePrivate->didResumeLoading();
}

void FrameLoaderClientBlackBerry::readyToRender(bool pageIsVisuallyNonEmpty)
{
    // Only send the notification once.
    if (!m_sentReadyToRender) {
        m_webPagePrivate->m_client->notifyLoadReadyToRender(pageIsVisuallyNonEmpty);
        m_sentReadyToRender = true;
    }
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientBlackBerry::createNetworkingContext()
{
    return FrameNetworkingContextBlackBerry::create(m_frame);
}

void FrameLoaderClientBlackBerry::startDownload(const ResourceRequest& request, const String& suggestedName)
{
    m_webPagePrivate->load(request.url().string(), BlackBerry::Platform::String::emptyString(), "GET", NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, false, false, true, "", suggestedName);
}

void FrameLoaderClientBlackBerry::convertMainResourceLoadToDownload(MainResourceLoader* mainResourceLoader, const ResourceRequest&, const ResourceRequest&, const ResourceResponse& r)
{
    BlackBerry::Platform::FilterStream* stream = NetworkManager::instance()->streamForHandle(mainResourceLoader->loader()->handle());
    ASSERT(stream);

    m_webPagePrivate->m_client->downloadRequested(stream, r.suggestedFilename());
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveIcon()
{
    String url = m_frame->document()->url().string();
    NativeImageSkia* bitmap = iconDatabase().synchronousNativeIconForPageURL(url, IntSize(10, 10));
    if (!bitmap || bitmap->bitmap().empty())
        return;

    SkAutoLockPixels locker(bitmap->bitmap());
    SkDynamicMemoryWStream writer;
    if (!SkImageEncoder::EncodeStream(&writer, bitmap->bitmap(), SkImageEncoder::kPNG_Type, 100)) {
        BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelInfo, "Failed to convert the icon to PNG format.");
        return;
    }
    SkData* data = writer.copyToData();
    Vector<char> out;
    base64Encode(static_cast<const char*>(data->data()), data->size(), out);
    out.append('\0'); // Make it null-terminated.
    String iconUrl = iconDatabase().synchronousIconURLForPageURL(url);
    m_webPagePrivate->m_client->setFavicon(out.data(), iconUrl);
    data->unref();
}

bool FrameLoaderClientBlackBerry::canCachePage() const
{
    // We won't cache pages containing multipart with "multipart/x-mixed-replace".
    ASSERT(m_frame->loader()->documentLoader());
    const ResponseVector& responses = m_frame->loader()->documentLoader()->responses();
    size_t count = responses.size();
    for (size_t i = 0; i < count; i++) {
        if (responses[i].isMultipartPayload())
            return false;
    }
    return true;
}

void FrameLoaderClientBlackBerry::didSaveToPageCache()
{
    // When page goes into PageCache, clean up any possible
    // document data cache we might have.
    m_webPagePrivate->clearDocumentData(m_frame->document());
}

void FrameLoaderClientBlackBerry::provisionalLoadStarted()
{
    // We would like to hide the virtual keyboard before it navigates to another page
    // so that the scroll offset without keyboard shown will be saved in the history item.
    // Then when the user navigates back, it will scroll to the right position.
    if (isMainFrame())
        m_webPagePrivate->showVirtualKeyboard(false);
}

// We don't need to provide the error message string, that will be handled in BrowserErrorPage according to the error code.
ResourceError FrameLoaderClientBlackBerry::cannotShowURLError(const ResourceRequest& request)
{
    // FIXME: Why are we not passing the domain to the ResourceError? See PR #119789.
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

void FrameLoaderClientBlackBerry::didRestoreFromPageCache()
{
    m_webPagePrivate->m_didRestoreFromPageCache = true;
}

void FrameLoaderClientBlackBerry::dispatchWillUpdateApplicationCache(const ResourceRequest&)
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    m_webPagePrivate->m_client->notifyWillUpdateApplicationCache();
}

void FrameLoaderClientBlackBerry::dispatchDidLoadFromApplicationCache(const ResourceRequest&)
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    m_webPagePrivate->m_client->notifyDidLoadFromApplicationCache();
}

PassRefPtr<SecurityOrigin> FrameLoaderClientBlackBerry::securityOriginForNewDocument(const KURL& url)
{
    // What we are trying to do here is to keep using the old path as origin when a file-based html page
    // changes its location to some html in a subfolder. This will allow some file-based html packages
    // to work smoothly even with security checks enabled.

    RefPtr<SecurityOrigin> newSecurityOrigin = SecurityOrigin::create(url);

    if (m_wasProvisionalLoadTriggeredByUserGesture || !url.isLocalFile())
        return newSecurityOrigin;

    RefPtr<SecurityOrigin> currentSecurityOrigin = m_frame->document()->securityOrigin();
    if (currentSecurityOrigin && currentSecurityOrigin->containsInFolder(newSecurityOrigin.get()))
        return currentSecurityOrigin;

    return newSecurityOrigin;
}

} // WebCore
