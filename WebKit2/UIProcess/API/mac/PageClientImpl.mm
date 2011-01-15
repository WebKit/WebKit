/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "NativeWebKeyboardEvent.h"
#import "PageClientImpl.h"

#import "DataReference.h"
#import "FindIndicator.h"
#import "WKAPICast.h"
#import "WKStringCF.h"
#import "WKViewInternal.h"
#import "WebContextMenuProxyMac.h"
#import "WebEditCommandProxy.h"
#import "WebPopupMenuProxyMac.h"
#import <WebCore/Cursor.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FoundationExtras.h>
#import <WebCore/KeyboardEvent.h>
#import <wtf/PassOwnPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

@interface WebEditCommandObjC : NSObject
{
    RefPtr<WebKit::WebEditCommandProxy> m_command;
}

- (id)initWithWebEditCommandProxy:(PassRefPtr<WebKit::WebEditCommandProxy>)command;
- (WebKit::WebEditCommandProxy*)command;

@end

@implementation WebEditCommandObjC

- (id)initWithWebEditCommandProxy:(PassRefPtr<WebKit::WebEditCommandProxy>)command
{
    self = [super init];
    if (!self)
        return nil;

    m_command = command;
    return self;
}

- (WebKit::WebEditCommandProxy*)command
{
    return m_command.get();
}

@end

@interface WebEditorUndoTargetObjC : NSObject

- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;

@end

@implementation WebEditorUndoTargetObjC

- (void)undoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WebEditCommandObjC class]]);
    [sender command]->unapply();
}

- (void)redoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WebEditCommandObjC class]]);
    [sender command]->reapply();
}

@end

namespace WebKit {

NSString* nsStringFromWebCoreString(const String& string)
{
    return string.impl() ? HardAutorelease(WKStringCopyCFString(0, toAPI(string.impl()))) : @"";
}

PassOwnPtr<PageClientImpl> PageClientImpl::create(WKView* wkView)
{
    return adoptPtr(new PageClientImpl(wkView));
}

PageClientImpl::PageClientImpl(WKView* wkView)
    : m_wkView(wkView)
    , m_undoTarget(AdoptNS, [[WebEditorUndoTargetObjC alloc] init])
{
}

PageClientImpl::~PageClientImpl()
{
}

PassOwnPtr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return [m_wkView _createDrawingAreaProxy];
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    [m_wkView setNeedsDisplayInRect:rect];
}

void PageClientImpl::displayView()
{
    [m_wkView displayIfNeeded];
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([m_wkView bounds].size);
}

bool PageClientImpl::isViewWindowActive()
{
    return [[m_wkView window] isKeyWindow];
}

bool PageClientImpl::isViewFocused()
{
    return [m_wkView _isFocused];
}

bool PageClientImpl::isViewVisible()
{
    if (![m_wkView window])
        return false;

    if ([m_wkView isHiddenOrHasHiddenAncestor])
        return false;

    return true;
}

bool PageClientImpl::isViewInWindow()
{
    return [m_wkView window];
}

void PageClientImpl::processDidCrash()
{
    [m_wkView _processDidCrash];
}

void PageClientImpl::didRelaunchProcess()
{
    [m_wkView _didRelaunchProcess];
}

void PageClientImpl::takeFocus(bool direction)
{
    [m_wkView _takeFocus:direction];
}

void PageClientImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    [m_wkView _toolTipChangedFrom:nsStringFromWebCoreString(oldToolTip) to:nsStringFromWebCoreString(newToolTip)];
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    [m_wkView _setCursor:cursor.platformCursor()];
}

void PageClientImpl::setViewportArguments(const WebCore::ViewportArguments&)
{

}

