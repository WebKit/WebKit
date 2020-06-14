/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SubframeLoader.h"

#include "ContentSecurityPolicy.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLAppletElement.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "MIMETypeRegistry.h"
#include "NavigationScheduler.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginDocument.h"
#include "RenderEmbeddedObject.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {
    
using namespace HTMLNames;

FrameLoader::SubframeLoader::SubframeLoader(Frame& frame)
    : m_frame(frame)
{
}

void FrameLoader::SubframeLoader::clear()
{
    m_containsPlugins = false;
}

bool FrameLoader::SubframeLoader::requestFrame(HTMLFrameOwnerElement& ownerElement, const String& urlString, const AtomString& frameName, LockHistory lockHistory, LockBackForwardList lockBackForwardList)
{
    // Support for <frame src="javascript:string">
    URL scriptURL;
    URL url;
    if (WTF::protocolIsJavaScript(urlString)) {
        scriptURL = completeURL(urlString); // completeURL() encodes the URL.
        url = aboutBlankURL();
    } else
        url = completeURL(urlString);

    if (shouldConvertInvalidURLsToBlank() && !url.isValid())
        url = aboutBlankURL();

    // If we will schedule a JavaScript URL load, we need to delay the firing of the load event at least until we've run the JavaScript in the URL.
    CompletionHandlerCallingScope stopDelayingLoadEvent;
    if (!scriptURL.isEmpty()) {
        ownerElement.document().incrementLoadEventDelayCount();
        stopDelayingLoadEvent = CompletionHandlerCallingScope([ownerDocument = makeRef(ownerElement.document())] {
            ownerDocument->decrementLoadEventDelayCount();
        });
    }

    Frame* frame = loadOrRedirectSubframe(ownerElement, url, frameName, lockHistory, lockBackForwardList);
    if (!frame)
        return false;

    if (!scriptURL.isEmpty() && ownerElement.canLoadScriptURL(scriptURL)) {
        // FIXME: Some sites rely on the javascript:'' loading synchronously, which is why we have this special case.
        // Blink has the same workaround (https://bugs.chromium.org/p/chromium/issues/detail?id=923585).
        if (urlString == "javascript:''" || urlString == "javascript:\"\"")
            frame->script().executeJavaScriptURL(scriptURL);
        else
            frame->navigationScheduler().scheduleLocationChange(ownerElement.document(), ownerElement.document().securityOrigin(), scriptURL, m_frame.loader().outgoingReferrer(), lockHistory, lockBackForwardList, stopDelayingLoadEvent.release());
    }

    return true;
}
    
bool FrameLoader::SubframeLoader::resourceWillUsePlugin(const String& url, const String& mimeType)
{
    URL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    bool useFallback;
    return shouldUsePlugin(completedURL, mimeType, false, useFallback);
}

bool FrameLoader::SubframeLoader::pluginIsLoadable(const URL& url, const String& mimeType)
{
    auto* document = m_frame.document();

    if (MIMETypeRegistry::isJavaAppletMIMEType(mimeType)) {
        if (!m_frame.settings().isJavaEnabled())
            return false;
        if (document && document->securityOrigin().isLocal() && !m_frame.settings().isJavaEnabledForLocalFiles())
            return false;
    }

    if (document) {
        if (document->isSandboxed(SandboxPlugins))
            return false;

        if (!document->securityOrigin().canDisplay(url)) {
            FrameLoader::reportLocalLoadFailed(&m_frame, url.string());
            return false;
        }

        if (!m_frame.loader().mixedContentChecker().canRunInsecureContent(document->securityOrigin(), url))
            return false;
    }

    return true;
}

bool FrameLoader::SubframeLoader::requestPlugin(HTMLPlugInImageElement& ownerElement, const URL& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    // Application plug-ins are plug-ins implemented by the user agent, for example Qt plug-ins,
    // as opposed to third-party code such as Flash. The user agent decides whether or not they are
    // permitted, rather than WebKit.
    if (!(m_frame.settings().arePluginsEnabled() || MIMETypeRegistry::isApplicationPluginMIMEType(mimeType)))
        return false;

    if (!pluginIsLoadable(url, mimeType))
        return false;

    ASSERT(ownerElement.hasTagName(objectTag) || ownerElement.hasTagName(embedTag));
    return loadPlugin(ownerElement, url, mimeType, paramNames, paramValues, useFallback);
}

static String findPluginMIMETypeFromURL(Page* page, const String& url)
{
    if (!url)
        return String();

    size_t dotIndex = url.reverseFind('.');
    if (dotIndex == notFound)
        return String();

    String extension = url.substring(dotIndex + 1);

    const PluginData& pluginData = page->pluginData();

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    pluginData.getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    for (auto& mime : mimes) {
        for (auto& mimeExtension : mime.extensions) {
            if (equalIgnoringASCIICase(extension, mimeExtension))
                return mime.type;
        }
    }

    return String();
}

