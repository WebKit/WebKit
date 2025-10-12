/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKSerializedNode.h>

namespace TestWebKitAPI {

TEST(SerializedNode, Basic)
{
    RetainPtr webView = adoptNS([WKWebView new]);
    [webView loadHTMLString:@"<div id='testid'><div>test</div></div><template id='outerTemplate'><template id='innerTemplate'><span>Contents</span></template></template>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowNodeSerialization = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    auto verifyNodeSerialization = [world, webView] (const char* constructor, const char* accessor, const char* expected, const char* className, const char* init = "deep:true") {
        RetainPtr serializedNode = [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.webkit.serializeNode(%s, { %s })", constructor, init] inFrame:nil inContentWorld:world.get()];
        EXPECT_TRUE([serializedNode isKindOfClass:_WKSerializedNode.class]);
        RetainPtr other = adoptNS([TestWKWebView new]);

        id instanceof = [other objectByCallingAsyncFunction:@"return Object.getPrototypeOf(n).toString()" withArguments:@{ @"n" : serializedNode.get() }];
        NSString *expectedClass = [NSString stringWithFormat:@"[object %s]", className];
        EXPECT_WK_STREQ(instanceof, expectedClass);

        id result = [other objectByCallingAsyncFunction:[NSString stringWithFormat:@"return %s", accessor] withArguments:@{ @"n" : serializedNode.get() }];
        EXPECT_WK_STREQ(result, expected);
    };

    auto textAccessor = "n.wholeText";
    verifyNodeSerialization("document.createTextNode('text content')", textAccessor, "text content", "Text");

    auto attrAccessor = "n.namespaceURI + ',' + n.prefix + ',' + n.localName + ',' + n.name + ',' + n.value";
    verifyNodeSerialization("document.createAttributeNS('a', 'b')", attrAccessor, "a,null,b,b,", "Attr");
    verifyNodeSerialization("document.createAttribute('c')", attrAccessor, "null,null,c,c,", "Attr");

    verifyNodeSerialization("new Document().createCDATASection('test')", textAccessor, "test", "CDATASection");

    verifyNodeSerialization("document.implementation.createDocumentType('a', 'b', 'c')", "n.name + ',' + n.publicId + ',' + n.systemId", "a,b,c", "DocumentType");

    verifyNodeSerialization("document.createProcessingInstruction('a', 'b')", "n.target + ',' + n.data", "a,b", "ProcessingInstruction");

    auto documentAccessor = "n.URL + ',' + n.documentURI + ',' + new XMLSerializer().serializeToString(n)";
    verifyNodeSerialization("document.implementation.createDocument('http://www.w3.org/2000/svg', 'svg', null)", documentAccessor, "about:blank,about:blank,<svg xmlns=\"http://www.w3.org/2000/svg\"/>", "XMLDocument");
    verifyNodeSerialization("document.implementation.createHTMLDocument('test title')", documentAccessor, "about:blank,about:blank,<!DOCTYPE html><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>test title</title></head><body></body></html>", "HTMLDocument");
    verifyNodeSerialization("document", documentAccessor, "https://webkit.org/,https://webkit.org/,<html xmlns=\"http://www.w3.org/1999/xhtml\"><head></head><body><div id=\"testid\"><div>test</div></div><template id=\"outerTemplate\"><template id=\"innerTemplate\"><span>Contents</span></template></template></body></html>", "HTMLDocument");
    verifyNodeSerialization("document.getElementById('testid')", documentAccessor, "undefined,undefined,<div xmlns=\"http://www.w3.org/1999/xhtml\" id=\"testid\"></div>", "HTMLDivElement", "");

    verifyNodeSerialization("document.createDocumentFragment()", "n.nodeName", "#document-fragment", "DocumentFragment");

    NSString *shadowRootJS = @""
    "<div id='host'>"
    "    <template shadowrootmode='open' shadowrootclonable='true' shadowrootcustomelements>"
    "        <a-b>"
    "            <template shadowrootmode='open' shadowrootclonable='true' shadowrootcustomelements>"
    "                <c-d/><c-d>"
    "            </template>"
    "        </a-b>"
    "        <ef></ef>"
    "    </template>"
    "</div>";
    [webView loadHTMLString:shadowRootJS baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView _test_waitForDidFinishNavigation];
    verifyNodeSerialization("document.getElementById('host')", "n.shadowRoot.mode + ' ' + n.shadowRoot.delegatesFocus + ' ' + n.shadowRoot.serializable + ' ' + n.shadowRoot.host.tagName + ' ' + n.shadowRoot.querySelector('c-d')", "open false false DIV null", "HTMLDivElement");

    __block bool done { false };
    [webView evaluateJavaScript:@"window.webkit.serializeNode(document.getElementById('host').shadowRoot)" inFrame:nil inContentWorld:world.get() completionHandler:^(id, NSError *error) {
        EXPECT_WK_STREQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, WKErrorJavaScriptExceptionOccurred);
        done = true;
    }];
    Util::run(&done);
}

}
