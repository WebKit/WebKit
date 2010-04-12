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

// How we handle the base tag better.
// Current status:
// At now the normal way we use to handling base tag is
// a) For those links which have corresponding local saved files, such as
// savable CSS, JavaScript files, they will be written to relative URLs which
// point to local saved file. Why those links can not be resolved as absolute
// file URLs, because if they are resolved as absolute URLs, after moving the
// file location from one directory to another directory, the file URLs will
// be dead links.
// b) For those links which have not corresponding local saved files, such as
// links in A, AREA tags, they will be resolved as absolute URLs.
// c) We comment all base tags when serialzing DOM for the page.
// FireFox also uses above way to handle base tag.
//
// Problem:
// This way can not handle the following situation:
// the base tag is written by JavaScript.
// For example. The page "www.yahoo.com" use
// "document.write('<base href="http://www.yahoo.com/"...');" to setup base URL
// of page when loading page. So when saving page as completed-HTML, we assume
// that we save "www.yahoo.com" to "c:\yahoo.htm". After then we load the saved
// completed-HTML page, then the JavaScript will insert a base tag
// <base href="http://www.yahoo.com/"...> to DOM, so all URLs which point to
// local saved resource files will be resolved as
// "http://www.yahoo.com/yahoo_files/...", which will cause all saved  resource
// files can not be loaded correctly. Also the page will be rendered ugly since
// all saved sub-resource files (such as CSS, JavaScript files) and sub-frame
// files can not be fetched.
// Now FireFox, IE and WebKit based Browser all have this problem.
//
// Solution:
// My solution is that we comment old base tag and write new base tag:
// <base href="." ...> after the previous commented base tag. In WebKit, it
// always uses the latest "href" attribute of base tag to set document's base
// URL. Based on this behavior, when we encounter a base tag, we comment it and
// write a new base tag <base href="."> after the previous commented base tag.
// The new added base tag can help engine to locate correct base URL for
// correctly loading local saved resource files. Also I think we need to inherit
// the base target value from document object when appending new base tag.
// If there are multiple base tags in original document, we will comment all old
// base tags and append new base tag after each old base tag because we do not
// know those old base tags are original content or added by JavaScript. If
// they are added by JavaScript, it means when loading saved page, the script(s)
// will still insert base tag(s) to DOM, so the new added base tag(s) can
// override the incorrect base URL and make sure we alway load correct local
// saved resource files.

#include "config.h"
#include "WebPageSerializerImpl.h"

#include "Document.h"
#include "DocumentType.h"
#include "Element.h"
#include "FrameLoader.h"
#include "HTMLAllCollection.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "PlatformString.h"
#include "StringBuilder.h"
#include "TextEncoding.h"
#include "markup.h"

#include "DOMUtilitiesPrivate.h"
#include "WebFrameImpl.h"
#include "WebURL.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

// Maximum length of data buffer which is used to temporary save generated
// html content data. This is a soft limit which might be passed if a very large
// contegious string is found in the page.
static const unsigned dataBufferCapacity = 65536;

WebPageSerializerImpl::SerializeDomParam::SerializeDomParam(const KURL& currentFrameURL,
                                                            const TextEncoding& textEncoding,
                                                            Document* doc,
                                                            const String& directoryName)
    : currentFrameURL(currentFrameURL)
    , textEncoding(textEncoding)
    , doc(doc)
    , directoryName(directoryName)
    , hasDoctype(false)
    , hasCheckedMeta(false)
    , skipMetaElement(0)
    , isInScriptOrStyleTag(false)
    , hasDocDeclaration(false)
{
    // Cache the value since we check it lots of times.
    isHTMLDocument = doc->isHTMLDocument();
}

