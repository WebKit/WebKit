/*
 * Copyright (C) 2007, 2014-2015 Apple Inc.  All rights reserved.
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
#include "EditingDelegate.h"

#include <comutil.h>
#include "DumpRenderTree.h"
#include "TestRunner.h"
#include <WebCore/COMPtr.h>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/StringExtras.h>
#include <string>
#include <tchar.h>

using std::wstring;

EditingDelegate::EditingDelegate()
{
}

// IUnknown
HRESULT EditingDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebEditingDelegate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebEditingDelegate))
        *ppvObject = static_cast<IWebEditingDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebEditingDelegate2))
        *ppvObject = static_cast<IWebEditingDelegate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationObserver))
        *ppvObject = static_cast<IWebNotificationObserver*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG EditingDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG EditingDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

static std::string dumpPath(IDOMNode* node)
{
    ASSERT(node);

    std::string result;

    _bstr_t name;
    if (FAILED(node->nodeName(&name.GetBSTR())))
        return result;
    result.assign(static_cast<const char*>(name), name.length());

    COMPtr<IDOMNode> parent;
    if (SUCCEEDED(node->parentNode(&parent)))
        result += " > " + dumpPath(parent.get());

    return result;
}

static std::string dump(IDOMRange* range)
{
    if (!range)
        return std::string();

    int startOffset;
    if (FAILED(range->startOffset(&startOffset)))
        return std::string();

    int endOffset;
    if (FAILED(range->endOffset(&endOffset)))
        return std::string();

    COMPtr<IDOMNode> startContainer;
    if (FAILED(range->startContainer(&startContainer)))
        return std::string();

    COMPtr<IDOMNode> endContainer;
    if (FAILED(range->endContainer(&endContainer)))
        return std::string();

    char buffer[1024];
    snprintf(buffer, ARRAYSIZE(buffer), "range from %d of %s to %d of %s", startOffset, dumpPath(startContainer.get()).c_str(), endOffset, dumpPath(endContainer.get()).c_str());
    return buffer;
}

HRESULT EditingDelegate::shouldBeginEditingInDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange* range, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n", dump(range).c_str());

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldEndEditingInDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange* range, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n", dump(range).c_str());

    *result = m_acceptsEditing;
    return S_OK;
}

// IWebEditingDelegate
HRESULT EditingDelegate::shouldInsertNode(_In_opt_ IWebView* webView, _In_opt_ IDOMNode* node, _In_opt_ IDOMRange* range, WebViewInsertAction action)
{
    BOOL ignore;
    return shouldInsertNode(webView, node, range, action, &ignore);
}

// IWebEditingDelegate2
HRESULT EditingDelegate::shouldInsertNode(_In_opt_ IWebView* /*webView*/, _In_opt_ IDOMNode* node, _In_opt_ IDOMRange* range, WebViewInsertAction action, _Out_ BOOL* result)
{
    static const char* insertActionString[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (!result)
        return E_POINTER;

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldInsertNode:%s replacingDOMRange:%s givenAction:%s\n", dumpPath(node).c_str(), dump(range).c_str(), insertActionString[action]);

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldInsertText(_In_opt_ IWebView* /*webView*/, _In_ BSTR text, _In_opt_ IDOMRange* range, WebViewInsertAction action, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    static const char* insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t textBstr(text);
        fprintf(testResult, "EDITING DELEGATE: shouldInsertText:%s replacingDOMRange:%s givenAction:%s\n", static_cast<const char*>(textBstr), dump(range).c_str(), insertactionstring[action]);
    }

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldDeleteDOMRange(_In_opt_ IWebView* /*webView*/, _In_opt_ IDOMRange* range, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldDeleteDOMRange:%s\n", dump(range).c_str());

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldChangeSelectedDOMRange(_In_opt_ IWebView* /*webView*/, _In_opt_ IDOMRange* currentRange, _In_opt_ IDOMRange* proposedRange,
    WebSelectionAffinity selectionAffinity, BOOL stillSelecting, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    static const char* affinityString[] = {
        "NSSelectionAffinityUpstream",
        "NSSelectionAffinityDownstream"
    };
    static const char* boolstring[] = {
        "FALSE",
        "TRUE"
    };

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldChangeSelectedDOMRange:%s toDOMRange:%s affinity:%s stillSelecting:%s\n", dump(currentRange).c_str(), dump(proposedRange).c_str(), affinityString[selectionAffinity], boolstring[stillSelecting]);

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldApplyStyle(_In_opt_ IWebView* /*webView*/, _In_opt_ IDOMCSSStyleDeclaration* style, _In_opt_ IDOMRange* range, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n", "'style description'"/*[[style description] UTF8String]*/, dump(range).c_str());

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::shouldChangeTypingStyle(_In_opt_ IWebView* /*webView*/, _In_opt_ IDOMCSSStyleDeclaration* currentStyle,
    _In_opt_ IDOMCSSStyleDeclaration* proposedStyle, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done)
        fprintf(testResult, "EDITING DELEGATE: shouldChangeTypingStyle:%s toStyle:%s\n", "'currentStyle description'", "'proposedStyle description'");

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::doPlatformCommand(_In_opt_ IWebView* /*webView*/, _In_ BSTR command, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t commandBSTR(command);
        fprintf(testResult, "EDITING DELEGATE: doPlatformCommand:%s\n", static_cast<const char*>(commandBSTR));
    }

    *result = m_acceptsEditing;
    return S_OK;
}

HRESULT EditingDelegate::webViewDidBeginEditing(_In_opt_ IWebNotification* notification)
{
    if (!notification)
        return S_OK;

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t name;
        notification->name(&name.GetBSTR());
        fprintf(testResult, "EDITING DELEGATE: webViewDidBeginEditing:%s\n", static_cast<const char*>(name));
    }
    return S_OK;
}

HRESULT EditingDelegate::webViewDidChange(_In_opt_ IWebNotification* notification)
{
    if (!notification)
        return S_OK;

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t name;
        notification->name(&name.GetBSTR());
        fprintf(testResult, "EDITING DELEGATE: webViewDidChange:%s\n", static_cast<const char*>(name));
    }
    return S_OK;
}

HRESULT EditingDelegate::webViewDidEndEditing(_In_opt_ IWebNotification* notification)
{
    if (!notification)
        return S_OK;

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t name;
        notification->name(&name.GetBSTR());
        fprintf(testResult, "EDITING DELEGATE: webViewDidEndEditing:%s\n", static_cast<const char*>(name));
    }
    return S_OK;
}

HRESULT EditingDelegate::webViewDidChangeTypingStyle(_In_opt_ IWebNotification* notification)
{
    if (!notification)
        return S_OK;

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t name;
        notification->name(&name.GetBSTR());
        fprintf(testResult, "EDITING DELEGATE: webViewDidChangeTypingStyle:%s\n", static_cast<const char*>(name));
    }
    return S_OK;
}

