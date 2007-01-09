/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "config.h"
#import "FrameLoader.h"

#import "BlockExceptions.h"
#import "Cache.h"
#import "Chrome.h"
#import "DOMElementInternal.h"
#import "Document.h"
#import "DocumentLoader.h"
#import "Event.h"
#import "FloatRect.h"
#import "FormDataStreamMac.h"
#import "FormState.h"
#import "FrameLoadRequest.h"
#import "FrameLoaderClient.h"
#import "FrameMac.h"
#import "FramePrivate.h"
#import "FrameTree.h"
#import "FrameView.h"
#import "HistoryItem.h"
#import "HTMLFormElement.h"
#import "HTMLFrameElement.h"
#import "HTMLNames.h"
#import "IconDatabase.h"
#import "LoaderNSURLExtras.h"
#import "LoaderNSURLRequestExtras.h"
#import "MainResourceLoader.h"
#import "NavigationAction.h"
#import "Page.h"
#import "PageState.h"
#import "Plugin.h"
#import "ResourceError.h"
#import "ResourceHandle.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "Settings.h"
#import "SubresourceLoader.h"
#import "SystemTime.h"
#import "TextResourceDecoder.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import "Widget.h"
#import "WindowFeatures.h"
#import <kjs/JSLock.h>
#import <wtf/Assertions.h>

namespace WebCore {

using namespace HTMLNames;

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());

    switch (m_state) {
        case FrameStateProvisional: {
            if (m_delegateIsHandlingProvisionalLoadError)
                return;

            RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
            if (!pdl)
                return;
                
            // If we've received any errors we may be stuck in the provisional state and actually complete.
            const ResourceError& error = pdl->mainDocumentError();
            if (error.isNull())
                return;

            // Check all children first.
            RefPtr<HistoryItem> item;
            if (isBackForwardLoadType(loadType()) && m_frame == m_frame->page()->mainFrame())
                item = m_currentHistoryItem;
                
            bool shouldReset = true;
            if (!pdl->isLoadingInAPISense()) {
                m_delegateIsHandlingProvisionalLoadError = true;
                m_client->dispatchDidFailProvisionalLoad(error);
                m_delegateIsHandlingProvisionalLoadError = false;

                // FIXME: can stopping loading here possibly have any effect, if isLoading is false,
                // which it must be to be in this branch of the if? And is it OK to just do a full-on
                // stopAllLoaders instead of stopLoadingSubframes?
                stopLoadingSubframes();
                pdl->stopLoading();

                // Finish resetting the load state, but only if another load hasn't been started by the
                // delegate callback.
                if (pdl == m_provisionalDocumentLoader)
                    clearProvisionalLoad();
                else if (m_documentLoader) {
                    KURL unreachableURL = m_documentLoader->unreachableURL();
                    if (!unreachableURL.isEmpty() && unreachableURL == pdl->request().url())
                        shouldReset = false;
                }
            }
            if (shouldReset && item && m_frame->page())
                 m_frame->page()->backForwardList()->goToItem(item.get());

            return;
        }
        
        case FrameStateCommittedPage: {
            DocumentLoader* dl = m_documentLoader.get();            
            if (dl->isLoadingInAPISense())
                return;

            markLoadComplete();

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            m_client->forceLayoutForNonHTML();
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload)
                    && m_frame->page() && m_frame->page()->backForwardList())
                restoreScrollPositionAndViewState();

            const ResourceError& error = dl->mainDocumentError();
            if (!error.isNull())
                m_client->dispatchDidFailLoad(error);
            else
                m_client->dispatchDidFinishLoad();

            m_client->progressCompleted();
            return;
        }
        
        case FrameStateComplete:
            // Even if already complete, we might have set a previous item on a frame that
            // didn't do any data loading on the past transaction. Make sure to clear these out.
            m_client->frameLoadCompleted();
            return;
    }

    ASSERT_NOT_REACHED();
}

String FrameLoader::referrer() const
{
    return documentLoader()->request().httpReferrer();
}

void FrameLoader::didReceiveAuthenticationChallenge(ResourceLoader* loader, NSURLAuthenticationChallenge *currentWebChallenge)
{
    m_client->dispatchDidReceiveAuthenticationChallenge(activeDocumentLoader(), loader->identifier(), currentWebChallenge);
}