String WebPageSerializerImpl::preActionBeforeSerializeOpenTag(
    const Element* element, SerializeDomParam* param, bool* needSkip)
{
    StringBuilder result;

    *needSkip = false;
    if (param->isHTMLDocument) {
        // Skip the open tag of original META tag which declare charset since we
        // have overrided the META which have correct charset declaration after
        // serializing open tag of HEAD element.
        if (element->hasTagName(HTMLNames::metaTag)) {
            const HTMLMetaElement* meta = static_cast<const HTMLMetaElement*>(element);
            // Check whether the META tag has declared charset or not.
            String equiv = meta->httpEquiv();
            if (equalIgnoringCase(equiv, "content-type")) {
                String content = meta->content();
                if (content.length() && content.contains("charset", false)) {
                    // Find META tag declared charset, we need to skip it when
                    // serializing DOM.
                    param->skipMetaElement = element;
                    *needSkip = true;
                }
            }
        } else if (element->hasTagName(HTMLNames::htmlTag)) {
            // Check something before processing the open tag of HEAD element.
            // First we add doc type declaration if original doc has it.
            if (!param->hasDoctype) {
                param->hasDoctype = true;
                result.append(createMarkup(param->doc->doctype()));
            }

            // Add MOTW declaration before html tag.
            // See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
            result.append(WebPageSerializer::generateMarkOfTheWebDeclaration(param->currentFrameURL));
        } else if (element->hasTagName(HTMLNames::baseTag)) {
            // Comment the BASE tag when serializing dom.
            result.append("<!--");
        }
    } else {
        // Write XML declaration.
        if (!param->hasDocDeclaration) {
            param->hasDocDeclaration = true;
            // Get encoding info.
            String xmlEncoding = param->doc->xmlEncoding();
            if (xmlEncoding.isEmpty())
                xmlEncoding = param->doc->frame()->loader()->encoding();
            if (xmlEncoding.isEmpty())
                xmlEncoding = UTF8Encoding().name();
            result.append("<?xml version=\"");
            result.append(param->doc->xmlVersion());
            result.append("\" encoding=\"");
            result.append(xmlEncoding);
            if (param->doc->xmlStandalone())
                result.append("\" standalone=\"yes");
            result.append("\"?>\n");
        }
        // Add doc type declaration if original doc has it.
        if (!param->hasDoctype) {
            param->hasDoctype = true;
            result.append(createMarkup(param->doc->doctype()));
        }
    }
    return result.toString();
}

String WebPageSerializerImpl::postActionAfterSerializeOpenTag(
    const Element* element, SerializeDomParam* param)
{
    StringBuilder result;

    param->hasAddedContentsBeforeEnd = false;
    if (!param->isHTMLDocument)
        return result.toString();
    // Check after processing the open tag of HEAD element
    if (!param->hasCheckedMeta
        && element->hasTagName(HTMLNames::headTag)) {
        param->hasCheckedMeta = true;
        // Check meta element. WebKit only pre-parse the first 512 bytes
        // of the document. If the whole <HEAD> is larger and meta is the
        // end of head part, then this kind of pages aren't decoded correctly
        // because of this issue. So when we serialize the DOM, we need to
        // make sure the meta will in first child of head tag.
        // See http://bugs.webkit.org/show_bug.cgi?id=16621.
        // First we generate new content for writing correct META element.
        result.append(WebPageSerializer::generateMetaCharsetDeclaration(
            String(param->textEncoding.name())));

        param->hasAddedContentsBeforeEnd = true;
        // Will search each META which has charset declaration, and skip them all
        // in PreActionBeforeSerializeOpenTag.
    } else if (element->hasTagName(HTMLNames::scriptTag)
               || element->hasTagName(HTMLNames::styleTag)) {
        param->isInScriptOrStyleTag = true;
    }

    return result.toString();
}

String WebPageSerializerImpl::preActionBeforeSerializeEndTag(
    const Element* element, SerializeDomParam* param, bool* needSkip)
{
    String result;

    *needSkip = false;
    if (!param->isHTMLDocument)
        return result;
    // Skip the end tag of original META tag which declare charset.
    // Need not to check whether it's META tag since we guarantee
    // skipMetaElement is definitely META tag if it's not 0.
    if (param->skipMetaElement == element)
        *needSkip = true;
    else if (element->hasTagName(HTMLNames::scriptTag)
             || element->hasTagName(HTMLNames::styleTag)) {
        ASSERT(param->isInScriptOrStyleTag);
        param->isInScriptOrStyleTag = false;
    }

    return result;
}

// After we finish serializing end tag of a element, we give the target
// element a chance to do some post work to add some additional data.
String WebPageSerializerImpl::postActionAfterSerializeEndTag(
    const Element* element, SerializeDomParam* param)
{
    StringBuilder result;

    if (!param->isHTMLDocument)
        return result.toString();
    // Comment the BASE tag when serializing DOM.
    if (element->hasTagName(HTMLNames::baseTag)) {
        result.append("-->");
        // Append a new base tag declaration.
        result.append(WebPageSerializer::generateBaseTagDeclaration(
            param->doc->baseTarget()));
    }

    return result.toString();
}

void WebPageSerializerImpl::saveHTMLContentToBuffer(
    const String& result, SerializeDomParam* param)
{
    m_dataBuffer.append(result);
    encodeAndFlushBuffer(WebPageSerializerClient::CurrentFrameIsNotFinished,
                         param,
                         0);
}