static void logPluginRequest(Page* page, const String& mimeType, const String& url, bool success)
{
    if (!page)
        return;

    String newMIMEType = mimeType;
    if (!newMIMEType) {
        // Try to figure out the MIME type from the URL extension.
        newMIMEType = findPluginMIMETypeFromURL(page, url);
        if (!newMIMEType)
            return;
    }

    String pluginFile = page->pluginData().pluginFileForWebVisibleMimeType(newMIMEType);
    String description = !pluginFile ? newMIMEType : pluginFile;

    DiagnosticLoggingClient& diagnosticLoggingClient = page->diagnosticLoggingClient();
    diagnosticLoggingClient.logDiagnosticMessage(success ? DiagnosticLoggingKeys::pluginLoadedKey() : DiagnosticLoggingKeys::pluginLoadingFailedKey(), description, ShouldSample::No);

    if (!page->hasSeenAnyPlugin())
        diagnosticLoggingClient.logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsAtLeastOnePluginKey(), emptyString(), ShouldSample::No);

    if (!page->hasSeenPlugin(description))
        diagnosticLoggingClient.logDiagnosticMessage(DiagnosticLoggingKeys::pageContainsPluginKey(), description, ShouldSample::No);

    page->sawPlugin(description);
}

bool FrameLoader::SubframeLoader::requestObject(HTMLPlugInImageElement& ownerElement, const String& url, const AtomString& frameName, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    auto& document = ownerElement.document();

    URL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(completedURL, ContentSecurityPolicy::InsecureRequestType::Load);

    bool hasFallbackContent = is<HTMLObjectElement>(ownerElement) && downcast<HTMLObjectElement>(ownerElement).hasFallbackContent();

    bool useFallback;
    if (shouldUsePlugin(completedURL, mimeType, hasFallbackContent, useFallback)) {
        bool success = requestPlugin(ownerElement, completedURL, mimeType, paramNames, paramValues, useFallback);
        logPluginRequest(document.page(), mimeType, completedURL.string(), success);
        return success;
    }

    // If the plug-in element already contains a subframe, loadOrRedirectSubframe will re-use it. Otherwise,
    // it will create a new frame and set it as the RenderWidget's Widget, causing what was previously 
    // in the widget to be torn down.
    return loadOrRedirectSubframe(ownerElement, completedURL, frameName, LockHistory::Yes, LockBackForwardList::Yes);
}

RefPtr<Widget> FrameLoader::SubframeLoader::createJavaAppletWidget(const IntSize& size, HTMLAppletElement& element, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    String baseURLString;
    String codeBaseURLString;

    for (size_t i = 0; i < paramNames.size(); ++i) {
        if (equalLettersIgnoringASCIICase(paramNames[i], "baseurl"))
            baseURLString = paramValues[i];
        else if (equalLettersIgnoringASCIICase(paramNames[i], "codebase"))
            codeBaseURLString = paramValues[i];
    }

    if (!codeBaseURLString.isEmpty()) {
        URL codeBaseURL = completeURL(codeBaseURLString);
        if (!element.document().securityOrigin().canDisplay(codeBaseURL)) {
            FrameLoader::reportLocalLoadFailed(&m_frame, codeBaseURL.string());
            return nullptr;
        }

        const char javaAppletMimeType[] = "application/x-java-applet";
        ASSERT(element.document().contentSecurityPolicy());
        auto& contentSecurityPolicy = *element.document().contentSecurityPolicy();
        // Elements in user agent show tree should load whatever the embedding document policy is.
        if (!element.isInUserAgentShadowTree()
            && (!contentSecurityPolicy.allowObjectFromSource(codeBaseURL) || !contentSecurityPolicy.allowPluginType(javaAppletMimeType, javaAppletMimeType, codeBaseURL)))
            return nullptr;
    }

    if (baseURLString.isEmpty())
        baseURLString = element.document().baseURL().string();
    URL baseURL = completeURL(baseURLString);

    RefPtr<Widget> widget;
    if (m_frame.settings().arePluginsEnabled())
        widget = m_frame.loader().client().createJavaAppletWidget(size, element, baseURL, paramNames, paramValues);

    logPluginRequest(m_frame.page(), element.serviceType(), String(), widget);

    if (!widget) {
        RenderEmbeddedObject* renderer = element.renderEmbeddedObject();

        if (!renderer->isPluginUnavailable())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return nullptr;
    }

    m_containsPlugins = true;
    return widget;
}

Frame* FrameLoader::SubframeLoader::loadOrRedirectSubframe(HTMLFrameOwnerElement& ownerElement, const URL& requestURL, const AtomString& frameName, LockHistory lockHistory, LockBackForwardList lockBackForwardList)
{
    auto& initiatingDocument = ownerElement.document();

    URL upgradedRequestURL = requestURL;
    initiatingDocument.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(upgradedRequestURL, ContentSecurityPolicy::InsecureRequestType::Load);

    RefPtr<Frame> frame = makeRefPtr(ownerElement.contentFrame());
    if (frame)
        frame->navigationScheduler().scheduleLocationChange(initiatingDocument, initiatingDocument.securityOrigin(), upgradedRequestURL, m_frame.loader().outgoingReferrer(), lockHistory, lockBackForwardList);
    else
        frame = loadSubframe(ownerElement, upgradedRequestURL, frameName, m_frame.loader().outgoingReferrer());

    if (!frame)
        return nullptr;

    ASSERT(ownerElement.contentFrame() == frame || !ownerElement.contentFrame());
    return ownerElement.contentFrame();
}

