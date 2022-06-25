/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKDOMInternals.h"

#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/Node.h>
#import <WebCore/Range.h>
#import <WebCore/Text.h>
#import <wtf/NeverDestroyed.h>

// Classes to instantiate.
#import "WKDOMElement.h"
#import "WKDOMDocument.h"
#import "WKDOMText.h"

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#endif

namespace WebKit {

template<typename WebCoreType, typename WKDOMType>
static WKDOMType toWKDOMType(WebCoreType impl, DOMCache<WebCoreType, WKDOMType>& cache);

// -- Caches -- 

DOMCache<WebCore::Node*, __unsafe_unretained WKDOMNode *>& WKDOMNodeCache()
{
    static NeverDestroyed<DOMCache<WebCore::Node*, __unsafe_unretained WKDOMNode *>> cache;
    return cache;
}

DOMCache<WebCore::Range*, __unsafe_unretained WKDOMRange *>& WKDOMRangeCache()
{
    static NeverDestroyed<DOMCache<WebCore::Range*, __unsafe_unretained WKDOMRange *>> cache;
    return cache;
}

// -- Node and classes derived from Node. --

static Class WKDOMNodeClass(WebCore::Node* impl)
{
    switch (impl->nodeType()) {
    case WebCore::Node::ELEMENT_NODE:
        return [WKDOMElement class];
    case WebCore::Node::DOCUMENT_NODE:
        return [WKDOMDocument class];
    case WebCore::Node::TEXT_NODE:
        return [WKDOMText class];
    case WebCore::Node::ATTRIBUTE_NODE:
    case WebCore::Node::CDATA_SECTION_NODE:
    case WebCore::Node::PROCESSING_INSTRUCTION_NODE:
    case WebCore::Node::COMMENT_NODE:
    case WebCore::Node::DOCUMENT_TYPE_NODE:
    case WebCore::Node::DOCUMENT_FRAGMENT_NODE:
        return [WKDOMNode class];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

static RetainPtr<WKDOMNode> createWrapper(WebCore::Node* impl)
{
    return adoptNS([[WKDOMNodeClass(impl) alloc] _initWithImpl:impl]);
}

WebCore::Node* toWebCoreNode(WKDOMNode *wrapper)
{
    return wrapper ? wrapper->_impl.get() : 0;
}

WKDOMNode *toWKDOMNode(WebCore::Node* impl)
{
    return toWKDOMType<WebCore::Node*, __unsafe_unretained WKDOMNode *>(impl, WKDOMNodeCache());
}

WebCore::Element* toWebCoreElement(WKDOMElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Element*>(wrapper->_impl.get()) : 0;
}

WKDOMElement *toWKDOMElement(WebCore::Element* impl)
{
    return static_cast<WKDOMElement*>(toWKDOMNode(static_cast<WebCore::Node*>(impl)));
}

WebCore::Document* toWebCoreDocument(WKDOMDocument *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Document*>(wrapper->_impl.get()) : 0;
}

WKDOMDocument *toWKDOMDocument(WebCore::Document* impl)
{
    return static_cast<WKDOMDocument*>(toWKDOMNode(static_cast<WebCore::Node*>(impl)));
}

WebCore::Text* toWebCoreText(WKDOMText *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Text*>(wrapper->_impl.get()) : 0;
}

WKDOMText *toWKDOMText(WebCore::Text* impl)
{
    return static_cast<WKDOMText*>(toWKDOMNode(static_cast<WebCore::Node*>(impl)));
}

// -- Range. --

static RetainPtr<WKDOMRange> createWrapper(WebCore::Range* impl)
{
    return adoptNS([[WKDOMRange alloc] _initWithImpl:impl]);
}

WebCore::Range* toWebCoreRange(WKDOMRange * wrapper)
{
    return wrapper ? wrapper->_impl.get() : 0;
}

WKDOMRange *toWKDOMRange(WebCore::Range* impl)
{
    return toWKDOMType<WebCore::Range*, __unsafe_unretained WKDOMRange *>(impl, WKDOMRangeCache());
}

// -- Helpers --

template<typename WebCoreType, typename WKDOMType>
static WKDOMType toWKDOMType(WebCoreType impl, DOMCache<WebCoreType, WKDOMType>& cache)
{
    if (!impl)
        return nil;
    if (WKDOMType wrapper = cache.get(impl))
        return retainPtr(wrapper).autorelease();
    auto wrapper = createWrapper(impl);
    if (!wrapper)
        return nil;
    return wrapper.autorelease();
}

} // namespace WebKit
