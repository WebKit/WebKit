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
#include "WebPageSerializer.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLAllCollection.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "PageSerializer.h"
#include "Vector.h"

#include "WebCString.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebPageSerializerClient.h"
#include "WebPageSerializerImpl.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"
#include "WebView.h"
#include "WebViewImpl.h"

#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace {

KURL getSubResourceURLFromElement(Element* element)
{
    ASSERT(element);
    const QualifiedName* attributeName = 0;
    if (element->hasTagName(HTMLNames::imgTag) || element->hasTagName(HTMLNames::scriptTag))
        attributeName = &HTMLNames::srcAttr;
    else if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        if (input->isImageButton())
            attributeName = &HTMLNames::srcAttr;
    } else if (element->hasTagName(HTMLNames::bodyTag)
               || element->hasTagName(HTMLNames::tableTag)
               || element->hasTagName(HTMLNames::trTag)
               || element->hasTagName(HTMLNames::tdTag))
        attributeName = &HTMLNames::backgroundAttr;
    else if (element->hasTagName(HTMLNames::blockquoteTag)
             || element->hasTagName(HTMLNames::qTag)
             || element->hasTagName(HTMLNames::delTag)
             || element->hasTagName(HTMLNames::insTag))
        attributeName = &HTMLNames::citeAttr;
    else if (element->hasTagName(HTMLNames::linkTag)) {
        // If the link element is not css, ignore it.
        if (equalIgnoringCase(element->getAttribute(HTMLNames::typeAttr), "text/css")) {
            // FIXME: Add support for extracting links of sub-resources which
            // are inside style-sheet such as @import, @font-face, url(), etc.
            attributeName = &HTMLNames::hrefAttr;
        }
    } else if (element->hasTagName(HTMLNames::objectTag))
        attributeName = &HTMLNames::dataAttr;
    else if (element->hasTagName(HTMLNames::embedTag))
        attributeName = &HTMLNames::srcAttr;

    if (!attributeName)
        return KURL();

    String value = element->getAttribute(*attributeName);
    // Ignore javascript content.
    if (value.isEmpty() || value.stripWhiteSpace().startsWith("javascript:", false))
        return KURL();
  
    return element->document()->completeURL(value);
}

void retrieveResourcesForElement(Element* element,
                                 Vector<Frame*>* visitedFrames,
                                 Vector<Frame*>* framesToVisit,
                                 Vector<KURL>* frameURLs,
                                 Vector<KURL>* resourceURLs)
{
    // If the node is a frame, we'll process it later in retrieveResourcesForFrame.
    if ((element->hasTagName(HTMLNames::iframeTag) || element->hasTagName(HTMLNames::frameTag)
        || element->hasTagName(HTMLNames::objectTag) || element->hasTagName(HTMLNames::embedTag))
            && element->isFrameOwnerElement()) {
        Frame* frame = static_cast<HTMLFrameOwnerElement*>(element)->contentFrame();
        if (frame) {
            if (!visitedFrames->contains(frame))
                framesToVisit->append(frame);
            return;
        }
    }

    KURL url = getSubResourceURLFromElement(element);
    if (url.isEmpty() || !url.isValid())
        return; // No subresource for this node.

    // Ignore URLs that have a non-standard protocols. Since the FTP protocol
    // does no have a cache mechanism, we skip it as well.
    if (!url.protocolInHTTPFamily() && !url.isLocalFile())
        return;

    if (!resourceURLs->contains(url))
        resourceURLs->append(url);
}

void retrieveResourcesForFrame(Frame* frame,
                               const WebKit::WebVector<WebKit::WebCString>& supportedSchemes,
                               Vector<Frame*>* visitedFrames,
                               Vector<Frame*>* framesToVisit,
                               Vector<KURL>* frameURLs,
                               Vector<KURL>* resourceURLs)
{
    KURL frameURL = frame->loader()->documentLoader()->request().url();

    // If the frame's URL is invalid, ignore it, it is not retrievable.
    if (!frameURL.isValid())
        return;

    // Ignore frames from unsupported schemes.
    bool isValidScheme = false;
    for (size_t i = 0; i < supportedSchemes.size(); ++i) {
        if (frameURL.protocolIs(static_cast<CString>(supportedSchemes[i]).data())) {
            isValidScheme = true;
            break;
        }
    }
    if (!isValidScheme)
        return;

    // If we have already seen that frame, ignore it.
    if (visitedFrames->contains(frame))
        return;
    visitedFrames->append(frame);
    if (!frameURLs->contains(frameURL))
        frameURLs->append(frameURL);
  
    // Now get the resources associated with each node of the document.
    RefPtr<HTMLAllCollection> allNodes = frame->document()->all();
    for (unsigned i = 0; i < allNodes->length(); ++i) {
        Node* node = allNodes->item(i);
        // We are only interested in HTML resources.
        if (!node->isElementNode())
            continue;
        retrieveResourcesForElement(static_cast<Element*>(node),
                                    visitedFrames, framesToVisit,
                                    frameURLs, resourceURLs);
    }
}

} // namespace

