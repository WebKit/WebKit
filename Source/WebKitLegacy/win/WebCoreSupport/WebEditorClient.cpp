/*
 * Copyright (C) 2006-2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "WebKitDLL.h"
#include "WebEditorClient.h"

#include "DOMCoreClasses.h"
#include "WebKit.h"
#include "WebNotification.h"
#include "WebNotificationCenter.h"
#include "WebView.h"
#include <comutil.h>
#include <WebCore/BString.h>
#include <WebCore/Document.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Range.h>
#include <WebCore/Settings.h>
#include <WebCore/UndoStep.h>
#include <WebCore/UserTypingGestureIndicator.h>
#include <WebCore/VisibleSelection.h>
#include <wtf/text/StringView.h>
#include <wtf/text/win/WCharStringExtras.h>

using namespace WebCore;
using namespace HTMLNames;

// {09A11D2B-FAFB-4ca0-A6F7-791EE8932C88}
static const GUID IID_IWebUndoCommand = 
{ 0x9a11d2b, 0xfafb, 0x4ca0, { 0xa6, 0xf7, 0x79, 0x1e, 0xe8, 0x93, 0x2c, 0x88 } };

class IWebUndoCommand : public IUnknown {
public:
    virtual void execute() = 0;
};

// WebEditorUndoTarget -------------------------------------------------------------

class WebEditorUndoTarget final : public IWebUndoTarget
{
public:
    WebEditorUndoTarget();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebUndoTarget
    virtual HRESULT STDMETHODCALLTYPE invoke( 
        /* [in] */ BSTR actionName,
        /* [in] */ IUnknown *obj);

private:
    ULONG m_refCount;
};

WebEditorUndoTarget::WebEditorUndoTarget()
: m_refCount(1)
{
}

HRESULT WebEditorUndoTarget::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUndoTarget*>(this);
    else if (IsEqualGUID(riid, IID_IWebUndoTarget))
        *ppvObject = static_cast<IWebUndoTarget*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebEditorUndoTarget::AddRef()
{
    return ++m_refCount;
}

ULONG WebEditorUndoTarget::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT WebEditorUndoTarget::invoke(/* [in] */ BSTR /*actionName*/, /* [in] */ IUnknown *obj)
{
    IWebUndoCommand* undoCommand = 0;
    if (SUCCEEDED(obj->QueryInterface(IID_IWebUndoCommand, (void**)&undoCommand))) {
        undoCommand->execute();
        undoCommand->Release();
    }
    return S_OK;
}

// WebEditorClient ------------------------------------------------------------------

WebEditorClient::WebEditorClient(WebView* webView)
    : m_webView(webView)
    , m_undoTarget(0)
{
    m_undoTarget = new WebEditorUndoTarget();
}

WebEditorClient::~WebEditorClient()
{
    if (m_undoTarget)
        m_undoTarget->Release();
}

bool WebEditorClient::isContinuousSpellCheckingEnabled()
{
    BOOL enabled;
    if (FAILED(m_webView->isContinuousSpellCheckingEnabled(&enabled)))
        return false;
    return !!enabled;
}

void WebEditorClient::toggleContinuousSpellChecking()
{
    m_webView->toggleContinuousSpellChecking(0);
}

bool WebEditorClient::isGrammarCheckingEnabled()
{
    BOOL enabled;
    if (FAILED(m_webView->isGrammarCheckingEnabled(&enabled)))
        return false;
    return !!enabled;
}

void WebEditorClient::toggleGrammarChecking()
{
    m_webView->toggleGrammarChecking(0);
}

static void initViewSpecificSpelling(IWebViewEditing* viewEditing)
{
    // we just use this as a flag to indicate that we've spell checked the document
    // and need to close the spell checker out when the view closes.
    int tag;
    viewEditing->spellCheckerDocumentTag(&tag);
}

int WebEditorClient::spellCheckerDocumentTag()
{
    // we don't use the concept of spelling tags
    notImplemented();
    ASSERT_NOT_REACHED();
    return 0;
}

bool WebEditorClient::shouldBeginEditing(const WebCore::SimpleRange& range)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return true;

    COMPtr<IDOMRange> currentRange(AdoptCOM, DOMRange::createInstance(range));

    BOOL shouldBegin = FALSE;
    if (FAILED(ed->shouldBeginEditingInDOMRange(m_webView, currentRange.get(), &shouldBegin)))
        return true;

    return shouldBegin;
}