static NSString* nameForEditAction(EditAction editAction)
{
    // FIXME: Use localized strings.
    // FIXME: Move this to a platform independent location.

    switch (editAction) {
    case EditActionUnspecified: return nil;
    case EditActionSetColor: return @"Set Color";
    case EditActionSetBackgroundColor: return @"Set Background Color";
    case EditActionTurnOffKerning: return @"Turn Off Kerning";
    case EditActionTightenKerning: return @"Tighten Kerning";
    case EditActionLoosenKerning: return @"Loosen Kerning";
    case EditActionUseStandardKerning: return @"Use Standard Kerning";
    case EditActionTurnOffLigatures: return @"Turn Off Ligatures";
    case EditActionUseStandardLigatures: return @"Use Standard Ligatures";
    case EditActionUseAllLigatures: return @"Use All Ligatures";
    case EditActionRaiseBaseline: return @"Raise Baseline";
    case EditActionLowerBaseline: return @"Lower Baseline";
    case EditActionSetTraditionalCharacterShape: return @"Set Traditional Character Shape";
    case EditActionSetFont: return @"Set Font";
    case EditActionChangeAttributes: return @"Change Attributes";
    case EditActionAlignLeft: return @"Align Left";
    case EditActionAlignRight: return @"Align Right";
    case EditActionCenter: return @"Center";
    case EditActionJustify: return @"Justify";
    case EditActionSetWritingDirection: return @"Set Writing Direction";
    case EditActionSubscript: return @"Subscript";
    case EditActionSuperscript: return @"Superscript";
    case EditActionUnderline: return @"Underline";
    case EditActionOutline: return @"Outline";
    case EditActionUnscript: return @"Unscript";
    case EditActionDrag: return @"Drag";
    case EditActionCut: return @"Cut";
    case EditActionPaste: return @"Paste";
    case EditActionPasteFont: return @"Paste Font";
    case EditActionPasteRuler: return @"Paste Ruler";
    case EditActionTyping: return @"Typing";
    case EditActionCreateLink: return @"Create Link";
    case EditActionUnlink: return @"Unlink";
    case EditActionInsertList: return @"Insert List";
    case EditActionFormatBlock: return @"Formatting";
    case EditActionIndent: return @"Indent";
    case EditActionOutdent: return @"Outdent";
    }
    return nil;
}

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    RefPtr<WebEditCommandProxy> command = prpCommand;

    RetainPtr<WebEditCommandObjC> commandObjC(AdoptNS, [[WebEditCommandObjC alloc] initWithWebEditCommandProxy:command]);
    NSString *actionName = nameForEditAction(command->editAction());

    NSUndoManager *undoManager = [m_wkView undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == WebPageProxy::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (actionName)
        [undoManager setActionName:actionName];
}

void PageClientImpl::clearAllEditCommands()
{
    [[m_wkView undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

void PageClientImpl::setEditCommandState(const String& commandName, bool isEnabled, int newState)
{
    [m_wkView _setUserInterfaceItemState:nsStringFromWebCoreString(commandName) enabled:isEnabled state:newState];
}

void PageClientImpl::interceptKeyEvent(const NativeWebKeyboardEvent& event, Vector<WebCore::KeypressCommand>& commandsList, uint32_t selectionStart, uint32_t selectionEnd, Vector<WebCore::CompositionUnderline>& underlines)
{
    commandsList = [m_wkView _interceptKeyEvent:event.nativeEvent()];
    [m_wkView _getTextInputState:selectionStart selectionEnd:selectionEnd underlines:underlines];
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    return [m_wkView _convertToDeviceSpace:rect];
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    return [m_wkView _convertToUserSpace:rect];
}

void PageClientImpl::didNotHandleKeyEvent(const NativeWebKeyboardEvent& event)
{
    NSEvent* nativeEvent = event.nativeEvent();
    if ([nativeEvent type] == NSKeyDown) {
        [m_wkView _setEventBeingResent:nativeEvent];
        [[NSApplication sharedApplication] sendEvent:nativeEvent];
    }
}

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuProxyMac::create(m_wkView, page);
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyMac::create(m_wkView, page);
}

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator> findIndicator, bool fadeOut)
{
    [m_wkView _setFindIndicator:findIndicator fadeOut:fadeOut];
}

void PageClientImpl::accessibilityChildTokenReceived(const CoreIPC::DataReference& data)
{
    NSData* remoteToken = [NSData dataWithBytes:data.data() length:data.size()];
    [m_wkView _setAccessibilityChildToken:remoteToken];
}
    
#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::pageDidEnterAcceleratedCompositing()
{
    [m_wkView _pageDidEnterAcceleratedCompositing];
}

void PageClientImpl::pageDidLeaveAcceleratedCompositing()
{
    [m_wkView _pageDidLeaveAcceleratedCompositing];
}
#endif // USE(ACCELERATED_COMPOSITING)

void PageClientImpl::setComplexTextInputEnabled(uint64_t pluginComplexTextInputIdentifier, bool complexTextInputEnabled)
{
    [m_wkView _setComplexTextInputEnabled:complexTextInputEnabled pluginComplexTextInputIdentifier:pluginComplexTextInputIdentifier];
}

void PageClientImpl::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
    [m_wkView _setPageHasCustomRepresentation:useCustomRepresentation];
}

void PageClientImpl::didFinishLoadingDataForCustomRepresentation(const CoreIPC::DataReference& dataReference)
{
    [m_wkView _didFinishLoadingDataForCustomRepresentation:dataReference];
}

double PageClientImpl::customRepresentationZoomFactor()
{
    return [m_wkView _customRepresentationZoomFactor];
}

void PageClientImpl::setCustomRepresentationZoomFactor(double zoomFactor)
{
    [m_wkView _setCustomRepresentationZoomFactor:zoomFactor];
}

} // namespace WebKit
