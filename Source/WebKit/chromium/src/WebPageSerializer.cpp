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

#include "CSSFontFaceRule.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSImageValue.h"
#include "CSSImportRule.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLAllCollection.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "KURL.h"
#include "KURLHash.h"
#include "ListHashSet.h"
#include "StyleCachedImage.h"
#include "StyleImage.h"
#include "StyleSheetList.h"
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

#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace {

void retrieveResourcesForCSSStyleDeclaration(CSSStyleDeclaration*,
                                             ListHashSet<KURL>* resourceURLs);

const QualifiedName* getResourceAttributeForElement(HTMLElement* element)
{
    if (element->hasTagName(HTMLNames::imgTag) || element->hasTagName(HTMLNames::scriptTag))
        return &HTMLNames::srcAttr;

    if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        if (input->isImageButton())
            return &HTMLNames::srcAttr;
    }

    if (element->hasTagName(HTMLNames::bodyTag)
        || element->hasTagName(HTMLNames::tableTag)
        || element->hasTagName(HTMLNames::trTag)
        || element->hasTagName(HTMLNames::tdTag))
        return &HTMLNames::backgroundAttr;

    if (element->hasTagName(HTMLNames::blockquoteTag)
        || element->hasTagName(HTMLNames::qTag)
        || element->hasTagName(HTMLNames::delTag)
        || element->hasTagName(HTMLNames::insTag))
        return &HTMLNames::citeAttr;

    if (element->hasTagName(HTMLNames::objectTag))
        return &HTMLNames::dataAttr;

    if (element->hasTagName(HTMLNames::iframeTag)
        || element->hasTagName(HTMLNames::frameTag)
        || element->hasTagName(HTMLNames::embedTag))
        return &HTMLNames::srcAttr;

    if (element->hasTagName(HTMLNames::linkTag)
        && equalIgnoringCase(element->getAttribute(HTMLNames::typeAttr), "text/css"))
        return &HTMLNames::hrefAttr;

    return 0;
}

void retrieveStyleSheetForElement(HTMLElement* element,
                                  ListHashSet<CSSStyleSheet*>* styleSheets)
{
    if (element->hasTagName(HTMLNames::linkTag)) {
        HTMLLinkElement* linkElement = static_cast<HTMLLinkElement*>(element);
        // We are only interested in CSS links.
        if (equalIgnoringCase(element->getAttribute(HTMLNames::typeAttr), "text/css")) {
            StyleSheet* sheet = linkElement->sheet();
            if (sheet && sheet->isCSSStyleSheet())
                styleSheets->add(static_cast<CSSStyleSheet*>(sheet));
        }
        return;
    } 
    if (element->hasTagName(HTMLNames::styleTag)) {
        HTMLStyleElement* styleElement = static_cast<HTMLStyleElement*>(element);
        StyleSheet* sheet = styleElement->sheet();
        if (sheet && sheet->isCSSStyleSheet())
            styleSheets->add(static_cast<CSSStyleSheet*>(sheet));
    }
}

void retrieveResourcesForElement(HTMLElement* element,
                                 const WebKit::WebVector<WebKit::WebCString>& supportedSchemes,
                                 ListHashSet<Frame*>* visitedFrames,
                                 Vector<Frame*>* framesToVisit,
                                 ListHashSet<CSSStyleSheet*>* styleSheets,
                                 ListHashSet<KURL>* frameURLs,
                                 ListHashSet<KURL>* resourceURLs)
{
    ASSERT(element);
    if ((element->hasTagName(HTMLNames::iframeTag) || element->hasTagName(HTMLNames::frameTag)
         || element->hasTagName(HTMLNames::objectTag) || element->hasTagName(HTMLNames::embedTag))
        && element->isFrameOwnerElement()) {
        Frame* frame = static_cast<const HTMLFrameOwnerElement*>(element)->contentFrame();
        if (frame) {
            if (!visitedFrames->contains(frame))
                framesToVisit->append(frame);
            return;
        }
    }

    const QualifiedName* attribute = getResourceAttributeForElement(element);
    if (attribute) {
        String value = element->getAttribute(*attribute);
        if (!value.isEmpty()) {
            KURL url = element->document()->completeURL(value);
            // Ignore URLs that have a non-standard protocols. Since the FTP protocol
            // does not have a cache mechanism, we skip it as well.
            if (url.isValid() && (url.protocolInHTTPFamily() || url.isLocalFile()))
                resourceURLs->add(url);
        }
    }

    retrieveStyleSheetForElement(element, styleSheets);
    
    // Process in-line style.
    if (CSSStyleDeclaration* styleDeclaration = element->style())
        retrieveResourcesForCSSStyleDeclaration(styleDeclaration, resourceURLs);
}

void retrieveResourcesForFrame(Frame* frame,
                               const WebKit::WebVector<WebKit::WebCString>& supportedSchemes,
                               ListHashSet<Frame*>* visitedFrames,
                               Vector<Frame*>* framesToVisit,
                               ListHashSet<CSSStyleSheet*>* styleSheets,
                               ListHashSet<KURL>* frameURLs,
                               ListHashSet<KURL>* resourceURLs)
{
    if (!visitedFrames->add(frame).second)
        return; // We have already seen that frame.

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

    frameURLs->add(frameURL);
  
    // Now get the resources associated with each node of the document.
    RefPtr<HTMLAllCollection> allNodes = frame->document()->all();
    for (unsigned i = 0; i < allNodes->length(); ++i) {
        // We are only interested in HTML resources.
        if (HTMLElement* element = toHTMLElement(allNodes->item(i))) {
            retrieveResourcesForElement(element, supportedSchemes,
                                        visitedFrames, framesToVisit,
                                        styleSheets, frameURLs, resourceURLs);
        }
    }
}