bool WebEditorClient::shouldEndEditing(const WebCore::SimpleRange& range)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return true;

    COMPtr<IDOMRange> currentRange(AdoptCOM, DOMRange::createInstance(range));

    BOOL shouldEnd = FALSE;
    if (FAILED(ed->shouldEndEditingInDOMRange(m_webView, currentRange.get(), &shouldEnd)))
        return true;

    return shouldEnd;
}

void WebEditorClient::didBeginEditing()
{
    static _bstr_t webViewDidBeginEditingNotificationName(WebViewDidBeginEditingNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(webViewDidBeginEditingNotificationName.GetBSTR(), static_cast<IWebView*>(m_webView), nullptr);
}

void WebEditorClient::respondToChangedContents()
{
    static _bstr_t webViewDidChangeNotificationName(WebViewDidChangeNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(webViewDidChangeNotificationName.GetBSTR(), static_cast<IWebView*>(m_webView), 0);
}

void WebEditorClient::respondToChangedSelection(Frame*)
{
    m_webView->selectionChanged();

    static _bstr_t webViewDidChangeSelectionNotificationName(WebViewDidChangeSelectionNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(webViewDidChangeSelectionNotificationName.GetBSTR(), static_cast<IWebView*>(m_webView), 0);
}

void WebEditorClient::discardedComposition(Frame*)
{
}

void WebEditorClient::canceledComposition()
{
}

void WebEditorClient::didEndEditing()
{
    static _bstr_t webViewDidEndEditingNotificationName(WebViewDidEndEditingNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(webViewDidEndEditingNotificationName.GetBSTR(), static_cast<IWebView*>(m_webView), nullptr);
}

void WebEditorClient::didWriteSelectionToPasteboard()
{
}

void WebEditorClient::willWriteSelectionToPasteboard(const Optional<WebCore::SimpleRange>&)
{
}

void WebEditorClient::getClientPasteboardData(const Optional<WebCore::SimpleRange>&, Vector<String>&, Vector<RefPtr<WebCore::SharedBuffer> >&)
{
    notImplemented();
}

bool WebEditorClient::shouldDeleteRange(const Optional<WebCore::SimpleRange>& range)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return true;

    COMPtr<IDOMRange> currentRange(AdoptCOM, DOMRange::createInstance(range));

    BOOL shouldDelete = FALSE;
    if (FAILED(ed->shouldDeleteDOMRange(m_webView, currentRange.get(), &shouldDelete)))
        return true;

    return shouldDelete;
}

static WebViewInsertAction kit(EditorInsertAction action)
{
    switch (action) {
    case EditorInsertAction::Typed:
        return WebViewInsertActionTyped;
    case EditorInsertAction::Pasted:
        return WebViewInsertActionPasted;
    case EditorInsertAction::Dropped:
        return WebViewInsertActionDropped;
    }

    ASSERT_NOT_REACHED();
    return WebViewInsertActionTyped;
}

bool WebEditorClient::shouldInsertNode(Node& node, const Optional<WebCore::SimpleRange>& insertingRange, EditorInsertAction givenAction)
{ 
    COMPtr<IWebEditingDelegate> editingDelegate;
    if (FAILED(m_webView->editingDelegate(&editingDelegate)) || !editingDelegate.get())
        return true;

    COMPtr<IDOMRange> insertingDOMRange(AdoptCOM, DOMRange::createInstance(insertingRange));
    if (!insertingDOMRange)
        return true;

    COMPtr<IDOMNode> insertDOMNode(AdoptCOM, DOMNode::createInstance(&node));
    if (!insertDOMNode)
        return true;

    BOOL shouldInsert = FALSE;
    COMPtr<IWebEditingDelegate2> editingDelegate2(Query, editingDelegate);
    if (editingDelegate2) {
        if (FAILED(editingDelegate2->shouldInsertNode(m_webView, insertDOMNode.get(), insertingDOMRange.get(), kit(givenAction), &shouldInsert)))
            return true;
    }

    return shouldInsert;
}

bool WebEditorClient::shouldInsertText(const String& str, const Optional<WebCore::SimpleRange>& insertingRange, EditorInsertAction givenAction)
{
    COMPtr<IWebEditingDelegate> editingDelegate;
    if (FAILED(m_webView->editingDelegate(&editingDelegate)) || !editingDelegate.get())
        return true;

    COMPtr<IDOMRange> insertingDOMRange(AdoptCOM, DOMRange::createInstance(insertingRange));
    if (!insertingDOMRange)
        return true;

    BString text(str);
    BOOL shouldInsert = FALSE;
    if (FAILED(editingDelegate->shouldInsertText(m_webView, text, insertingDOMRange.get(), kit(givenAction), &shouldInsert)))
        return true;

    return shouldInsert;
}

bool WebEditorClient::shouldChangeSelectedRange(const Optional<WebCore::SimpleRange>& currentRange, const Optional<WebCore::SimpleRange>& proposedRange, WebCore::EAffinity selectionAffinity, bool flag)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return true;

    COMPtr<IDOMRange> currentIDOMRange(AdoptCOM, DOMRange::createInstance(currentRange));
    COMPtr<IDOMRange> proposedIDOMRange(AdoptCOM, DOMRange::createInstance(proposedRange));

    BOOL shouldChange = FALSE;
    if (FAILED(ed->shouldChangeSelectedDOMRange(m_webView, currentIDOMRange.get(), proposedIDOMRange.get(), static_cast<WebSelectionAffinity>(selectionAffinity), flag, &shouldChange)))
        return true;

    return shouldChange;
}

bool WebEditorClient::shouldApplyStyle(const StyleProperties&, const Optional<SimpleRange>&)
{
    return true;
}

void WebEditorClient::didApplyStyle()
{
}

bool WebEditorClient::shouldMoveRangeAfterDelete(const WebCore::SimpleRange&, const WebCore::SimpleRange&)
{
    return true;
}

bool WebEditorClient::smartInsertDeleteEnabled(void)
{
    Page* page = m_webView->page();
    if (!page)
        return false;
    return page->settings().smartInsertDeleteEnabled();
}

bool WebEditorClient::isSelectTrailingWhitespaceEnabled(void) const
{
    Page* page = m_webView->page();
    if (!page)
        return false;
    return page->settings().selectTrailingWhitespaceEnabled();
}

void WebEditorClient::textFieldDidBeginEditing(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textFieldDidBeginEditing(domInputElement, kit(e->document().frame()));
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebEditorClient::textFieldDidEndEditing(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textFieldDidEndEditing(domInputElement, kit(e->document().frame()));
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebEditorClient::textDidChangeInTextField(Element* e)
{
    if (!UserTypingGestureIndicator::processingUserTypingGesture() || UserTypingGestureIndicator::focusedElementAtGestureStart() != e)
        return;

    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textDidChangeInTextField(domInputElement, kit(e->document().frame()));
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

bool WebEditorClient::doTextFieldCommandFromEvent(Element* e, KeyboardEvent* ke)
{
    BOOL result = FALSE;
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                String command = m_webView->interpretKeyEvent(ke);
                // We allow empty commands here because the app code actually depends on this being called for all key presses.
                // We may want to revisit this later because it doesn't really make sense to send an empty command.
                formDelegate->doPlatformCommand(domInputElement, BString(command), kit(e->document().frame()), &result);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
    return !!result;
}

void WebEditorClient::textWillBeDeletedInTextField(Element* e)
{
    // We're using the deleteBackward command for all deletion operations since the autofill code treats all deletions the same way.
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                BOOL result;
                formDelegate->doPlatformCommand(domInputElement, BString(L"DeleteBackward"), kit(e->document().frame()), &result);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebEditorClient::textDidChangeInTextArea(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(m_webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLTextAreaElement* domTextAreaElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLTextAreaElement, (void**)&domTextAreaElement))) {
                formDelegate->textDidChangeInTextArea(domTextAreaElement, kit(e->document().frame()));
                domTextAreaElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

class WebEditorUndoCommand final : public IWebUndoCommand
{
public:
    WebEditorUndoCommand(UndoStep&, bool isUndo);
    void execute();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

private:
    ULONG m_refCount;
    Ref<UndoStep> m_step;
    bool m_isUndo;
};

WebEditorUndoCommand::WebEditorUndoCommand(UndoStep& step, bool isUndo)
    : m_refCount(1)
    , m_step(step)
    , m_isUndo(isUndo)
{ 
}

void WebEditorUndoCommand::execute()
{
    if (m_isUndo)
        m_step->unapply();
    else
        m_step->reapply();
}

HRESULT WebEditorUndoCommand::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUndoCommand*>(this);
    else if (IsEqualGUID(riid, IID_IWebUndoCommand))
        *ppvObject = static_cast<IWebUndoCommand*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebEditorUndoCommand::AddRef()
{
    return ++m_refCount;
}

ULONG WebEditorUndoCommand::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

static String undoNameForEditAction(EditAction editAction)
{
    switch (editAction) {
    case EditAction::Unspecified:
    case EditAction::InsertReplacement:
        return String();
    case EditAction::SetColor: return WEB_UI_STRING_KEY("Set Color", "Set Color (Undo action name)", "Undo action name");
    case EditAction::SetBackgroundColor: return WEB_UI_STRING_KEY("Set Background Color", "Set Background Color (Undo action name)", "Undo action name");
    case EditAction::TurnOffKerning: return WEB_UI_STRING_KEY("Turn Off Kerning", "Turn Off Kerning (Undo action name)", "Undo action name");
    case EditAction::TightenKerning: return WEB_UI_STRING_KEY("Tighten Kerning", "Tighten Kerning (Undo action name)", "Undo action name");
    case EditAction::LoosenKerning: return WEB_UI_STRING_KEY("Loosen Kerning", "Loosen Kerning (Undo action name)", "Undo action name");
    case EditAction::UseStandardKerning: return WEB_UI_STRING_KEY("Use Standard Kerning", "Use Standard Kerning (Undo action name)", "Undo action name");
    case EditAction::TurnOffLigatures: return WEB_UI_STRING_KEY("Turn Off Ligatures", "Turn Off Ligatures (Undo action name)", "Undo action name");
    case EditAction::UseStandardLigatures: return WEB_UI_STRING_KEY("Use Standard Ligatures", "Use Standard Ligatures (Undo action name)", "Undo action name");
    case EditAction::UseAllLigatures: return WEB_UI_STRING_KEY("Use All Ligatures", "Use All Ligatures (Undo action name)", "Undo action name");
    case EditAction::RaiseBaseline: return WEB_UI_STRING_KEY("Raise Baseline", "Raise Baseline (Undo action name)", "Undo action name");
    case EditAction::LowerBaseline: return WEB_UI_STRING_KEY("Lower Baseline", "Lower Baseline (Undo action name)", "Undo action name");
    case EditAction::SetTraditionalCharacterShape: return WEB_UI_STRING_KEY("Set Traditional Character Shape", "Set Traditional Character Shape (Undo action name)", "Undo action name");
    case EditAction::SetFont: return WEB_UI_STRING_KEY("Set Font", "Set Font (Undo action name)", "Undo action name");
    case EditAction::ChangeAttributes: return WEB_UI_STRING_KEY("Change Attributes", "Change Attributes (Undo action name)", "Undo action name");
    case EditAction::AlignLeft: return WEB_UI_STRING_KEY("Align Left", "Align Left (Undo action name)", "Undo action name");
    case EditAction::AlignRight: return WEB_UI_STRING_KEY("Align Right", "Align Right (Undo action name)", "Undo action name");
    case EditAction::Center: return WEB_UI_STRING_KEY("Center", "Center (Undo action name)", "Undo action name");
    case EditAction::Justify: return WEB_UI_STRING_KEY("Justify", "Justify (Undo action name)", "Undo action name");
    case EditAction::SetInlineWritingDirection:
    case EditAction::SetBlockWritingDirection:
        return WEB_UI_STRING_KEY("Set Writing Direction", "Set Writing Direction (Undo action name)", "Undo action name");
    case EditAction::Subscript: return WEB_UI_STRING_KEY("Subscript", "Subscript (Undo action name)", "Undo action name");
    case EditAction::Superscript: return WEB_UI_STRING_KEY("Superscript", "Superscript (Undo action name)", "Undo action name");
    case EditAction::Bold: return WEB_UI_STRING_KEY("Bold", "Bold (Undo action name)", "Undo action name");
    case EditAction::Italics: return WEB_UI_STRING_KEY("Italics", "Italics (Undo action name)", "Undo action name");
    case EditAction::Underline: return WEB_UI_STRING_KEY("Underline", "Underline (Undo action name)", "Undo action name");
    case EditAction::Outline: return WEB_UI_STRING_KEY("Outline", "Outline (Undo action name)", "Undo action name");
    case EditAction::Unscript: return WEB_UI_STRING_KEY("Unscript", "Unscript (Undo action name)", "Undo action name");
    case EditAction::DeleteByDrag: return WEB_UI_STRING_KEY("Drag", "Drag (Undo action name)", "Undo action name");
    case EditAction::Cut: return WEB_UI_STRING_KEY("Cut", "Cut (Undo action name)", "Undo action name");
    case EditAction::Paste: return WEB_UI_STRING_KEY("Paste", "Paste (Undo action name)", "Undo action name");
    case EditAction::PasteFont: return WEB_UI_STRING_KEY("Paste Font", "Paste Font (Undo action name)", "Undo action name");
    case EditAction::PasteRuler: return WEB_UI_STRING_KEY("Paste Ruler", "Paste Ruler (Undo action name)", "Undo action name");
    case EditAction::TypingDeleteSelection:
    case EditAction::TypingDeleteBackward:
    case EditAction::TypingDeleteForward:
    case EditAction::TypingDeleteWordBackward:
    case EditAction::TypingDeleteWordForward:
    case EditAction::TypingDeleteLineBackward:
    case EditAction::TypingDeleteLineForward:
    case EditAction::TypingInsertText:
    case EditAction::TypingInsertLineBreak:
    case EditAction::TypingInsertParagraph:
        return WEB_UI_STRING_KEY("Typing", "Typing (Undo action name)", "Undo action name");
    case EditAction::CreateLink: return WEB_UI_STRING_KEY("Create Link", "Create Link (Undo action name)", "Undo action name");
    case EditAction::Unlink: return WEB_UI_STRING_KEY("Unlink", "Unlink (Undo action name)", "Undo action name");
    case EditAction::InsertUnorderedList:
    case EditAction::InsertOrderedList:
        return WEB_UI_STRING_KEY("Insert List", "Insert List (Undo action name)", "Undo action name");
    case EditAction::FormatBlock: return WEB_UI_STRING_KEY("Formatting", "Format Block (Undo action name)", "Undo action name");
    case EditAction::Indent: return WEB_UI_STRING_KEY("Indent", "Indent (Undo action name)", "Undo action name");
    case EditAction::Outdent: return WEB_UI_STRING_KEY("Outdent", "Outdent (Undo action name)", "Undo action name");
    default: return String();
    }
}

void WebEditorClient::registerUndoStep(UndoStep& step)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        String actionName = undoNameForEditAction(step.editingAction());
        WebEditorUndoCommand* undoCommand = new WebEditorUndoCommand(step, true);
        if (!undoCommand)
            return;
        uiDelegate->registerUndoWithTarget(m_undoTarget, 0, undoCommand);
        undoCommand->Release(); // the undo manager owns the reference
        if (!actionName.isEmpty())
            uiDelegate->setActionTitle(BString(actionName));
        uiDelegate->Release();
    }
}

void WebEditorClient::registerRedoStep(UndoStep& step)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        WebEditorUndoCommand* undoCommand = new WebEditorUndoCommand(step, false);
        if (!undoCommand)
            return;
        uiDelegate->registerUndoWithTarget(m_undoTarget, 0, undoCommand);
        undoCommand->Release(); // the undo manager owns the reference
        uiDelegate->Release();
    }
}

void WebEditorClient::clearUndoRedoOperations()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->removeAllActionsWithTarget(m_undoTarget);
        uiDelegate->Release();
    }
}