void WebPageSerializerImpl::encodeAndFlushBuffer(
    WebPageSerializerClient::PageSerializationStatus status,
    SerializeDomParam* param,
    bool force)
{
    // Data buffer is not full nor do we want to force flush.
    if (!force && m_dataBuffer.length() <= dataBufferCapacity)
        return;

    String content = m_dataBuffer.toString();
    m_dataBuffer.clear();

    // Convert the unicode content to target encoding
    CString encodedContent = param->textEncoding.encode(
        content.characters(), content.length(), EntitiesForUnencodables);

    // Send result to the client.
    m_client->didSerializeDataForFrame(param->currentFrameURL,
                                       WebCString(encodedContent.data(), encodedContent.length()),
                                       status);
}

void WebPageSerializerImpl::openTagToString(const Element* element,
                                            SerializeDomParam* param)
{
    // FIXME: use StringBuilder instead of String.
    bool needSkip;
    // Do pre action for open tag.
    String result = preActionBeforeSerializeOpenTag(element, param, &needSkip);
    if (needSkip)
        return;
    // Add open tag
    result += "<" + element->nodeName();
    // Go through all attributes and serialize them.
    const NamedNodeMap *attrMap = element->attributes(true);
    if (attrMap) {
        unsigned numAttrs = attrMap->length();
        for (unsigned i = 0; i < numAttrs; i++) {
            result += " ";
            // Add attribute pair
            const Attribute *attribute = attrMap->attributeItem(i);
            result += attribute->name().toString();
            result += "=\"";
            if (!attribute->value().isEmpty()) {
                const String& attrValue = attribute->value();

                // Check whether we need to replace some resource links
                // with local resource paths.
                const QualifiedName& attrName = attribute->name();
                if (elementHasLegalLinkAttribute(element, attrName)) {
                    // For links start with "javascript:", we do not change it.
                    if (attrValue.startsWith("javascript:", false))
                        result += attrValue;
                    else {
                        // Get the absolute link
                        String completeURL = param->doc->completeURL(attrValue);
                        // Check whether we have local files for those link.
                        if (m_localLinks.contains(completeURL)) {
                            if (!m_localDirectoryName.isEmpty())
                                result += "./" + m_localDirectoryName + "/";
                            result += m_localLinks.get(completeURL);
                        } else
                            result += completeURL;
                    }
                } else {
                    if (param->isHTMLDocument)
                        result += m_htmlEntities.convertEntitiesInString(attrValue);
                    else
                        result += m_xmlEntities.convertEntitiesInString(attrValue);
                }
            }
            result += "\"";
        }
    }

    // Do post action for open tag.
    String addedContents = postActionAfterSerializeOpenTag(element, param);
    // Complete the open tag for element when it has child/children.
    if (element->hasChildNodes() || param->hasAddedContentsBeforeEnd)
        result += ">";
    // Append the added contents generate in  post action of open tag.
    result += addedContents;
    // Save the result to data buffer.
    saveHTMLContentToBuffer(result, param);
}

// Serialize end tag of an specified element.
void WebPageSerializerImpl::endTagToString(const Element* element,
                                           SerializeDomParam* param)
{
    bool needSkip;
    // Do pre action for end tag.
    String result = preActionBeforeSerializeEndTag(element,
                                                   param,
                                                   &needSkip);
    if (needSkip)
        return;
    // Write end tag when element has child/children.
    if (element->hasChildNodes() || param->hasAddedContentsBeforeEnd) {
        result += "</";
        result += element->nodeName();
        result += ">";
    } else {
        // Check whether we have to write end tag for empty element.
        if (param->isHTMLDocument) {
            result += ">";
            const HTMLElement* htmlElement =
            static_cast<const HTMLElement*>(element);
            if (htmlElement->endTagRequirement() == TagStatusRequired) {
                // We need to write end tag when it is required.
                result += "</";
                result += element->nodeName();
                result += ">";
            }
        } else {
            // For xml base document.
            result += " />";
        }
    }
    // Do post action for end tag.
    result += postActionAfterSerializeEndTag(element, param);
    // Save the result to data buffer.
    saveHTMLContentToBuffer(result, param);
}

