/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#import "WebDOMOperationsPrivate.h"

#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "DOMWheelEventInternal.h"
#import "WebArchiveInternal.h"
#import "WebDataSourcePrivate.h"
#import "WebFrameInternal.h"
#import "WebFrameLoaderClient.h"
#import "WebFramePrivate.h"
#import "WebKitNSStringExtras.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSCJSValue.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSLock.h>
#import <WebCore/Document.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/JSElement.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/PlatformWheelEvent.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderTreeAsText.h>
#import <WebCore/ShadowRoot.h>
#import <WebCore/SimpleRange.h>
#import <WebCore/WheelEvent.h>
#import <WebCore/markup.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMHTML.h>
#import <wtf/Assertions.h>

using namespace WebCore;
using namespace JSC;

@implementation DOMElement (WebDOMElementOperationsPrivate)

+ (DOMElement *)_DOMElementFromJSContext:(JSContextRef)context value:(JSValueRef)value
{
    if (!context)
        return 0;

    if (!value)
        return 0;

    JSGlobalObject* lexicalGlobalObject = toJS(context);
    JSLockHolder lock(lexicalGlobalObject);
    return kit(JSElement::toWrapped(lexicalGlobalObject->vm(), toJS(lexicalGlobalObject, value)));
}

@end

@implementation DOMNode (WebDOMNodeOperations)

- (WebArchive *)webArchive
{
    return [[[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create(*core(self))] autorelease];
}

- (WebArchive *)webArchiveByFilteringSubframes:(WebArchiveSubframeFilter)webArchiveSubframeFilter
{
    WebArchive *webArchive = [[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create(*core(self), [webArchiveSubframeFilter](Frame& subframe) -> bool {
        return webArchiveSubframeFilter(kit(&subframe));
    })];

    return [webArchive autorelease];
}

#if PLATFORM(IOS_FAMILY)

- (BOOL)isHorizontalWritingMode
{
    Node* node = core(self);
    if (!node)
        return YES;
    
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return YES;
    
    return renderer->style().isHorizontalWritingMode();
}

- (void)hidePlaceholder
{
    if (auto node = core(self); is<HTMLTextFormControlElement>(node))
        downcast<HTMLTextFormControlElement>(*node).setCanShowPlaceholder(false);
}

- (void)showPlaceholderIfNecessary
{
    if (auto node = core(self); is<HTMLTextFormControlElement>(node))
        downcast<HTMLTextFormControlElement>(*node).setCanShowPlaceholder(true);
}

#endif

@end

@implementation DOMNode (WebDOMNodeOperationsPendingPublic)

- (NSString *)markupString
{
    auto& node = *core(self);

    String markupString = serializeFragment(node, SerializedNodes::SubtreeIncludingNode);
    Node::NodeType nodeType = node.nodeType();
    if (nodeType != Node::DOCUMENT_NODE && nodeType != Node::DOCUMENT_TYPE_NODE)
        markupString = documentTypeString(node.document()) + markupString;

    return markupString;
}

- (NSRect)_renderRect:(bool *)isReplaced
{
    return NSRect(core(self)->pixelSnappedRenderRect(isReplaced));
}

@end

@implementation DOMDocument (WebDOMDocumentOperations)

- (WebFrame *)webFrame
{
    Frame* frame = core(self)->frame();
    if (!frame)
        return nil;
    return kit(frame);
}

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    return core(self)->completeURL(stripLeadingAndTrailingHTMLSpaces(string));
}

@end

@implementation DOMDocument (WebDOMDocumentOperationsInternal)

- (DOMRange *)_documentRange
{
    DOMRange *range = [self createRange];

    if (DOMNode* documentElement = [self documentElement])
        [range selectNode:documentElement];

    return range;
}

@end

@implementation DOMRange (WebDOMRangeOperations)

- (WebArchive *)webArchive
{
    return [[[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create(makeSimpleRange(*core(self)))] autorelease];
}

- (NSString *)markupString
{
    auto& range = *core(self);
    return String { documentTypeString(range.ownerDocument()) + serializePreservingVisualAppearance(makeSimpleRange(range), nullptr, AnnotateForInterchange::Yes) };
}

@end

@implementation DOMHTMLFrameElement (WebDOMHTMLFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLIFrameElement (WebDOMHTMLIFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLInputElement (WebDOMHTMLInputElementOperationsPrivate)

- (BOOL)_isAutofilled
{
    return downcast<HTMLInputElement>(core((DOMElement *)self))->isAutoFilled();
}

- (BOOL)_isAutoFilledAndViewable
{
    return downcast<HTMLInputElement>(core((DOMElement *)self))->isAutoFilledAndViewable();
}

- (void)_setAutofilled:(BOOL)autofilled
{
    downcast<HTMLInputElement>(core((DOMElement *)self))->setAutoFilled(autofilled);
}

- (void)_setAutoFilledAndViewable:(BOOL)autoFilledAndViewable
{
    downcast<HTMLInputElement>(core((DOMElement *)self))->setAutoFilledAndViewable(autoFilledAndViewable);
}

@end

@implementation DOMHTMLObjectElement (WebDOMHTMLObjectElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

#if !PLATFORM(IOS_FAMILY)
static NSEventPhase toNSEventPhase(PlatformWheelEventPhase platformPhase)
{
    uint32_t phase = PlatformWheelEventPhaseNone; 
    if (platformPhase & PlatformWheelEventPhaseBegan)
        phase |= NSEventPhaseBegan;
    if (platformPhase & PlatformWheelEventPhaseStationary)
        phase |= NSEventPhaseStationary;
    if (platformPhase & PlatformWheelEventPhaseChanged)
        phase |= NSEventPhaseChanged;
    if (platformPhase & PlatformWheelEventPhaseEnded)
        phase |= NSEventPhaseEnded;
    if (platformPhase & PlatformWheelEventPhaseCancelled)
        phase |= NSEventPhaseCancelled;
    if (platformPhase & PlatformWheelEventPhaseMayBegin)
        phase |= NSEventPhaseMayBegin;

    return static_cast<NSEventPhase>(phase);
}

@implementation DOMWheelEvent (WebDOMWheelEventOperationsPrivate)

- (NSEventPhase)_phase
{
    return toNSEventPhase(core(self)->phase());
}

- (NSEventPhase)_momentumPhase
{
    return toNSEventPhase(core(self)->momentumPhase());
}

@end
#endif
