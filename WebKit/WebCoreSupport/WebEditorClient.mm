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

#import "WebEditorClient.h"

#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/DOMRangeInternal.h>
#import "WebEditingDelegate.h"

using namespace WebCore;

WebEditorClient::WebEditorClient()
    : m_webView(NULL) 
{ }

WebEditorClient::WebEditorClient(WebView* webView)
    : m_webView(webView) 
{
    [m_webView retain];
}

WebEditorClient::~WebEditorClient()
{
    [m_webView release];
}

bool WebEditorClient::shouldDeleteRange(Range *range)
{
    return [[m_webView _editingDelegateForwarder] webView:m_webView shouldDeleteDOMRange:[DOMRange _rangeWith:range]];
}

/*
bool WebEditorClient::shouldBeginEditingInRange(Range *range) { return false; }
bool WebEditorClient::shouldEndEditingInRange(Range *range) { return false; }
bool WebEditorClient::shouldInsertNode(Node *node, Range* replacingRange, WebViewInsertAction givenAction) { return false; }
bool WebEditorClient::shouldInsertText(NSString *text, Range *replacingRange, WebViewInsertActiongivenAction) { return false; }
bool WebEditorClient::shouldChangeSelectedRange(Range *currentRange, Range *toProposedRange, NSSelectionAffinity selectionAffinity, bool stillSelecting) { return false; }
bool WebEditorClient::shouldApplyStyle(CSSStyleDeclaration *style, Range *toElementsInDOMRange) { return false; }
bool WebEditorClient::shouldChangeTypingStyle(CSSStyleDeclaration *currentStyle, CSSStyleDeclaration *toProposedStyle) { return false; }
bool WebEditorClient::doCommandBySelector(SEL selector) { return false; }

void WebEditorClient::webViewDidBeginEditing:(NSNotification *)notification { }
void WebEditorClient::webViewDidChange:(NSNotification *)notification { }
void WebEditorClient::webViewDidEndEditing:(NSNotification *)notification { }
void WebEditorClient::webViewDidChangeTypingStyle:(NSNotification *)notification { }
void WebEditorClient::webViewDidChangeSelection:(NSNotification *)notification { }
NSUndoManager* WebEditorClient::undoManagerForWebView:(WebView *)webView { return NULL; }
*/