void WebPageSerializerImpl::buildContentForNode(const Node* node,
                                                SerializeDomParam* param)
{
    switch (node->nodeType()) {
    case Node::ELEMENT_NODE:
        // Process open tag of element.
        openTagToString(static_cast<const Element*>(node), param);
        // Walk through the children nodes and process it.
        for (const Node *child = node->firstChild(); child; child = child->nextSibling())
            buildContentForNode(child, param);
        // Process end tag of element.
        endTagToString(static_cast<const Element*>(node), param);
        break;
    case Node::TEXT_NODE:
        saveHTMLContentToBuffer(createMarkup(node), param);
        break;
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        // Should not exist.
        ASSERT_NOT_REACHED();
        break;
    // Document type node can be in DOM?
    case Node::DOCUMENT_TYPE_NODE:
        param->hasDoctype = true;
    default:
        // For other type node, call default action.
        saveHTMLContentToBuffer(createMarkup(node), param);
        break;
    }
}

WebPageSerializerImpl::WebPageSerializerImpl(WebFrame* frame,
                                             bool recursiveSerialization,
                                             WebPageSerializerClient* client,
                                             const WebVector<WebURL>& links,
                                             const WebVector<WebString>& localPaths,
                                             const WebString& localDirectoryName)
    : m_client(client)
    , m_recursiveSerialization(recursiveSerialization)
    , m_framesCollected(false)
    , m_localDirectoryName(localDirectoryName)
    , m_htmlEntities(false)
    , m_xmlEntities(true)
{
    // Must specify available webframe.
    ASSERT(frame);
    m_specifiedWebFrameImpl = static_cast<WebFrameImpl*>(frame);
    // Make sure we have non 0 client.
    ASSERT(client);
    // Build local resources map.
    ASSERT(links.size() == localPaths.size());
    for (size_t i = 0; i < links.size(); i++) {
        KURL url = links[i];
        ASSERT(!m_localLinks.contains(url.string()));
        m_localLinks.set(url.string(), localPaths[i]);
    }

    ASSERT(!m_dataBuffer.length());
}

void WebPageSerializerImpl::collectTargetFrames()
{
    ASSERT(!m_framesCollected);
    m_framesCollected = true;

    // First, process main frame.
    m_frames.append(m_specifiedWebFrameImpl);
    // Return now if user only needs to serialize specified frame, not including
    // all sub-frames.
    if (!m_recursiveSerialization)
        return;
    // Collect all frames inside the specified frame.
    for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
        WebFrameImpl* currentFrame = m_frames[i];
        // Get current using document.
        Document* currentDoc = currentFrame->frame()->document();
        // Go through sub-frames.
        RefPtr<HTMLAllCollection> all = currentDoc->all();
        for (Node* node = all->firstItem(); node; node = all->nextItem()) {
            if (!node->isHTMLElement())
                continue;
            Element* element = static_cast<Element*>(node);
            WebFrameImpl* webFrame =
                WebFrameImpl::fromFrameOwnerElement(element);
            if (webFrame)
                m_frames.append(webFrame);
        }
    }
}

bool WebPageSerializerImpl::serialize()
{
    // Collect target frames.
    if (!m_framesCollected)
        collectTargetFrames();
    bool didSerialization = false;
    // Get KURL for main frame.
    KURL mainPageURL = m_specifiedWebFrameImpl->frame()->loader()->url();

    // Go through all frames for serializing DOM for whole page, include
    // sub-frames.
    for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
        // Get current serializing frame.
        WebFrameImpl* currentFrame = m_frames[i];
        // Get current using document.
        Document* currentDoc = currentFrame->frame()->document();
        // Get current frame's URL.
        const KURL& currentFrameURL = currentFrame->frame()->loader()->url();

        // Check whether we have done this document.
        if (m_localLinks.contains(currentFrameURL.string())) {
            // A new document, we will serialize it.
            didSerialization = true;
            // Get target encoding for current document.
            String encoding = currentFrame->frame()->loader()->encoding();
            // Create the text encoding object with target encoding.
            TextEncoding textEncoding(encoding);
            // Construct serialize parameter for late processing document.
            SerializeDomParam param(currentFrameURL,
                                    encoding.length() ? textEncoding : UTF8Encoding(),
                                    currentDoc,
                                    currentFrameURL == mainPageURL ? m_localDirectoryName : "");

            // Process current document.
            Element* rootElement = currentDoc->documentElement();
            if (rootElement)
                buildContentForNode(rootElement, &param);

            // Flush the remainder data and finish serializing current frame.
            encodeAndFlushBuffer(WebPageSerializerClient::CurrentFrameIsFinished,
                                 &param,
                                 1);
        }
    }

    // We have done call frames, so we send message to embedder to tell it that
    // frames are finished serializing.
    ASSERT(!m_dataBuffer.length());
    m_client->didSerializeDataForFrame(KURL(),
                                       WebCString("", 0),
                                       WebPageSerializerClient::AllFramesAreFinished);
    return didSerialization;
}

}  // namespace WebKit