bool WebEditorClient::canCopyCut(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canPaste(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canUndo() const
{
    BOOL result = FALSE;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->canUndo(&result);
        uiDelegate->Release();
    }
    return !!result;
}

bool WebEditorClient::canRedo() const
{
    BOOL result = FALSE;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->canRedo(&result);
        uiDelegate->Release();
    }
    return !!result;
}

void WebEditorClient::undo()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->undo();
        uiDelegate->Release();
    }
}

void WebEditorClient::redo()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->redo();
        uiDelegate->Release();
    }
}

void WebEditorClient::handleKeyboardEvent(KeyboardEvent& event)
{
    if (m_webView->handleEditingKeyboardEvent(event))
        event.setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent&)
{
}

bool WebEditorClient::shouldEraseMarkersAfterChangeSelection(TextCheckingType) const
{
    return true;
}

void WebEditorClient::ignoreWordInSpellDocument(const String& word)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    initViewSpecificSpelling(m_webView);
    ed->ignoreWordInSpellDocument(m_webView, BString(word));
}

void WebEditorClient::learnWord(const String& word)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    ed->learnWord(BString(word));
}

void WebEditorClient::checkSpellingOfString(StringView text, int* misspellingLocation, int* misspellingLength)
{
    *misspellingLocation = -1;
    *misspellingLength = 0;

    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    initViewSpecificSpelling(m_webView);
    ed->checkSpellingOfString(m_webView, wcharFrom(text.upconvertedCharacters()), text.length(), misspellingLocation, misspellingLength);
}