HRESULT EditingDelegate::webViewDidChangeSelection(_In_opt_ IWebNotification* notification)
{
    if (!notification)
        return S_OK;

    if (::gTestRunner->dumpEditingCallbacks() && !done) {
        _bstr_t name;
        notification->name(&name.GetBSTR());
        fprintf(testResult, "EDITING DELEGATE: webViewDidChangeSelection:%s\n", static_cast<const char*>(name));
    }
    return S_OK;
}

HRESULT EditingDelegate::undoManagerForWebView(_In_opt_ IWebView*, _COM_Outptr_opt_ IWebUndoManager** undoManager)
{
    if (!undoManager)
        return E_POINTER;
    *undoManager = nullptr;
    return E_NOTIMPL;
}

static int indexOfFirstWordCharacter(const TCHAR* text)
{
    const TCHAR* cursor = text;
    while (*cursor && !iswalpha(*cursor))
        ++cursor;
    return *cursor ? (cursor - text) : -1;
};

static int wordLength(const TCHAR* text)
{
    const TCHAR* cursor = text;
    while (*cursor && iswalpha(*cursor))
        ++cursor;
    return cursor - text;
};

HRESULT EditingDelegate::checkSpellingOfString(_In_opt_ IWebView* view, _In_ LPCTSTR text,
    int length, _Out_ int* misspellingLocation, _Out_ int* misspellingLength)
{
    static const TCHAR* misspelledWords[] = {
        // These words are known misspelled words in webkit tests.
        // If there are other misspelled words in webkit tests, please add them in
        // this array.
        TEXT("foo"),
        TEXT("Foo"),
        TEXT("baz"),
        TEXT("fo"),
        TEXT("LibertyF"),
        TEXT("chello"),
        TEXT("xxxtestxxx"),
        TEXT("XXxxx"),
        TEXT("Textx"),
        TEXT("blockquoted"),
        TEXT("asd"),
        TEXT("Lorem"),
        TEXT("Nunc"),
        TEXT("Curabitur"),
        TEXT("eu"),
        TEXT("adlj"),
        TEXT("adaasj"),
        TEXT("sdklj"),
        TEXT("jlkds"),
        TEXT("jsaada"),
        TEXT("jlda"),
        TEXT("zz"),
        TEXT("contentEditable"),
        0,
    };

    if (!misspellingLocation || !misspellingLength)
        return E_POINTER;

    *misspellingLocation = 0;
    *misspellingLength = 0;

    wstring textString(text, length);
    int wordStart = indexOfFirstWordCharacter(textString.c_str());
    if (-1 == wordStart)
        return S_OK;

    wstring word = textString.substr(wordStart, wordLength(textString.c_str() + wordStart));
    for (size_t i = 0; misspelledWords[i]; ++i) {
        if (word == misspelledWords[i]) {
            *misspellingLocation = wordStart;
            *misspellingLength = word.size();
            break;
        }
    }

    return S_OK;
}