void retrieveResourcesForCSSRule(CSSStyleRule* rule,
                                 ListHashSet<KURL>* resourceURLs)
{
    if (rule->style())
        retrieveResourcesForCSSStyleDeclaration(rule->style(), resourceURLs);
}

void retrieveResourcesForCSSStyleDeclaration(CSSStyleDeclaration* styleDeclaration,
                                             ListHashSet<KURL>* resourceURLs)
{
    // The background-image and list-style-image (for ul or ol) are the CSS properties
    // that make use of images. We iterate to make sure we include any other
    // image properties there might be.
    for (unsigned i = 0; i < styleDeclaration->length(); ++i) {
        // FIXME: it's kind of ridiculous to get the property name and then get
        // the value out of the name. Ideally we would get the value out of the
        // property ID, but CSSStyleDeclaration only gives access to property
        // names, not IDs.
        RefPtr<CSSValue> value = styleDeclaration->getPropertyCSSValue(styleDeclaration->item(i));
        if (value->isImageValue()) {
            CSSImageValue* imageValue = static_cast<CSSImageValue*>(value.get());
            StyleImage* styleImage = imageValue->cachedOrPendingImage();
            // Non cached-images are just place-holders and do not contain data.
            if (styleImage->isCachedImage()) {
                StyleSheet* styleSheet = styleDeclaration->stylesheet();
                if (styleSheet->isCSSStyleSheet()) {
                    String url = static_cast<StyleCachedImage*>(styleImage)->cachedImage()->url();
                    resourceURLs->add(static_cast<CSSStyleSheet*>(styleSheet)->document()->completeURL(url));
                }
            }
        }
    }
}

void retrieveResourcesForCSSStyleSheet(CSSStyleSheet* styleSheet,
                                       ListHashSet<CSSStyleSheet*>* visitedStyleSheets,
                                       const WebKit::WebVector<WebKit::WebCString>& supportedSchemes,
                                       ListHashSet<KURL>* resourceURLs)
{
    if (!styleSheet)
        return;

    if (!visitedStyleSheets->add(styleSheet).second)
        return; // We have already seen that styleSheet.

    // Parse the styles.
    for (unsigned i = 0; i < styleSheet->length(); ++i) {
        StyleBase* item = styleSheet->item(i);
        if (!item)
            continue;

       if (item->isImportRule()) {
            CSSImportRule* importRule = static_cast<CSSImportRule*>(item);
            // The imported CSS file itself is a resource.
            resourceURLs->add(styleSheet->document()->completeURL(importRule->href()));
            // And it may contain some more resources.
            retrieveResourcesForCSSStyleSheet(importRule->styleSheet(), visitedStyleSheets, supportedSchemes, resourceURLs);
        } else if (item->isFontFaceRule()) {
            CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
            RefPtr<CSSValue> cssValue = fontFaceRule->style()->getPropertyCSSValue(CSSPropertySrc);
            if (cssValue->isValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(cssValue.get());
                for (unsigned j = 0; j < valueList->length(); ++j) {
                    // Note that there does not seem to be a way to ensure the value in the list is a CSSFontFaceSrcValue.
                    // We do trust that list only contains CSSFontFaceSrcValues as done in WebCore/css/CSSFontSelector.cpp
                    CSSFontFaceSrcValue* fontFaceSrc = static_cast<CSSFontFaceSrcValue*>(valueList->item(j));
                    resourceURLs->add(styleSheet->document()->completeURL(fontFaceSrc->resource()));
                }          
            }
        } else if (item->isStyleRule())
            retrieveResourcesForCSSRule(static_cast<CSSStyleRule*>(item), resourceURLs);
    }
}

} // namespace

namespace WebKit {

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
    ListHashSet<Frame*> visitedFrames;
    ListHashSet<CSSStyleSheet*> styleSheets;
    ListHashSet<KURL> frameKURLs;
    ListHashSet<KURL> resourceKURLs;
    
    // Let's retrieve the resources from every frame in this page.
    framesToVisit.append(mainFrame->frame());
    while (!framesToVisit.isEmpty()) {
        Frame* frame = framesToVisit[0];
        framesToVisit.remove(0);
        retrieveResourcesForFrame(frame, supportedSchemes,
                                  &visitedFrames, &framesToVisit, &styleSheets,
                                  &frameKURLs, &resourceKURLs);
     }

    // While retrieving the frame resources, we also retrieved the CSS style-sheets,
    // we can process them now.
    ListHashSet<CSSStyleSheet*> visitedStyleSheets;
    for (ListHashSet<CSSStyleSheet*>::const_iterator iter = styleSheets.begin();
         iter != styleSheets.end(); ++iter) {
        retrieveResourcesForCSSStyleSheet(*iter, &visitedStyleSheets,
                                          supportedSchemes, &resourceKURLs);
    }

    // Converts the results to WebURLs.
    WebVector<WebURL> resultResourceURLs(static_cast<size_t>(resourceKURLs.size()));
    int i = 0;
    for (ListHashSet<KURL>::const_iterator iter = resourceKURLs.begin();
         iter != resourceKURLs.end(); ++iter, ++i) {
        KURL url = *iter;
        resultResourceURLs[i] = url;
        // A frame's src can point to the same URL as another resource, keep the
        // resource URL only in such cases.
        frameKURLs.remove(url);
    }
    *resourceURLs = resultResourceURLs;
    WebVector<WebURL> resultFrameURLs(static_cast<size_t>(frameKURLs.size()));
    i = 0;
    for (ListHashSet<KURL>::const_iterator iter = frameKURLs.begin();
         iter != frameKURLs.end(); ++iter, ++i)
        resultFrameURLs[i] = *iter;

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
