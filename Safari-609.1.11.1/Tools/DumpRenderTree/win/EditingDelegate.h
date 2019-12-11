/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef EditingDelegate_h
#define EditingDelegate_h

#include <WebKitLegacy/WebKit.h>

class __declspec(uuid("265DCD4B-79C3-44a2-84BC-511C3EDABD6F")) EditingDelegate final : public IWebEditingDelegate2, public IWebNotificationObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    EditingDelegate();

    void setAcceptsEditing(bool b) { m_acceptsEditing = b; }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebEditingDelegate
    virtual HRESULT STDMETHODCALLTYPE shouldBeginEditingInDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldEndEditingInDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldInsertNode(_In_opt_ IWebView*, _In_opt_ IDOMNode*, _In_opt_ IDOMRange*, WebViewInsertAction);
    virtual HRESULT STDMETHODCALLTYPE shouldInsertText(_In_opt_ IWebView*, _In_ BSTR text, _In_opt_ IDOMRange*, WebViewInsertAction, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldDeleteDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldChangeSelectedDOMRange(_In_opt_ IWebView*, _In_opt_ IDOMRange*, _In_opt_ IDOMRange*,
        WebSelectionAffinity, BOOL stillSelecting, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldApplyStyle(_In_opt_ IWebView*, _In_opt_ IDOMCSSStyleDeclaration*, _In_opt_ IDOMRange*, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE shouldChangeTypingStyle(_In_opt_ IWebView*, _In_opt_ IDOMCSSStyleDeclaration* currentStyle,
        _In_opt_ IDOMCSSStyleDeclaration* proposedStyle, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE doPlatformCommand(_In_opt_ IWebView*, _In_ BSTR command, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE webViewDidBeginEditing(_In_opt_ IWebNotification*);
    virtual HRESULT STDMETHODCALLTYPE webViewDidChange(_In_opt_ IWebNotification*);
    virtual HRESULT STDMETHODCALLTYPE webViewDidEndEditing(_In_opt_ IWebNotification*);
    virtual HRESULT STDMETHODCALLTYPE webViewDidChangeTypingStyle(_In_opt_ IWebNotification*);
    virtual HRESULT STDMETHODCALLTYPE webViewDidChangeSelection(_In_opt_ IWebNotification*);    
    virtual HRESULT STDMETHODCALLTYPE undoManagerForWebView(_In_opt_ IWebView*, _COM_Outptr_opt_ IWebUndoManager **);
    virtual HRESULT STDMETHODCALLTYPE ignoreWordInSpellDocument(_In_opt_ IWebView*, _In_ BSTR word) { return E_NOTIMPL; }        
    virtual HRESULT STDMETHODCALLTYPE learnWord(_In_ BSTR word) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE checkSpellingOfString(_In_opt_ IWebView*, _In_ LPCTSTR text, int length, _Out_ int* misspellingLocation, _Out_ int* misspellingLength);
    virtual HRESULT STDMETHODCALLTYPE checkGrammarOfString(_In_opt_ IWebView*, _In_ LPCTSTR text, int length, _COM_Outptr_opt_ IEnumWebGrammarDetails**,
        _Out_ int* badGrammarLocation, _Out_ int* badGrammarLength);       
    virtual HRESULT STDMETHODCALLTYPE updateSpellingUIWithGrammarString(_In_ BSTR string, int location, int length, _In_ BSTR userDescription,
        _In_opt_ BSTR* guesses, int guessesCount)
    {
        return E_NOTIMPL;
    }
        
    virtual HRESULT STDMETHODCALLTYPE updateSpellingUIWithMisspelledWord(_In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE showSpellingUI(BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE spellingUIIsShowing(_Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE guessesForWord(_In_ BSTR word, _COM_Outptr_opt_ IEnumSpellingGuesses** guesses);
    virtual HRESULT STDMETHODCALLTYPE closeSpellDocument(_In_opt_ IWebView*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE sharedSpellCheckerExists(_Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE preflightChosenSpellServer() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE updateGrammar() { return E_NOTIMPL; }

    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify(_In_opt_ IWebNotification*);

    // IWebEditingDelegate2
    virtual HRESULT STDMETHODCALLTYPE shouldInsertNode(_In_opt_ IWebView*, _In_opt_ IDOMNode*, _In_opt_ IDOMRange*, WebViewInsertAction, _Out_ BOOL* result);

private:
    ULONG m_refCount { 1 };
    bool m_acceptsEditing { true };
};

#endif // !defined(EditingDelegate_h)
