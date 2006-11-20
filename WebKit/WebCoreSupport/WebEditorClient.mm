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
#import "WebLocalizableStrings.h"
#import "WebViewInternal.h"
#import "WebEditingDelegatePrivate.h"
#import <wtf/PassRefPtr.h>
#import <WebCore/EditAction.h>
#import <WebCore/EditCommand.h>

using namespace WebCore;

EditorInsertAction core(WebViewInsertAction);
WebViewInsertAction kit(EditorInsertAction);

EditorInsertAction core(WebViewInsertAction kitAction)
{
    return static_cast<EditorInsertAction>(kitAction);
}

WebViewInsertAction kit(EditorInsertAction coreAction)
{
    return static_cast<WebViewInsertAction>(coreAction);
}

@interface WebEditCommand : NSObject
{
    EditCommand *m_command;   
}

+ (WebEditCommand *)commandWithEditCommand:(PassRefPtr<EditCommand>)command;
- (EditCommand *)command;

@end

@implementation WebEditCommand

- (id)initWithEditCommand:(PassRefPtr<WebCore::EditCommand>)command
{
    ASSERT(command);
    [super init];
    m_command = command.releaseRef();
    return self;
}

- (void)dealloc
{
    m_command->deref();
    [super dealloc];
}

- (void)finalize
{
    m_command->deref();
    [super finalize];
}

+ (WebEditCommand *)commandWithEditCommand:(PassRefPtr<EditCommand>)command
{
    return [[[WebEditCommand alloc] initWithEditCommand:command] autorelease];
}

- (EditCommand *)command;
{
    return m_command;
}

@end

@interface WebEditorUndoTarget : NSObject
{
}

- (void)undoEditing:(id)arg;
- (void)redoEditing:(id)arg;

@end

@implementation WebEditorUndoTarget

- (void)undoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[WebEditCommand class]]);
    [arg command]->unapply();
}

- (void)redoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[WebEditCommand class]]);
    [arg command]->reapply();
}

@end

PassRefPtr<WebEditorClient> WebEditorClient::create()
{
    return new WebEditorClient;
}

WebEditorClient::WebEditorClient()
    : m_webFrame(nil)
    , m_undoTarget([[[WebEditorUndoTarget alloc] init] autorelease])
    , m_haveUndoRedoOperations(false)
{
}

WebEditorClient::~WebEditorClient()
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

bool WebEditorClient::selectWordBeforeMenuEvent()
{
    return [[m_webFrame webView] _selectWordBeforeMenuEvent];
}

