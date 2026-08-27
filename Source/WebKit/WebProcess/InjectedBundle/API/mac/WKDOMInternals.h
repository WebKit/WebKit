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

#import <WebCore/Node.h>
#import <WebCore/Range.h>
#import <WebKit/WKDOMNode.h>
#import <WebKit/WKDOMRange.h>
#import <wtf/HashMap.h>

namespace WebCore {
class Element;
class Document;
}

@class WKDOMElement;
@class WKDOMDocument;
@class WKDOMText;

@interface WKDOMNode () {
@package
    RefPtr<WebCore::Node> _impl;
}

- (id)_initWithImpl:(WebCore::Node*)impl;
@end

@interface WKDOMRange () {
@package
    RefPtr<WebCore::Range> _impl;
}

- (id)_initWithImpl:(WebCore::Range*)impl;
@end

namespace WebKit {

template<typename WebCoreType, typename WKDOMType>
class DOMCache {
public:
    void add(WebCoreType core, WKDOMType kit)
    {
        m_map.add(core, kit);
    }
    
    WKDOMType get(WebCoreType core)
    {
        return m_map.get(core);
    }

    void remove(WebCoreType core)
    {
        m_map.remove(core);
    }

private:
    // This class should only ever be used as a singleton.
    ~DOMCache() = delete;

    HashMap<WebCoreType, WKDOMType> m_map;
};

// -- Caches --

DOMCache<WebCore::Node*, __unsafe_unretained WKDOMNode *>& NODELETE WKDOMNodeCache();
DOMCache<WebCore::Range*, __unsafe_unretained WKDOMRange *>& NODELETE WKDOMRangeCache();

// -- Node and classes derived from Node. --

WebCore::Node* CLANG_POINTER_CONVERSION toWebCoreNode(WKDOMNode *);
WKDOMNode *CLANG_POINTER_CONVERSION toWKDOMNode(WebCore::Node*);

WebCore::Element* CLANG_POINTER_CONVERSION toWebCoreElement(WKDOMElement *);
WKDOMElement *CLANG_POINTER_CONVERSION toWKDOMElement(WebCore::Element*);

WebCore::Document* CLANG_POINTER_CONVERSION toWebCoreDocument(WKDOMDocument *);
WKDOMDocument *CLANG_POINTER_CONVERSION toWKDOMDocument(WebCore::Document*);

WebCore::Text* CLANG_POINTER_CONVERSION toWebCoreText(WKDOMText *);
WKDOMText *CLANG_POINTER_CONVERSION toWKDOMText(WebCore::Text*);

// -- Range. --

WebCore::Range* toWebCoreRange(WKDOMRange *);
WKDOMRange *toWKDOMRange(WebCore::Range*);

} // namespace WebKit