String WebEditorClient::getAutoCorrectSuggestionForMisspelledWord(const String& inputWord)
{
    // This method can be implemented using customized algorithms for the particular browser.
    // Currently, it computes an empty string.
    return String();
}

void WebEditorClient::checkGrammarOfString(StringView text, Vector<GrammarDetail>& details, int* badGrammarLocation, int* badGrammarLength)
{
    details.clear();
    *badGrammarLocation = -1;
    *badGrammarLength = 0;

    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    initViewSpecificSpelling(m_webView);
    COMPtr<IEnumWebGrammarDetails> enumDetailsObj;
    if (FAILED(ed->checkGrammarOfString(m_webView, wcharFrom(text.upconvertedCharacters()), text.length(), &enumDetailsObj, badGrammarLocation, badGrammarLength)))
        return;

    while (true) {
        ULONG fetched;
        COMPtr<IWebGrammarDetail> detailObj;
        if (enumDetailsObj->Next(1, &detailObj, &fetched) != S_OK)
            break;

        int length;
        if (FAILED(detailObj->length(&length)))
            continue;
        int location;
        if (FAILED(detailObj->location(&location)))
            continue;
        BString userDesc;
        if (FAILED(detailObj->userDescription(&userDesc)))
            continue;

        GrammarDetail detail;
        detail.range = CharacterRange(location, length);
        detail.userDescription = String(userDesc, SysStringLen(userDesc));

        COMPtr<IEnumSpellingGuesses> enumGuessesObj;
        if (FAILED(detailObj->guesses(&enumGuessesObj)))
            continue;
        while (true) {
            BString guess;
            if (enumGuessesObj->Next(1, &guess, &fetched) != S_OK)
                break;
            detail.guesses.append(String(guess, SysStringLen(guess)));
        }

        details.append(detail);
    }
}