bool WebEditorClient::isEditable()
{
    return [[m_webFrame webView] isEditable];
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

bool WebEditorClient::shouldInsertText(String text, Range* range, EditorInsertAction action)
{
    WebView* webView = [m_webFrame webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:kit(range) givenAction:kit(action)];
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

static NSString* undoNameForEditAction(EditAction editAction)
{
    switch (editAction) {
        case EditActionUnspecified: return nil;
        case EditActionSetColor: return UI_STRING_KEY("Set Color", "Set Color (Undo action name)", "Undo action name");
        case EditActionSetBackgroundColor: return UI_STRING_KEY("Set Background Color", "Set Background Color (Undo action name)", "Undo action name");
        case EditActionTurnOffKerning: return UI_STRING_KEY("Turn Off Kerning", "Turn Off Kerning (Undo action name)", "Undo action name");
        case EditActionTightenKerning: return UI_STRING_KEY("Tighten Kerning", "Tighten Kerning (Undo action name)", "Undo action name");
        case EditActionLoosenKerning: return UI_STRING_KEY("Loosen Kerning", "Loosen Kerning (Undo action name)", "Undo action name");
        case EditActionUseStandardKerning: return UI_STRING_KEY("Use Standard Kerning", "Use Standard Kerning (Undo action name)", "Undo action name");
        case EditActionTurnOffLigatures: return UI_STRING_KEY("Turn Off Ligatures", "Turn Off Ligatures (Undo action name)", "Undo action name");
        case EditActionUseStandardLigatures: return UI_STRING_KEY("Use Standard Ligatures", "Use Standard Ligatures (Undo action name)", "Undo action name");
        case EditActionUseAllLigatures: return UI_STRING_KEY("Use All Ligatures", "Use All Ligatures (Undo action name)", "Undo action name");
        case EditActionRaiseBaseline: return UI_STRING_KEY("Raise Baseline", "Raise Baseline (Undo action name)", "Undo action name");
        case EditActionLowerBaseline: return UI_STRING_KEY("Lower Baseline", "Lower Baseline (Undo action name)", "Undo action name");
        case EditActionSetTraditionalCharacterShape: return UI_STRING_KEY("Set Traditional Character Shape", "Set Traditional Character Shape (Undo action name)", "Undo action name");
        case EditActionSetFont: return UI_STRING_KEY("Set Font", "Set Font (Undo action name)", "Undo action name");
        case EditActionChangeAttributes: return UI_STRING_KEY("Change Attributes", "Change Attributes (Undo action name)", "Undo action name");
        case EditActionAlignLeft: return UI_STRING_KEY("Align Left", "Align Left (Undo action name)", "Undo action name");
        case EditActionAlignRight: return UI_STRING_KEY("Align Right", "Align Right (Undo action name)", "Undo action name");
        case EditActionCenter: return UI_STRING_KEY("Center", "Center (Undo action name)", "Undo action name");
        case EditActionJustify: return UI_STRING_KEY("Justify", "Justify (Undo action name)", "Undo action name");
        case EditActionSetWritingDirection: return UI_STRING_KEY("Set Writing Direction", "Set Writing Direction (Undo action name)", "Undo action name");
        case EditActionSubscript: return UI_STRING_KEY("Subscript", "Subscript (Undo action name)", "Undo action name");
        case EditActionSuperscript: return UI_STRING_KEY("Superscript", "Superscript (Undo action name)", "Undo action name");
        case EditActionUnderline: return UI_STRING_KEY("Underline", "Underline (Undo action name)", "Undo action name");
        case EditActionOutline: return UI_STRING_KEY("Outline", "Outline (Undo action name)", "Undo action name");
        case EditActionUnscript: return UI_STRING_KEY("Unscript", "Unscript (Undo action name)", "Undo action name");
        case EditActionDrag: return UI_STRING_KEY("Drag", "Drag (Undo action name)", "Undo action name");
        case EditActionCut: return UI_STRING_KEY("Cut", "Cut (Undo action name)", "Undo action name");
        case EditActionPaste: return UI_STRING_KEY("Paste", "Paste (Undo action name)", "Undo action name");
        case EditActionPasteFont: return UI_STRING_KEY("Paste Font", "Paste Font (Undo action name)", "Undo action name");
        case EditActionPasteRuler: return UI_STRING_KEY("Paste Ruler", "Paste Ruler (Undo action name)", "Undo action name");
        case EditActionTyping: return UI_STRING_KEY("Typing", "Typing (Undo action name)", "Undo action name");
        case EditActionCreateLink: return UI_STRING_KEY("Create Link", "Create Link (Undo action name)", "Undo action name");
        case EditActionUnlink: return UI_STRING_KEY("Unlink", "Unlink (Undo action name)", "Undo action name");
        case EditActionInsertList: return UI_STRING_KEY("Insert List", "Insert List (Undo action name)", "Undo action name");
        case EditActionFormatBlock: return UI_STRING_KEY("Formatting", "Format Block (Undo action name)", "Undo action name");
        case EditActionIndent: return UI_STRING_KEY("Indent", "Indent (Undo action name)", "Undo action name");
        case EditActionOutdent: return UI_STRING_KEY("Outdent", "Outdent (Undo action name)", "Undo action name");
    }
    return nil;
}

void WebEditorClient::registerCommandForUndoOrRedo(PassRefPtr<EditCommand> cmd, bool isRedo)
{
    ASSERT(cmd);
    
    NSUndoManager *undoManager = [[m_webFrame webView] undoManager];
    NSString *actionName = undoNameForEditAction(cmd->editingAction());
    WebEditCommand *command = [WebEditCommand commandWithEditCommand:cmd];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:(isRedo ? @selector(redoEditing:) : @selector(undoEditing:)) object:command];
    if (actionName)
        [undoManager setActionName:actionName];
    m_haveUndoRedoOperations = YES;
}

void WebEditorClient::registerCommandForUndo(PassRefPtr<EditCommand> cmd)
{
    registerCommandForUndoOrRedo(cmd, false);
}

void WebEditorClient::registerCommandForRedo(PassRefPtr<EditCommand> cmd)
{
    registerCommandForUndoOrRedo(cmd, true);
}

void WebEditorClient::clearUndoRedoOperations()
{
    if (m_haveUndoRedoOperations) {
        // workaround for <rdar://problem/4645507> NSUndoManager dies
        // with uncaught exception when undo items cleared while
        // groups are open
        NSUndoManager *undoManager = [[m_webFrame webView] undoManager];
        int groupingLevel = [undoManager groupingLevel];
        for (int i = 0; i < groupingLevel; ++i)
            [undoManager endUndoGrouping];
        
        [undoManager removeAllActionsWithTarget:m_undoTarget.get()];
        
        for (int i = 0; i < groupingLevel; ++i)
            [undoManager beginUndoGrouping];
        
        m_haveUndoRedoOperations = NO;
    }    
}

bool WebEditorClient::canUndo() const
{
    return [[[m_webFrame webView] undoManager] canUndo];
}

bool WebEditorClient::canRedo() const
{
    return [[[m_webFrame webView] undoManager] canRedo];
}

void WebEditorClient::undo()
{
    if (canUndo())
        [[[m_webFrame webView] undoManager] undo];
}

void WebEditorClient::redo()
{
    if (canRedo())
        [[[m_webFrame webView] undoManager] redo];    
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
