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

#import "WebDocument.h"
#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebViewInternal.h"
#import "WebEditingDelegatePrivate.h"
#import <wtf/PassRefPtr.h>

using namespace WebCore;

PassRefPtr<WebEditorClient> WebEditorClient::create()
{
    return new WebEditorClient;
}

WebEditorClient::WebEditorClient()
    : m_webFrame(nil) 
{
}

void WebEditorClient::ref()
{
    Shared<WebEditorClient>::ref();
}

void WebEditorClient::deref()
{
    Shared<WebEditorClient>::deref();
}

void WebEditorClient::setWebFrame(WebFrame *webFrame)
{
    ASSERT(!m_webFrame); // Should only be called during initialization
    ASSERT(webFrame);
    m_webFrame = webFrame;
}

bool WebEditorClient::isContinuousSpellCheckingEnabled()
{
    return [[m_webFrame webView] isContinuousSpellCheckingEnabled];
}

bool WebEditorClient::isGrammarCheckingEnabled()
{
#ifdef BUILDING_ON_TIGER
    return false;
#else
    return [[m_webFrame webView] isGrammarCheckingEnabled];
#endif
}

int WebEditorClient::spellCheckerDocumentTag()
{
    return [[m_webFrame webView] spellCheckerDocumentTag];
}

bool WebEditorClient::shouldDeleteRange(Range* range)
{
    return [[[m_webFrame webView] _editingDelegateForwarder] webView:[m_webFrame webView]
        shouldDeleteDOMRange:kit(range)];
}

bool WebEditorClient::shouldShowDeleteInterface(HTMLElement* element)
{
    return [[[m_webFrame webView] _editingDelegateForwarder] webView:[m_webFrame webView]
        shouldShowDeleteInterfaceForElement:kit(element)];
}

bool WebEditorClient::shouldApplyStyle(CSSStyleDeclaration* style, Range* range)
{
    return [[[m_webFrame webView] _editingDelegateForwarder] webView:[m_webFrame webView]
        shouldApplyStyle:kit(style) toElementsInDOMRange:kit(range)];
}

bool WebEditorClient::shouldBeginEditing(Range* range)
{
    return [[[m_webFrame webView] _editingDelegateForwarder] webView:[m_webFrame webView]
        shouldBeginEditingInDOMRange:kit(range)];

    return false;
}

bool WebEditorClient::shouldEndEditing(Range* range)
{
    return [[[m_webFrame webView] _editingDelegateForwarder] webView:[m_webFrame webView]
                             shouldEndEditingInDOMRange:kit(range)];
}

void WebEditorClient::didBeginEditing()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidBeginEditingNotification object:[m_webFrame webView]];
}

void WebEditorClient::respondToChangedContents()
{
    NSView <WebDocumentView> *view = [[m_webFrame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)view _updateFontPanel];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidChangeNotification object:[m_webFrame webView]];    
}

void WebEditorClient::didEndEditing()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidEndEditingNotification object:[m_webFrame webView]];
}

/*
bool WebEditorClient::shouldInsertNode(Node *node, Range* replacingRange, WebViewInsertAction givenAction) { return false; }
bool WebEditorClient::shouldInsertText(NSString *text, Range *replacingRange, WebViewInsertActiongivenAction) { return false; }
bool WebEditorClient::shouldChangeSelectedRange(Range *currentRange, Range *toProposedRange, NSSelectionAffinity selectionAffinity, bool stillSelecting) { return false; }
bool WebEditorClient::shouldChangeTypingStyle(CSSStyleDeclaration *currentStyle, CSSStyleDeclaration *toProposedStyle) { return false; }
bool WebEditorClient::doCommandBySelector(SEL selector) { return false; }

void WebEditorClient::webViewDidBeginEditing:(NSNotification *)notification { }
void WebEditorClient::webViewDidChange:(NSNotification *)notification { }
void WebEditorClient::webViewDidEndEditing:(NSNotification *)notification { }
void WebEditorClient::webViewDidChangeTypingStyle:(NSNotification *)notification { }
void WebEditorClient::webViewDidChangeSelection:(NSNotification *)notification { }
NSUndoManager* WebEditorClient::undoManagerForWebView:(WebView *)webView { return NULL; }
*/