void WebEditorClient::updateSpellingUIWithGrammarString(const String& string, const WebCore::GrammarDetail& detail)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    Vector<BSTR> guessesBSTRs;
    for (unsigned i = 0; i < detail.guesses.size(); i++) {
        BString guess(detail.guesses[i]);
        guessesBSTRs.append(guess.release());
    }
    BString userDescriptionBSTR(detail.userDescription);
    ed->updateSpellingUIWithGrammarString(BString(string), detail.range.location, detail.range.length, userDescriptionBSTR, guessesBSTRs.data(), (int)guessesBSTRs.size());
    for (unsigned i = 0; i < guessesBSTRs.size(); i++)
        SysFreeString(guessesBSTRs[i]);
}

void WebEditorClient::updateSpellingUIWithMisspelledWord(const String& word)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    ed->updateSpellingUIWithMisspelledWord(BString(word));
}

void WebEditorClient::showSpellingUI(bool show)
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;
    
    ed->showSpellingUI(show);
}

bool WebEditorClient::spellingUIIsShowing()
{
    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return false;

    BOOL showing;
    if (FAILED(ed->spellingUIIsShowing(&showing)))
        return false;

    return !!showing;
}

void WebEditorClient::getGuessesForWord(const String& word, const String& context, const VisibleSelection&, Vector<String>& guesses)
{
    guesses.clear();

    COMPtr<IWebEditingDelegate> ed;
    if (FAILED(m_webView->editingDelegate(&ed)) || !ed.get())
        return;

    COMPtr<IEnumSpellingGuesses> enumGuessesObj;
    if (FAILED(ed->guessesForWord(BString(word), &enumGuessesObj)))
        return;

    while (true) {
        ULONG fetched;
        BString guess;
        if (enumGuessesObj->Next(1, &guess, &fetched) != S_OK)
            break;
        guesses.append(String(guess, SysStringLen(guess)));
    }
}

void WebEditorClient::willSetInputMethodState()
{
}

void WebEditorClient::setInputMethodState(WebCore::Element* element)
{
    m_webView->setInputMethodState(element && element->shouldUseInputMethod());
}