namespace WebKit {

void WebPageSerializer::serialize(WebView* view, WebVector<WebPageSerializer::Resource>* resourcesParam)
{
    Vector<PageSerializer::Resource> resources;
    PageSerializer serializer(&resources);
    serializer.serialize(static_cast<WebViewImpl*>(view)->page());

    Vector<Resource> result;
    for (Vector<PageSerializer::Resource>::const_iterator iter = resources.begin(); iter != resources.end(); ++iter) {
        Resource resource;
        resource.url = iter->url;
        resource.mimeType = iter->mimeType.ascii();
        // FIXME: we are copying all the resource data here. Idealy we would have a WebSharedData().
        resource.data = WebCString(iter->data->data(), iter->data->size());
        result.append(resource);
    }

    *resourcesParam = result;         
}

bool WebPageSerializer::serialize(WebFrame* frame,
                                  bool recursive,
                                  WebPageSerializerClient* client,
                                  const WebVector<WebURL>& links,
                                  const WebVector<WebString>& localPaths,
                                  const WebString& localDirectoryName)
{
    WebPageSerializerImpl serializerImpl(
        frame, recursive, client, links, localPaths, localDirectoryName);
    return serializerImpl.serialize();
}

bool WebPageSerializer::retrieveAllResources(WebView* view,
                                             const WebVector<WebCString>& supportedSchemes,
                                             WebVector<WebURL>* resourceURLs,
                                             WebVector<WebURL>* frameURLs) {
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(view->mainFrame());
    if (!mainFrame)
        return false;

    Vector<Frame*> framesToVisit;
    Vector<Frame*> visitedFrames;
    Vector<KURL> frameKURLs;
    Vector<KURL> resourceKURLs;
    
    // Let's retrieve the resources from every frame in this page.
    framesToVisit.append(mainFrame->frame());
    while (!framesToVisit.isEmpty()) {
        Frame* frame = framesToVisit[0];
        framesToVisit.remove(0);
        retrieveResourcesForFrame(frame, supportedSchemes,
                                  &visitedFrames, &framesToVisit,
                                  &frameKURLs, &resourceKURLs);
    }

    // Converts the results to WebURLs.
    WebVector<WebURL> resultResourceURLs(resourceKURLs.size());
    for (size_t i = 0; i < resourceKURLs.size(); ++i) {
        resultResourceURLs[i] = resourceKURLs[i];
        // A frame's src can point to the same URL as another resource, keep the
        // resource URL only in such cases.
        size_t index = frameKURLs.find(resourceKURLs[i]);
        if (index != notFound)
            frameKURLs.remove(index);
    }
    *resourceURLs = resultResourceURLs;
    WebVector<WebURL> resultFrameURLs(frameKURLs.size());
    for (size_t i = 0; i < frameKURLs.size(); ++i)
        resultFrameURLs[i] = frameKURLs[i];
    *frameURLs = resultFrameURLs;
    
    return true;
}

WebString WebPageSerializer::generateMetaCharsetDeclaration(const WebString& charset)
{
    return makeString("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=", static_cast<const String&>(charset), "\">");
}

WebString WebPageSerializer::generateMarkOfTheWebDeclaration(const WebURL& url)
{
    return String::format("\n<!-- saved from url=(%04d)%s -->\n",
                          static_cast<int>(url.spec().length()),
                          url.spec().data());
}

WebString WebPageSerializer::generateBaseTagDeclaration(const WebString& baseTarget)
{
    if (baseTarget.isEmpty())
        return makeString("<base href=\".\">");
    return makeString("<base href=\".\" target=\"", static_cast<const String&>(baseTarget), "\">");
}

} // namespace WebKit