void FrameLoader::didCancelAuthenticationChallenge(ResourceLoader* loader, NSURLAuthenticationChallenge *currentWebChallenge)
{
    m_client->dispatchDidCancelAuthenticationChallenge(activeDocumentLoader(), loader->identifier(), currentWebChallenge);
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    m_client->didChangeTitle(loader);

    // The title doesn't get communicated to the WebView until we are committed.
    if (loader->isCommitted())
        if (NSURL *urlForHistory = canonicalURL(loader->urlForHistory().getNSURL())) {
            // Must update the entries in the back-forward list too.
            // This must go through the WebFrame because it has the right notion of the current b/f item.
            m_client->setTitle(loader->title(), urlForHistory);
            m_client->setMainFrameDocumentReady(true); // update observers with new DOMDocument
            m_client->dispatchDidReceiveTitle(loader->title());
        }
}

Frame* FrameLoader::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    BOOL allowsScrolling = YES;
    int marginWidth = -1;
    int marginHeight = -1;
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElement* o = static_cast<HTMLFrameElement*>(ownerElement);
        allowsScrolling = o->scrollingMode() != ScrollbarAlwaysOff;
        marginWidth = o->getMarginWidth();
        marginHeight = o->getMarginHeight();
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    return [Mac(m_frame)->bridge() createChildFrameNamed:name
                                                 withURL:url.getNSURL()
                                                referrer:referrer 
                                              ownerElement:ownerElement
                                         allowsScrolling:allowsScrolling
                                             marginWidth:marginWidth
                                            marginHeight:marginHeight];

    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

ObjectContentType FrameLoader::objectContentType(const KURL& url, const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return (ObjectContentType)[Mac(m_frame)->bridge() determineObjectFromMIMEType:mimeType URL:url.getNSURL()];
    END_BLOCK_OBJC_EXCEPTIONS;
    return ObjectContentNone;
}

static NSArray* nsArray(const Vector<String>& vector)
{
    unsigned len = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
    for (unsigned x = 0; x < len; x++)
        [array addObject:vector[x]];
    return array;
}

Widget* FrameLoader::createPlugin(Element* element, const KURL& url,
    const Vector<String>& paramNames, const Vector<String>& paramValues,
    const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return new Widget([Mac(m_frame)->bridge() viewForPluginWithURL:url.getNSURL()
                                  attributeNames:nsArray(paramNames)
                                  attributeValues:nsArray(paramValues)
                                  MIMEType:mimeType
                                  DOMElement:[DOMElement _elementWith:element]
                                loadManually:m_frame->document()->isPluginDocument()]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

void FrameLoader::redirectDataToPlugin(Widget* pluginWidget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [Mac(m_frame)->bridge() redirectDataToPlugin:pluginWidget->getView()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize& size, Element* element, const HashMap<String, String>& args)
{
    Widget* result = new Widget;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSMutableArray *attributeNames = [[NSMutableArray alloc] init];
    NSMutableArray *attributeValues = [[NSMutableArray alloc] init];
    
    DeprecatedString baseURLString;
    HashMap<String, String>::const_iterator end = args.end();
    for (HashMap<String, String>::const_iterator it = args.begin(); it != end; ++it) {
        if (it->first.lower() == "baseurl")
            baseURLString = it->second.deprecatedString();
        [attributeNames addObject:it->first];
        [attributeValues addObject:it->second];
    }
    
    if (baseURLString.isEmpty())
        baseURLString = m_frame->document()->baseURL();

    result->setView([Mac(m_frame)->bridge() viewForJavaAppletWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                                         attributeNames:attributeNames
                                        attributeValues:attributeValues
                                                baseURL:completeURL(baseURLString).getNSURL()
                                             DOMElement:[DOMElement _elementWith:element]]);
    [attributeNames release];
    [attributeValues release];
    m_frame->view()->addChild(result);
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return result;
}

void FrameLoader::partClearedInBegin()
{
    if (m_frame->settings()->isJavaScriptEnabled())
        [Mac(m_frame)->bridge() windowObjectCleared];
}

String FrameLoader::overrideMediaType() const
{
    NSString *overrideType = [Mac(m_frame)->bridge() overrideMediaType];
    if (overrideType)
        return overrideType;
    return String();
}

KURL FrameLoader::originalRequestURL() const
{
    return activeDocumentLoader()->initialRequest().url();
}

void FrameLoader::didFinishLoad(ResourceLoader* loader)
{    
    m_client->completeProgress(loader->identifier());
    m_client->dispatchDidFinishLoading(activeDocumentLoader(), loader->identifier());
}

void FrameLoader::setTitle(const String& title)
{
    documentLoader()->setTitle(title);
}

void FrameLoader::closeBridge()
{
    [Mac(m_frame)->bridge() close];
}


}
