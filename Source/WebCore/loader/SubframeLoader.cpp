/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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
#include "FrameLoaderClient.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "MIMETypeRegistry.h"
#include "MixedContentChecker.h"
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

static bool canLoadJavaScriptURL(HTMLFrameOwnerElement& ownerElement, const URL& url)
{
    ASSERT(url.protocolIsJavaScript());
    if (!ownerElement.document().contentSecurityPolicy()->allowJavaScriptURLs(aboutBlankURL().string(), { }, url.string(), &ownerElement))
        return false;
    if (!ownerElement.canLoadScriptURL(url))
        return false;
    return true;
}

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
    URL url = completeURL(urlString);

    if (shouldConvertInvalidURLsToBlank() && !url.isValid())
        url = aboutBlankURL();

    // Check the CSP of the embedder to determine if we allow execution of javascript: URLs via child frame navigation.
    if (url.protocolIsJavaScript() && !canLoadJavaScriptURL(ownerElement, url))
        url = aboutBlankURL();

    return loadOrRedirectSubframe(ownerElement, url, frameName, lockHistory, lockBackForwardList);
}
    
bool FrameLoader::SubframeLoader::resourceWillUsePlugin(const String& url, const String& mimeType)
{
    URL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    bool useFallback;
    return shouldUsePlugin(completedURL, mimeType, false, useFallback);
}

bool FrameLoader::SubframeLoader::pluginIsLoadable(const URL& url)
{
    auto* document = m_frame.document();
    if (document) {
        if (document->isSandboxed(SandboxPlugins))
            return false;

        if (!document->securityOrigin().canDisplay(url)) {
            FrameLoader::reportLocalLoadFailed(&m_frame, url.string());
            return false;
        }

        if (!portAllowed(url)) {
            FrameLoader::reportBlockedLoadFailed(m_frame, url);
            return false;
        }

        if (!MixedContentChecker::canRunInsecureContent(m_frame, document->securityOrigin(), url))
            return false;
    }

    return true;
}

static String findPluginMIMETypeFromURL(Page& page, const URL& url)
{
    auto lastPathComponent = url.lastPathComponent();
    size_t dotIndex = lastPathComponent.reverseFind('.');
    if (dotIndex == notFound)
        return { };

    auto extensionFromURL = lastPathComponent.substring(dotIndex + 1);

    for (auto& type : page.pluginData().webVisibleMimeTypes()) {
        for (auto& extension : type.extensions) {
            if (equalIgnoringASCIICase(extensionFromURL, extension))
                return type.type;
        }
    }

    return { };
}

bool FrameLoader::SubframeLoader::requestPlugin(HTMLPlugInImageElement& ownerElement, const URL& url, const String& explicitMIMEType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues, bool useFallback)
{
    String mimeType = explicitMIMEType;
    if (mimeType.isEmpty()) {
        if (auto page = ownerElement.document().page())
            mimeType = findPluginMIMETypeFromURL(*page, url);
    }

    // Application plug-ins are plug-ins implemented by the user agent, for example Qt plug-ins,
    // as opposed to third-party code such as Flash. The user agent decides whether or not they are
    // permitted, rather than WebKit.
    if (!(m_frame.arePluginsEnabled() || MIMETypeRegistry::isApplicationPluginMIMEType(mimeType)))
        return false;

    if (!pluginIsLoadable(url))
        return false;

    ASSERT(ownerElement.hasTagName(objectTag) || ownerElement.hasTagName(embedTag));
    return loadPlugin(ownerElement, url, explicitMIMEType, paramNames, paramValues, useFallback);
}

static void logPluginRequest(Page* page, const String& mimeType, const URL& url)
{
    if (!page)
        return;

    String newMIMEType = mimeType;
    if (!newMIMEType) {
        // Try to figure out the MIME type from the URL extension.
        newMIMEType = findPluginMIMETypeFromURL(*page, url);
        if (!newMIMEType)
            return;
    }

    String pluginFile = page->pluginData().pluginFileForWebVisibleMimeType(newMIMEType);
    String description = !pluginFile ? newMIMEType : pluginFile;
    page->sawPlugin(description);
}