HRESULT EditingDelegate::checkGrammarOfString(_In_opt_ IWebView*, _In_ LPCTSTR text, int length, _COM_Outptr_opt_ IEnumWebGrammarDetails** details,
    _Out_ int* badGrammarLocation, _Out_ int* badGrammarLength)
{
    if (!details)
        return E_POINTER;
    *details = nullptr;
    return E_NOTIMPL;
}

HRESULT EditingDelegate::guessesForWord(_In_ BSTR word, _COM_Outptr_opt_ IEnumSpellingGuesses** guesses)
{
    if (!guesses)
        return E_POINTER;
    *guesses = nullptr;
    return E_NOTIMPL;
}


HRESULT EditingDelegate::onNotify(_In_opt_ IWebNotification* notification)
{
    _bstr_t notificationName;
    HRESULT hr = notification->name(&notificationName.GetBSTR());
    if (FAILED(hr))
        return hr;

    static _bstr_t webViewDidBeginEditingNotificationName(WebViewDidBeginEditingNotification);
    static _bstr_t webViewDidChangeSelectionNotificationName(WebViewDidChangeSelectionNotification);
    static _bstr_t webViewDidEndEditingNotificationName(WebViewDidEndEditingNotification);
    static _bstr_t webViewDidChangeTypingStyleNotificationName(WebViewDidChangeTypingStyleNotification);
    static _bstr_t webViewDidChangeNotificationName(WebViewDidChangeNotification);

    if (!wcscmp(notificationName, webViewDidBeginEditingNotificationName))
        return webViewDidBeginEditing(notification);

    if (!wcscmp(notificationName, webViewDidChangeSelectionNotificationName))
        return webViewDidChangeSelection(notification);

    if (!wcscmp(notificationName, webViewDidEndEditingNotificationName))
        return webViewDidEndEditing(notification);

    if (!wcscmp(notificationName, webViewDidChangeTypingStyleNotificationName))
        return webViewDidChangeTypingStyle(notification);

    if (!wcscmp(notificationName, webViewDidChangeNotificationName))
        return webViewDidChange(notification);

    return S_OK;
}