RefPtr<Frame> FrameLoader::SubframeLoader::loadSubframe(HTMLFrameOwnerElement& ownerElement, const URL& url, const String& name, const String& referrer)
{
    Ref<Frame> protect(m_frame);
    auto document = makeRef(ownerElement.document());

    if (!document->securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(&m_frame, url.string());
        return nullptr;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(ownerElement))
        return nullptr;

    if (!m_frame.page() || m_frame.page()->subframeCount() >= Page::maxNumberOfFrames)
        return nullptr;

    // Prevent initial empty document load from triggering load events.
    document->incrementLoadEventDelayCount();

    auto frame = m_frame.loader().client().createFrame(name, ownerElement);
    if (!frame)  {
        m_frame.loader().checkCallImplicitClose();
        return nullptr;
    }
    ReferrerPolicy policy = ownerElement.referrerPolicy();
    if (policy == ReferrerPolicy::EmptyString)
        policy = document->referrerPolicy();
    String referrerToUse = SecurityPolicy::generateReferrerHeader(policy, url, referrer);

    m_frame.loader().loadURLIntoChildFrame(url, referrerToUse, frame.get());

    document->decrementLoadEventDelayCount();

    // The frame's onload handler may have removed it from the document.
    if (!frame || !frame->tree().parent()) {
        m_frame.loader().checkCallImplicitClose();
        return nullptr;
    }

    // All new frames will have m_isComplete set to true at this point due to synchronously loading
    // an empty document in FrameLoader::init(). But many frames will now be starting an
    // asynchronous load of url, so we set m_isComplete to false and then check if the load is
    // actually completed below. (Note that we set m_isComplete to false even for synchronous
    // loads, so that checkCompleted() below won't bail early.)
    // FIXME: Can we remove this entirely? m_isComplete normally gets set to false when a load is committed.
    frame->loader().started();
   
    auto* renderer = ownerElement.renderer();
    auto* view = frame->view();
    if (is<RenderWidget>(renderer) && view)
        downcast<RenderWidget>(*renderer).setWidget(view);

    m_frame.loader().checkCallImplicitClose();

    // Some loads are performed synchronously (e.g., about:blank and loads
    // cancelled by returning a null ResourceRequest from requestFromDelegate).
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the 
    // completed() signal for the child by hand and mark the load as being
    // complete.
    // FIXME: In this case the Frame will have finished loading before 
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (frame->loader().state() == FrameStateComplete && !frame->loader().policyDocumentLoader())
        frame->loader().checkCompleted();

    if (!frame->tree().parent())
        return nullptr;

    return frame;
}

bool FrameLoader::SubframeLoader::shouldUsePlugin(const URL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    if (m_frame.loader().client().shouldAlwaysUsePluginDocument(mimeType)) {
        useFallback = false;
        return true;
    }

    ObjectContentType objectType = m_frame.loader().client().objectContentType(url, mimeType);
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentType::None && hasFallback;

    return objectType == ObjectContentType::None || objectType == ObjectContentType::PlugIn;
}

bool FrameLoader::SubframeLoader::loadPlugin(HTMLPlugInImageElement& pluginElement, const URL& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    if (useFallback)
        return false;

    auto& document = pluginElement.document();
    auto* renderer = pluginElement.renderEmbeddedObject();

    // FIXME: This code should not depend on renderer!
    if (!renderer)
        return false;

    pluginElement.subframeLoaderWillCreatePlugIn(url);

    IntSize contentSize = roundedIntSize(LayoutSize(renderer->contentWidth(), renderer->contentHeight()));
    bool loadManually = is<PluginDocument>(document) && !m_containsPlugins && downcast<PluginDocument>(document).shouldLoadPluginManually();

#if PLATFORM(IOS_FAMILY)
    // On iOS, we only tell the plugin to be in full page mode if the containing plugin document is the top level document.
    if (document.ownerElement())
        loadManually = false;
#endif

    auto weakRenderer = makeWeakPtr(*renderer);

    auto widget = m_frame.loader().client().createPlugin(contentSize, pluginElement, url, paramNames, paramValues, mimeType, loadManually);

    // The call to createPlugin *may* cause this renderer to disappear from underneath.
    if (!weakRenderer)
        return false;

    if (!widget) {
        if (!renderer->isPluginUnavailable())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return false;
    }

    pluginElement.subframeLoaderDidCreatePlugIn(*widget);
    renderer->setWidget(WTFMove(widget));
    m_containsPlugins = true;
    return true;
}

URL FrameLoader::SubframeLoader::completeURL(const String& url) const
{
    ASSERT(m_frame.document());
    return m_frame.document()->completeURL(url);
}

bool FrameLoader::SubframeLoader::shouldConvertInvalidURLsToBlank() const
{
    return m_frame.settings().shouldConvertInvalidURLsToBlank();
}

} // namespace WebCore