bool FrameLoader::SubframeLoader::requestObject(HTMLPlugInImageElement& ownerElement, const String& url, const AtomString& frameName, const String& mimeType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    auto& document = ownerElement.document();

    URL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(completedURL, ContentSecurityPolicy::InsecureRequestType::Load);

    // Historically, we haven't run javascript URLs in <embed> / <object> elements.
    if (completedURL.protocolIsJavaScript())
        return false;

    bool hasFallbackContent = is<HTMLObjectElement>(ownerElement) && downcast<HTMLObjectElement>(ownerElement).hasFallbackContent();

    bool useFallback;
    if (shouldUsePlugin(completedURL, mimeType, hasFallbackContent, useFallback)) {
        bool success = requestPlugin(ownerElement, completedURL, mimeType, paramNames, paramValues, useFallback);
        logPluginRequest(document.page(), mimeType, completedURL);
        return success;
    }

    // If the plug-in element already contains a subframe, loadOrRedirectSubframe will re-use it. Otherwise,
    // it will create a new frame and set it as the RenderWidget's Widget, causing what was previously 
    // in the widget to be torn down.
    return loadOrRedirectSubframe(ownerElement, completedURL, frameName, LockHistory::Yes, LockBackForwardList::Yes);
}

Frame* FrameLoader::SubframeLoader::loadOrRedirectSubframe(HTMLFrameOwnerElement& ownerElement, const URL& requestURL, const AtomString& frameName, LockHistory lockHistory, LockBackForwardList lockBackForwardList)
{
    auto& initiatingDocument = ownerElement.document();

    URL upgradedRequestURL = requestURL;
    initiatingDocument.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(upgradedRequestURL, ContentSecurityPolicy::InsecureRequestType::Load);

    RefPtr frame = ownerElement.contentFrame();
    if (frame) {
        CompletionHandler<void()> stopDelayingLoadEvent = [] { };
        if (upgradedRequestURL.protocolIsJavaScript()) {
            ownerElement.document().incrementLoadEventDelayCount();
            stopDelayingLoadEvent = [ownerDocument = Ref { ownerElement.document() }] {
                ownerDocument->decrementLoadEventDelayCount();
            };
        }
        frame->navigationScheduler().scheduleLocationChange(initiatingDocument, initiatingDocument.securityOrigin(), upgradedRequestURL, m_frame.loader().outgoingReferrer(), lockHistory, lockBackForwardList, WTFMove(stopDelayingLoadEvent));
    } else
        frame = loadSubframe(ownerElement, upgradedRequestURL, frameName, m_frame.loader().outgoingReferrer());

    if (!frame)
        return nullptr;

    ASSERT(ownerElement.contentFrame() == frame || !ownerElement.contentFrame());
    return ownerElement.contentFrame();
}

RefPtr<Frame> FrameLoader::SubframeLoader::loadSubframe(HTMLFrameOwnerElement& ownerElement, const URL& url, const AtomString& name, const String& referrer)
{
    Ref protectedFrame { m_frame };
    Ref document = ownerElement.document();

    if (!document->securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(&m_frame, url.string());
        return nullptr;
    }

    if (!portAllowed(url)) {
        FrameLoader::reportBlockedLoadFailed(m_frame, url);
        return nullptr;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(ownerElement))
        return nullptr;

    if (!m_frame.page() || m_frame.page()->subframeCount() >= Page::maxNumberOfFrames)
        return nullptr;

    if (m_frame.tree().depth() >= Page::maxFrameDepth)
        return nullptr;

    // Prevent initial empty document load from triggering load events.
    document->incrementLoadEventDelayCount();

    auto frame = m_frame.loader().client().createFrame(name, ownerElement);
    if (!frame)  {
        m_frame.loader().checkCallImplicitClose();
        document->decrementLoadEventDelayCount();
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
    if (frame->loader().state() == FrameState::Complete && !frame->loader().policyDocumentLoader())
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

bool FrameLoader::SubframeLoader::loadPlugin(HTMLPlugInImageElement& pluginElement, const URL& url, const String& mimeType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues, bool useFallback)
{
    if (useFallback)
        return false;

    auto& document = pluginElement.document();
    auto* renderer = pluginElement.renderEmbeddedObject();

    // FIXME: This code should not depend on renderer!
    if (!renderer)
        return false;

    IntSize contentSize = roundedIntSize(LayoutSize(renderer->contentWidth(), renderer->contentHeight()));
    bool loadManually = is<PluginDocument>(document) && !m_containsPlugins && downcast<PluginDocument>(document).shouldLoadPluginManually();

#if PLATFORM(IOS_FAMILY)
    // On iOS, we only tell the plugin to be in full page mode if the containing plugin document is the top level document.
    if (document.ownerElement())
        loadManually = false;
#endif

    WeakPtr weakRenderer { *renderer };

    auto widget = m_frame.loader().client().createPlugin(contentSize, pluginElement, url, paramNames, paramValues, mimeType, loadManually);

    // The call to createPlugin *may* cause this renderer to disappear from underneath.
    if (!weakRenderer)
        return false;

    if (!widget) {
        if (!renderer->isPluginUnavailable())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return false;
    }

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
