/*
 * Copyright (C) 2008, 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebAccessibilityObjectWrapperIOS.h"

#if ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)

#import "AccessibilityAttachment.h"
#import "AccessibilityMediaObject.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityScrollView.h"
#import "AccessibilityTable.h"
#import "AccessibilityTableCell.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "HitTestResult.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "IntRect.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "Range.h"
#import "RenderView.h"
#import "RuntimeApplicationChecks.h"
#import "SVGElementInlines.h"
#import "SVGNames.h"
#import "SelectionGeometry.h"
#import "SimpleRange.h"
#import "TextIterator.h"
#import "WAKScrollView.h"
#import "WAKWindow.h"
#import "WebCoreThread.h"
#import "VisibleUnits.h"
#import <CoreText/CoreText.h>
#import <wtf/cocoa/VectorCocoa.h>

enum {
    NSAttachmentCharacter = 0xfffc    /* To denote attachments. */
};

@interface NSObject (AccessibilityPrivate)
- (void)_accessibilityUnregister;
- (NSString *)accessibilityLabel;
- (NSString *)accessibilityValue;
- (BOOL)isAccessibilityElement;
- (NSInteger)accessibilityElementCount;
- (id)accessibilityElementAtIndex:(NSInteger)index;
- (NSInteger)indexOfAccessibilityElement:(id)element;
- (NSArray *)accessibilityElements;
@end

@interface WebAccessibilityObjectWrapper (AccessibilityPrivate)
- (id)accessibilityContainer;
- (void)setAccessibilityLabel:(NSString *)label;
- (void)setAccessibilityValue:(NSString *)value;
- (BOOL)containsUnnaturallySegmentedChildren;
- (NSInteger)positionForTextMarker:(id)marker;
@end

@implementation WAKView (iOSAccessibility)

- (BOOL)accessibilityIsIgnored
{
    return YES;
}

@end

using namespace WebCore;
using namespace HTMLNames;

typedef NS_ENUM(NSInteger, UIAccessibilityScrollDirection) {
    UIAccessibilityScrollDirectionRight = 1,
    UIAccessibilityScrollDirectionLeft,
    UIAccessibilityScrollDirectionUp,
    UIAccessibilityScrollDirectionDown,
    UIAccessibilityScrollDirectionNext,
    UIAccessibilityScrollDirectionPrevious
};

static AccessibilityObjectWrapper* AccessibilityUnignoredAncestor(AccessibilityObjectWrapper *wrapper)
{
    while (wrapper && ![wrapper isAccessibilityElement]) {
        AXCoreObject* object = wrapper.axBackingObject;
        if (!object)
            break;
        
        if ([wrapper isAttachment] && ![[wrapper attachmentView] accessibilityIsIgnored])
            break;
            
        AXCoreObject* parentObject = object->parentObjectUnignored();
        if (!parentObject)
            break;

        wrapper = parentObject->wrapper();
    }
    return wrapper;
}

#pragma mark Accessibility Text Marker

@interface WebAccessibilityTextMarker : NSObject
{
    AXObjectCache* _cache;
    TextMarkerData _textMarkerData;
}

+ (WebAccessibilityTextMarker *)textMarkerWithVisiblePosition:(VisiblePosition&)visiblePos cache:(AXObjectCache*)cache;
+ (WebAccessibilityTextMarker *)textMarkerWithCharacterOffset:(CharacterOffset&)characterOffset cache:(AXObjectCache*)cache;
+ (WebAccessibilityTextMarker *)startOrEndTextMarkerForRange:(const std::optional<SimpleRange>&)range isStart:(BOOL)isStart cache:(AXObjectCache*)cache;

@end

@implementation WebAccessibilityTextMarker

- (id)initWithTextMarker:(TextMarkerData *)data cache:(AXObjectCache*)cache
{
    if (!(self = [super init]))
        return nil;
    
    _cache = cache;
    memcpy(&_textMarkerData, data, sizeof(TextMarkerData));
    return self;
}

- (id)initWithData:(NSData *)data cache:(AXObjectCache*)cache
{
    if (!(self = [super init]))
        return nil;
    
    _cache = cache;
    [data getBytes:&_textMarkerData length:sizeof(TextMarkerData)];
    
    return self;
}

// This is needed for external clients to be able to create a text marker without having a pointer to the cache.
- (id)initWithData:(NSData *)data accessibilityObject:(AccessibilityObjectWrapper *)wrapper
{
    WebCore::AXCoreObject* axObject = wrapper.axBackingObject;
    if (!axObject)
        return nil;
    
    return [self initWithData:data cache:axObject->axObjectCache()];
}

+ (WebAccessibilityTextMarker *)textMarkerWithVisiblePosition:(VisiblePosition&)visiblePos cache:(AXObjectCache*)cache
{
    auto textMarkerData = cache->textMarkerDataForVisiblePosition(visiblePos);
    if (!textMarkerData)
        return nil;
    return adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData.value() cache:cache]).autorelease();
}

+ (WebAccessibilityTextMarker *)textMarkerWithCharacterOffset:(CharacterOffset&)characterOffset cache:(AXObjectCache*)cache
{
    if (!cache)
        return nil;
    
    if (characterOffset.isNull())
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID && !textMarkerData.ignored)
        return nil;
    return adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache]).autorelease();
}

+ (WebAccessibilityTextMarker *)startOrEndTextMarkerForRange:(const std::optional<SimpleRange>&)range isStart:(BOOL)isStart cache:(AXObjectCache*)cache
{
    if (!cache)
        return nil;
    if (!range)
        return nil;
    TextMarkerData textMarkerData;
    cache->startOrEndTextMarkerDataForRange(textMarkerData, *range, isStart);
    if (!textMarkerData.axID)
        return nil;
    return adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache]).autorelease();
}

- (NSData *)dataRepresentation
{
    return [NSData dataWithBytes:&_textMarkerData length:sizeof(TextMarkerData)];
}

- (VisiblePosition)visiblePosition
{
    return _cache->visiblePositionForTextMarkerData(_textMarkerData);
}

- (CharacterOffset)characterOffset
{
    return _cache->characterOffsetForTextMarkerData(_textMarkerData);
}

- (BOOL)isIgnored
{
    return _textMarkerData.ignored;
}

- (AccessibilityObject*)accessibilityObject
{
    if (_textMarkerData.ignored)
        return nullptr;
    return _cache->accessibilityObjectForTextMarkerData(_textMarkerData);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"[AXTextMarker %p] = node: %p offset: %d", self, _textMarkerData.node, _textMarkerData.offset];
}

@end

@implementation WebAccessibilityObjectWrapper

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    self = [super initWithAccessibilityObject:axObject];
    if (!self)
        return nil;
    
    // Initialize to a sentinel value.
    m_accessibilityTraitsFromAncestor = ULLONG_MAX;
    m_isAccessibilityElement = -1;
    
    return self;
}

- (WebCore::AccessibilityObject *)axBackingObject
{
    return m_axObject;
}

- (void)detach
{
    // rdar://8798960 Make sure the object is gone early, so that anything _accessibilityUnregister 
    // does can't call back into the render tree.
    [super detach];

    if ([self respondsToSelector:@selector(_accessibilityUnregister)])
        [self _accessibilityUnregister];
}

- (void)dealloc
{
    // We should have been detached before deallocated.
    ASSERT(!self.axBackingObject);
    [super dealloc];
}

// These are here so that we don't have to import AXRuntime.
// The methods will be swizzled when the accessibility bundle is loaded.

- (uint64_t)_axLinkTrait { return (1 << 0); }
- (uint64_t)_axVisitedTrait { return (1 << 1); }
- (uint64_t)_axHeaderTrait { return (1 << 2); }
- (uint64_t)_axContainedByListTrait { return (1 << 3); }
- (uint64_t)_axContainedByTableTrait { return (1 << 4); }
- (uint64_t)_axContainedByLandmarkTrait { return (1 << 5); }
- (uint64_t)_axWebContentTrait { return (1 << 6); }
- (uint64_t)_axSecureTextFieldTrait { return (1 << 7); }
- (uint64_t)_axTextEntryTrait { return (1 << 8); }
- (uint64_t)_axHasTextCursorTrait { return (1 << 9); }
- (uint64_t)_axTextOperationsAvailableTrait { return (1 << 10); }
- (uint64_t)_axImageTrait { return (1 << 11); }
- (uint64_t)_axTabButtonTrait { return (1 << 12); }
- (uint64_t)_axButtonTrait { return (1 << 13); }
- (uint64_t)_axToggleTrait { return (1 << 14); }
- (uint64_t)_axPopupButtonTrait { return (1 << 15); }
- (uint64_t)_axStaticTextTrait { return (1 << 16); }
- (uint64_t)_axAdjustableTrait { return (1 << 17); }
- (uint64_t)_axMenuItemTrait { return (1 << 18); }
- (uint64_t)_axSelectedTrait { return (1 << 19); }
- (uint64_t)_axNotEnabledTrait { return (1 << 20); }
- (uint64_t)_axRadioButtonTrait { return (1 << 21); }
- (uint64_t)_axContainedByFieldsetTrait { return (1 << 22); }
- (uint64_t)_axSearchFieldTrait { return (1 << 23); }
- (uint64_t)_axTextAreaTrait { return (1 << 24); }
- (uint64_t)_axUpdatesFrequentlyTrait { return (1 << 25); }

- (NSString *)accessibilityDOMIdentifier
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->identifierAttribute();
}

- (BOOL)accessibilityCanFuzzyHitTest
{
    if (![self _prepareAccessibilityCall])
        return false;
    
    AccessibilityRole role = self.axBackingObject->roleValue();
    // Elements that can be returned when performing fuzzy hit testing.
    switch (role) {
    case AccessibilityRole::Button:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::ColorWell:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::DisclosureTriangle:
    case AccessibilityRole::Heading:
    case AccessibilityRole::ImageMapLink:
    case AccessibilityRole::Image:
    case AccessibilityRole::Link:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::Slider:
    case AccessibilityRole::StaticText:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return !self.axBackingObject->accessibilityIsIgnored();
    default:
        return false;
    }
}

- (AccessibilityObjectWrapper *)accessibilityPostProcessHitTest:(CGPoint)point
{
    UNUSED_PARAM(point);
    // The UIKit accessibility wrapper will override this and perform the post process hit test.
    return nil;
}

- (id)accessibilityHitTest:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    // Try a fuzzy hit test first to find an accessible element.
    AXCoreObject *axObject = nullptr;
    {
        AXAttributeCacheEnabler enableCache(self.axBackingObject->axObjectCache());
        axObject = self.axBackingObject->accessibilityHitTest(IntPoint(point));
    }

    if (!axObject)
        return nil;
    
    // If this is a good accessible object to return, no extra work is required.
    if ([axObject->wrapper() accessibilityCanFuzzyHitTest])
        return AccessibilityUnignoredAncestor(axObject->wrapper());
    
    // Check to see if we can post-process this hit test to find a better candidate.
    AccessibilityObjectWrapper* wrapper = [axObject->wrapper() accessibilityPostProcessHitTest:point];
    if (wrapper)
        return AccessibilityUnignoredAncestor(wrapper);
    
    // Fall back to default behavior.
    return AccessibilityUnignoredAncestor(axObject->wrapper());    
}

- (void)enableAttributeCaching
{
    if (auto* cache = self.axBackingObject->axObjectCache())
        cache->startCachingComputedObjectAttributesUntilTreeMutates();
}

- (void)disableAttributeCaching
{
    if (auto* cache = self.axBackingObject->axObjectCache())
        cache->stopCachingComputedObjectAttributes();
}

- (NSArray *)accessibilityElements
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView accessibilityElements];
    }

    auto array = adoptNS([[NSMutableArray alloc] init]);
    for (const auto& child : self.axBackingObject->children()) {
        auto* wrapper = child->wrapper();
        if (child->isAttachment()) {
            if (id attachmentView = [wrapper attachmentView])
                [array addObject:attachmentView];
        } else
            [array addObject:wrapper];
    }
    
#if ENABLE(MODEL_ELEMENT)
    if (self.axBackingObject->isModel()) {
        for (auto child : self.axBackingObject->modelElementChildren())
            [array addObject:child.get()];
    }
#endif

    return array.autorelease();
}

- (NSInteger)accessibilityElementCount
{
    if (![self _prepareAccessibilityCall])
        return 0;

    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView accessibilityElementCount];
    }

    return self.axBackingObject->children().size();
}

- (id)accessibilityElementAtIndex:(NSInteger)index
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView accessibilityElementAtIndex:index];
    }
    
    const auto& children = self.axBackingObject->children();
    size_t elementIndex = static_cast<size_t>(index);
    if (elementIndex >= children.size())
        return nil;
    
    AccessibilityObjectWrapper* wrapper = children[elementIndex]->wrapper();
    if (children[elementIndex]->isAttachment()) {
        if (id attachmentView = [wrapper attachmentView])
            return attachmentView;
    }

    return wrapper;
}

- (NSInteger)indexOfAccessibilityElement:(id)element
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    
    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView indexOfAccessibilityElement:element];
    }
    
    const auto& children = self.axBackingObject->children();
    unsigned count = children.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObjectWrapper* wrapper = children[k]->wrapper();
        if (wrapper == element || (children[k]->isAttachment() && [wrapper attachmentView] == element))
            return k;
    }
    
    return NSNotFound;
}

- (CGPathRef)_accessibilityPath
{
    if (![self _prepareAccessibilityCall])
        return NULL;

    if (!self.axBackingObject->supportsPath())
        return NULL;
    
    Path path = self.axBackingObject->elementPath();
    if (path.isEmpty())
        return NULL;
    
    return [self convertPathToScreenSpace:path];
}

- (NSString *)_accessibilityWebRoleAsString
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return accessibilityRoleToString(self.axBackingObject->roleValue());
}

- (BOOL)accessibilityHasPopup
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->hasPopup();
}

- (NSString *)accessibilityPopupValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->popupValue();
}

- (BOOL)accessibilityHasDocumentRoleAncestor
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->hasDocumentRoleAncestor();
}

- (BOOL)accessibilityHasWebApplicationAncestor
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->hasWebApplicationAncestor();
}

- (BOOL)accessibilityIsInDescriptionListDefinition
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isInDescriptionListDetail();
}

- (BOOL)accessibilityIsInDescriptionListTerm
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isInDescriptionListTerm();
}

- (BOOL)_accessibilityIsInTableCell
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isInCell();
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attributeName
{
    if (![self _prepareAccessibilityCall])
        return NO;

    if ([attributeName isEqualToString:@"AXValue"])
        return self.axBackingObject->canSetValueAttribute();

    return NO;
}

- (BOOL)accessibilityIsRequired
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isRequired();
}

- (NSString *)accessibilityLanguage
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return self.axBackingObject->language();
}

- (BOOL)accessibilityIsDialog
{
    if (![self _prepareAccessibilityCall])
        return NO;

    AccessibilityRole roleValue = self.axBackingObject->roleValue();
    return roleValue == AccessibilityRole::ApplicationDialog || roleValue == AccessibilityRole::ApplicationAlertDialog;
}

- (BOOL)_accessibilityIsLandmarkRole:(AccessibilityRole)role
{
    switch (role) {
    case AccessibilityRole::Document:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::DocumentNote:
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
        return YES;
    default:
        return NO;
    }    
}

static AccessibilityObjectWrapper *ancestorWithRole(const AXCoreObject& descendant, const AccessibilityRoleSet& roles)
{
    auto* ancestor = Accessibility::findAncestor(descendant, false, [&roles] (const auto& object) {
        return roles.contains(object.roleValue());
    });
    return ancestor ? ancestor->wrapper() : nil;
}

- (AccessibilityObjectWrapper *)_accessibilityTreeAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::Tree });
}

- (AccessibilityObjectWrapper *)_accessibilityDescriptionListAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::DescriptionList });
}

- (AccessibilityObjectWrapper *)_accessibilityListAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::List, AccessibilityRole::ListBox });
}

- (AccessibilityObjectWrapper *)_accessibilityArticleAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::DocumentArticle });
}

- (AccessibilityObjectWrapper *)_accessibilityLandmarkAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::Document,
        AccessibilityRole::DocumentArticle,
        AccessibilityRole::DocumentNote,
        AccessibilityRole::LandmarkBanner,
        AccessibilityRole::LandmarkComplementary,
        AccessibilityRole::LandmarkContentInfo,
        AccessibilityRole::LandmarkDocRegion,
        AccessibilityRole::LandmarkMain,
        AccessibilityRole::LandmarkNavigation,
        AccessibilityRole::LandmarkRegion,
        AccessibilityRole::LandmarkSearch });
}

- (AccessibilityObjectWrapper *)_accessibilityTableAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::Table,
        AccessibilityRole::TreeGrid,
        AccessibilityRole::Grid });
}

- (AccessibilityObjectWrapper *)_accessibilityFieldsetAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto* ancestor = Accessibility::findAncestor(*self.axBackingObject, false, [] (const auto& object) {
        return object.isFieldset();
    });
    return ancestor ? ancestor->wrapper() : nil;
}

- (AccessibilityObjectWrapper *)_accessibilityFrameAncestor
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::WebArea });
}

// FIXME: This traversal should be entirely replaced by AXAncestorFlags.
- (uint64_t)_accessibilityTraitsFromAncestors
{
    uint64_t traits = 0;
    auto* backingObject = self.axBackingObject;

    // Trait information also needs to be gathered from the parents above the object.
    // The parentObject is needed instead of the unignoredParentObject, because a table might be ignored, but information still needs to be gathered from it.
    for (auto* parent = backingObject->parentObject(); parent; parent = parent->parentObject()) {
        AccessibilityRole parentRole = parent->roleValue();
        if (parentRole == AccessibilityRole::WebArea)
            break;

        switch (parentRole) {
        case AccessibilityRole::Link:
        case AccessibilityRole::WebCoreLink:
            traits |= [self _axLinkTrait];
            if (parent->isVisited())
                traits |= [self _axVisitedTrait];
            break;
        case AccessibilityRole::Heading:
            traits |= [self _axHeaderTrait];
            break;
        case AccessibilityRole::Cell:
        case AccessibilityRole::GridCell:
            if (parent->isSelected())
                traits |= [self _axSelectedTrait];
            break;
        default:
            if ([self _accessibilityIsLandmarkRole:parentRole])
                traits |= [self _axContainedByLandmarkTrait];
            break;
        }

        // If this object has fieldset parent, we should add containedByFieldsetTrait to it.
        if (parent->isFieldset())
            traits |= [self _axContainedByFieldsetTrait];
    }

    return traits;
}

- (BOOL)accessibilityIsWebInteractiveVideo
{
    if (![self _prepareAccessibilityCall])
        return NO;

    if (self.axBackingObject->roleValue() != AccessibilityRole::Video || !is<AccessibilityMediaObject>(self.axBackingObject))
        return NO;

    // Convey the video object as interactive if auto-play is not enabled.
    return !downcast<AccessibilityMediaObject>(*self.axBackingObject).isAutoplayEnabled();
}

- (NSString *)interactiveVideoDescription
{
    if (!is<AccessibilityMediaObject>(self.axBackingObject))
        return nil;
    return downcast<AccessibilityMediaObject>(self.axBackingObject)->interactiveVideoDuration();
}

- (BOOL)accessibilityIsMediaPlaying
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    if (!is<AccessibilityMediaObject>(self.axBackingObject))
        return NO;
    
    return downcast<AccessibilityMediaObject>(self.axBackingObject)->isPlaying();
}

- (BOOL)accessibilityIsMediaMuted
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    if (!is<AccessibilityMediaObject>(self.axBackingObject))
        return NO;
    
    return downcast<AccessibilityMediaObject>(self.axBackingObject)->isMuted();
}

- (void)accessibilityToggleMuteForMedia
{
    if (![self _prepareAccessibilityCall])
        return;
    
    if (!is<AccessibilityMediaObject>(self.axBackingObject))
        return;

    downcast<AccessibilityMediaObject>(self.axBackingObject)->toggleMute();
}

- (void)accessibilityVideoEnterFullscreen
{
    if (![self _prepareAccessibilityCall])
        return;
    
    if (!is<AccessibilityMediaObject>(self.axBackingObject))
        return;
    
    downcast<AccessibilityMediaObject>(self.axBackingObject)->enterFullscreen();
}

- (uint64_t)_accessibilityTextEntryTraits
{
    uint64_t traits = [self _axTextEntryTrait];

    auto* backingObject = self.axBackingObject;
    if (backingObject->isFocused())
        traits |= ([self _axHasTextCursorTrait] | [self _axTextOperationsAvailableTrait]);
    if (backingObject->isPasswordField())
        traits |= [self _axSecureTextFieldTrait];

    switch (backingObject->roleValue()) {
    case AccessibilityRole::SearchField:
        traits |= [self _axSearchFieldTrait];
        break;
    case AccessibilityRole::TextArea:
        traits |= [self _axTextAreaTrait];
        break;
    default:
        break;
    }

    return traits;
}

- (uint64_t)accessibilityTraits
{
    if (![self _prepareAccessibilityCall])
        return 0;

    AccessibilityRole role = self.axBackingObject->roleValue();
    uint64_t traits = [self _axWebContentTrait];
    switch (role) {
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
        traits |= [self _axLinkTrait];
        if (self.axBackingObject->isVisited())
            traits |= [self _axVisitedTrait];
        break;
    case AccessibilityRole::TextField:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::TextArea:
        traits |= [self _accessibilityTextEntryTraits];
        break;
    case AccessibilityRole::Image:
        traits |= [self _axImageTrait];
        break;
    case AccessibilityRole::Tab:
        traits |= [self _axTabButtonTrait];
        break;
    case AccessibilityRole::Button:
        traits |= [self _axButtonTrait];
        if (self.axBackingObject->isPressed())
            traits |= [self _axToggleTrait];
        break;
    case AccessibilityRole::PopUpButton:
        traits |= [self _axPopupButtonTrait];
        break;
    case AccessibilityRole::RadioButton:
        traits |= [self _axRadioButtonTrait] | [self _axToggleTrait];
        break;
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::Switch:
        traits |= ([self _axButtonTrait] | [self _axToggleTrait]);
        break;
    case AccessibilityRole::Heading:
        traits |= [self _axHeaderTrait];
        break;
    case AccessibilityRole::StaticText:
        traits |= [self _axStaticTextTrait];
        break;
    case AccessibilityRole::Slider:
    case AccessibilityRole::SpinButton:
        traits |= [self _axAdjustableTrait];
        break;
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
        traits |= [self _axMenuItemTrait];
        break;
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
        traits |= ([self _axMenuItemTrait] | [self _axToggleTrait]);
        break;
    default:
        break;
    }

    if (self.axBackingObject->isAttachmentElement())
        traits |= [self _axUpdatesFrequentlyTrait];
    
    if (self.axBackingObject->isSelected())
        traits |= [self _axSelectedTrait];

    if (!self.axBackingObject->isEnabled())
        traits |= [self _axNotEnabledTrait];
    
    // If the treeitem supports the checked state, then it should also be marked with toggle status.
    if (self.axBackingObject->supportsCheckedState())
        traits |= [self _axToggleTrait];

    if (m_accessibilityTraitsFromAncestor == ULLONG_MAX)
        m_accessibilityTraitsFromAncestor = [self _accessibilityTraitsFromAncestors];
    
    traits |= m_accessibilityTraitsFromAncestor;

    return traits;
}

- (BOOL)isSVGGroupElement
{
    // If an SVG group element has a title, it should be an accessible element on iOS.
    Node* node = self.axBackingObject->node();
    if (node && node->hasTagName(SVGNames::gTag) && [[self accessibilityLabel] length] > 0)
        return YES;
    
    return NO;
}

- (BOOL)determineIsAccessibilityElement
{
    if (!self.axBackingObject)
        return false;
    
    // Honor when something explicitly makes this an element (super will contain that logic) 
    if ([super isAccessibilityElement])
        return YES;
    
    self.axBackingObject->updateBackingStore();
    
    switch (self.axBackingObject->roleValue()) {
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::ColorWell:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Slider:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::ValueIndicator:
    case AccessibilityRole::Image:
    case AccessibilityRole::ImageMapLink:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Meter:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::Incrementor:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::DisclosureTriangle:
    case AccessibilityRole::ImageMap:
    case AccessibilityRole::ListMarker:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::Tab:
    case AccessibilityRole::DocumentMath:
    case AccessibilityRole::HorizontalRule:
    case AccessibilityRole::SliderThumb:
    case AccessibilityRole::Switch:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::SpinButton:
        return true;
    case AccessibilityRole::StaticText: {
        // Many text elements only contain a space.
        if (![[[self accessibilityLabel] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length])
            return false;

        // Text elements that are just pieces of links or headers should not be exposed.
        if ([AccessibilityUnignoredAncestor([self accessibilityContainer]) containsUnnaturallySegmentedChildren])
            return false;
        return true;
    }
        
    // Don't expose headers as elements; instead expose their children as elements, with the header trait (unless they have no children)
    case AccessibilityRole::Heading:
        if (![self accessibilityElementCount])
            return true;
        return false;
        
    case AccessibilityRole::Video:
        return [self accessibilityIsWebInteractiveVideo];
        
    // Links can sometimes be elements (when they only contain static text or don't contain anything).
    // They should not be elements when containing text and other types.
    case AccessibilityRole::WebCoreLink:
    case AccessibilityRole::Link:
        if ([self containsUnnaturallySegmentedChildren] || ![self accessibilityElementCount])
            return true;
        return false;
    case AccessibilityRole::Group:
        if ([self isSVGGroupElement])
            return true;
        FALLTHROUGH;
    case AccessibilityRole::Annotation:
    case AccessibilityRole::Application:
    case AccessibilityRole::ApplicationAlert:
    case AccessibilityRole::ApplicationAlertDialog:
    case AccessibilityRole::ApplicationDialog:
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ApplicationLog:
    case AccessibilityRole::ApplicationMarquee:
    case AccessibilityRole::ApplicationStatus:
    case AccessibilityRole::ApplicationTextGroup:
    case AccessibilityRole::ApplicationTimer:
    case AccessibilityRole::Audio:
    case AccessibilityRole::Blockquote:
    case AccessibilityRole::Browser:
    case AccessibilityRole::BusyIndicator:
    case AccessibilityRole::Canvas:
    case AccessibilityRole::Caption:
    case AccessibilityRole::Cell:
    case AccessibilityRole::Column:
    case AccessibilityRole::ColumnHeader:
    case AccessibilityRole::Definition:
    case AccessibilityRole::Deletion:
    case AccessibilityRole::DescriptionList:
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::DescriptionListDetail:
    case AccessibilityRole::Details:
    case AccessibilityRole::Directory:
    case AccessibilityRole::Div:
    case AccessibilityRole::Document:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::DocumentNote:
    case AccessibilityRole::Drawer:
    case AccessibilityRole::EditableText:
    case AccessibilityRole::Feed:
    case AccessibilityRole::Figure:
    case AccessibilityRole::Footer:
    case AccessibilityRole::Footnote:
    case AccessibilityRole::Form:
    case AccessibilityRole::GraphicsDocument:
    case AccessibilityRole::GraphicsObject:
    case AccessibilityRole::GraphicsSymbol:
    case AccessibilityRole::Grid:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::GrowArea:
    case AccessibilityRole::HelpTag:
    case AccessibilityRole::Inline:
    case AccessibilityRole::Insertion:
    case AccessibilityRole::Label:
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
    case AccessibilityRole::Legend:
    case AccessibilityRole::List:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::Mark:
    case AccessibilityRole::MathElement:
    case AccessibilityRole::Matte:
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::MenuListPopup:
    case AccessibilityRole::MenuListOption:
    case AccessibilityRole::Model:
    case AccessibilityRole::Outline:
    case AccessibilityRole::Paragraph:
    case AccessibilityRole::Pre:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::RowGroup:
    case AccessibilityRole::RowHeader:
    case AccessibilityRole::Row:
    case AccessibilityRole::RubyBase:
    case AccessibilityRole::RubyBlock:
    case AccessibilityRole::RubyInline:
    case AccessibilityRole::RubyRun:
    case AccessibilityRole::RubyText:
    case AccessibilityRole::Ruler:
    case AccessibilityRole::RulerMarker:
    case AccessibilityRole::ScrollArea:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::Sheet:
    case AccessibilityRole::SpinButtonPart:
    case AccessibilityRole::SplitGroup:
    case AccessibilityRole::Splitter:
    case AccessibilityRole::Subscript:
    case AccessibilityRole::Suggestion:
    case AccessibilityRole::Superscript:
    case AccessibilityRole::Summary:
    case AccessibilityRole::SystemWide:
    case AccessibilityRole::SVGRoot:
    case AccessibilityRole::SVGTextPath:
    case AccessibilityRole::SVGText:
    case AccessibilityRole::SVGTSpan:
    case AccessibilityRole::TabGroup:
    case AccessibilityRole::TabList:
    case AccessibilityRole::TabPanel:
    case AccessibilityRole::Table:
    case AccessibilityRole::TableHeaderContainer:
    case AccessibilityRole::Term:
    case AccessibilityRole::TextGroup:
    case AccessibilityRole::Time:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeItem:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::Toolbar:
    case AccessibilityRole::UserInterfaceTooltip:
    case AccessibilityRole::WebApplication:
    case AccessibilityRole::WebArea:
    case AccessibilityRole::Window:
        // Consider focusable leaf-nodes with a label to be accessible elements.
        // https://bugs.webkit.org/show_bug.cgi?id=223492
        return self.axBackingObject->isKeyboardFocusable()
            && [self accessibilityElementCount] == 0
            && self.axBackingObject->descriptionAttributeValue().find(isNotSpaceOrNewline) != notFound;
    case AccessibilityRole::Ignored:
    case AccessibilityRole::Presentational:
    case AccessibilityRole::Unknown:
        return false;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

- (BOOL)isAccessibilityElement
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    if (m_isAccessibilityElement == -1)
        m_isAccessibilityElement = [self determineIsAccessibilityElement];
    
    return m_isAccessibilityElement;
}

- (BOOL)stringValueShouldBeUsedInLabel
{
    if (self.axBackingObject->isTextControl())
        return NO;
    if (self.axBackingObject->roleValue() == AccessibilityRole::PopUpButton)
        return NO;
    if (self.axBackingObject->isFileUploadButton())
        return NO;
    if ([self accessibilityIsWebInteractiveVideo])
        return NO;

    return YES;
}

static void appendStringToResult(NSMutableString *result, NSString *string)
{
    ASSERT(result);
    if (![string length])
        return;
    if ([result length])
        [result appendString:@", "];
    [result appendString:string];
}

- (BOOL)_accessibilityHasTouchEventListener
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->hasTouchEventListener();
}

- (BOOL)_accessibilityIsStrongPasswordField
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    if (!self.axBackingObject->isPasswordField())
        return NO;
    
    return self.axBackingObject->valueAutofillButtonType() == AutoFillButtonType::StrongPassword;
}

- (CGFloat)_accessibilityMinValue
{
    if (![self _prepareAccessibilityCall])
        return 0;
    
    return self.axBackingObject->minValueForRange();
}

- (CGFloat)_accessibilityMaxValue
{
    if (![self _prepareAccessibilityCall])
        return 0;
    
    return self.axBackingObject->maxValueForRange();
}

- (NSString *)accessibilityRoleDescription
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (self.axBackingObject->isColorWell())
        return AXColorWellText();

    return self.axBackingObject->roleDescription();
}

- (NSString *)accessibilityBrailleLabel
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return self.axBackingObject->brailleLabel();
}

- (NSString *)accessibilityBrailleRoleDescription
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return self.axBackingObject->brailleRoleDescription();
}

- (NSString *)accessibilityLabel
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // check if the label was overridden
    NSString *label = [super accessibilityLabel];
    if (label)
        return label;

    auto* backingObject = self.axBackingObject;

    // If self is static text inside a heading, the label should be the string
    // value of the static text object, except when the heading has alternative
    // text, in which case, that alternative text is returned here.
    // The reason is that the string value for static text inside a heading is
    // used to convey the heading level instead.
    if (backingObject->roleValue() == AccessibilityRole::StaticText
        && self.accessibilityTraits & self._axHeaderTrait) {
        auto* heading = Accessibility::findAncestor(*backingObject, false, [] (const auto& ancestor) {
            return ancestor.roleValue() == AccessibilityRole::Heading;
        });

        if (heading) {
            auto headingLabel = heading->descriptionAttributeValue();
            if (!headingLabel.isEmpty())
                return headingLabel;
            return backingObject->stringValue();
        }
    }

    // iOS doesn't distinguish between a title and description field,
    // so concatentation will yield the best result.
    NSString *axTitle = backingObject->titleAttributeValue();
    NSString *axDescription = backingObject->descriptionAttributeValue();
    NSString *landmarkDescription = [self ariaLandmarkRoleDescription];
    NSString *interactiveVideoDescription = [self interactiveVideoDescription];

    // We should expose the value of the input type date or time through AXValue instead of AXTitle.
    if (backingObject->isInputTypePopupButton() && [axTitle isEqualToString:[self accessibilityValue]])
        axTitle = nil;

    // Footer is not considered a landmark, but we want the role description.
    if (backingObject->roleValue() == AccessibilityRole::Footer)
        landmarkDescription = AXFooterRoleDescriptionText();

    NSMutableString *result = [NSMutableString string];
    if (backingObject->roleValue() == AccessibilityRole::HorizontalRule)
        appendStringToResult(result, AXHorizontalRuleDescriptionText());

    appendStringToResult(result, axTitle);
    appendStringToResult(result, axDescription);
    if ([self stringValueShouldBeUsedInLabel]) {
        NSString *valueLabel = backingObject->stringValue();
        valueLabel = [valueLabel stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        appendStringToResult(result, valueLabel);
    }
    appendStringToResult(result, landmarkDescription);
    appendStringToResult(result, interactiveVideoDescription);

    return [result length] ? result : nil;
}

- (AccessibilityTableCell*)tableCellParent
{
    // Find if this element is in a table cell.
    if (AXCoreObject* parent = Accessibility::findAncestor<AXCoreObject>(*self.axBackingObject, true, [] (const AXCoreObject& object) {
        return object.isTableCell();
    }))
        return static_cast<AccessibilityTableCell*>(parent);
    return nil;
}

- (AccessibilityTable*)tableParent
{
    // Find the parent table for the table cell.
    if (AXCoreObject* parent = Accessibility::findAncestor<AXCoreObject>(*self.axBackingObject, true, [] (const AXCoreObject& object) {
        return is<AccessibilityTable>(object) && downcast<AccessibilityTable>(object).isExposable();
    }))
        return static_cast<AccessibilityTable*>(parent);
    return nil;
}

- (id)accessibilityTitleElement
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AXCoreObject* titleElement = self.axBackingObject->titleUIElement();
    if (titleElement)
        return titleElement->wrapper();

    return nil;
}

// Meant to return row or column headers (or other things as the future permits).
- (NSArray *)accessibilityHeaderElements
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return nil;
    
    AccessibilityTable* table = [self tableParent];
    if (!table)
        return nil;
    
    // Get the row and column range, so we can use them to find the headers.
    auto rowRange = tableCell->rowIndexRange();
    auto columnRange = tableCell->columnIndexRange();

    auto rowHeaders = table->rowHeaders();
    auto columnHeaders = table->columnHeaders();

    NSMutableArray *headers = [NSMutableArray array];
    
    unsigned columnRangeIndex = static_cast<unsigned>(columnRange.first);
    if (columnRangeIndex < columnHeaders.size()) {
        RefPtr<AXCoreObject> columnHeader = columnHeaders[columnRange.first];
        AccessibilityObjectWrapper* wrapper = columnHeader->wrapper();
        if (wrapper)
            [headers addObject:wrapper];
    }

    unsigned rowRangeIndex = static_cast<unsigned>(rowRange.first);
    // We should consider the cases where the row number does NOT match the index in
    // rowHeaders, the most common case is when row0/col0 does not have a header.
    for (const auto& rowHeader : rowHeaders) {
        if (!is<AccessibilityTableCell>(*rowHeader))
            break;
        auto rowHeaderRange = rowHeader->rowIndexRange();
        if (rowRangeIndex >= rowHeaderRange.first && rowRangeIndex < rowHeaderRange.first + rowHeaderRange.second) {
            if (AccessibilityObjectWrapper* wrapper = rowHeader->wrapper())
                [headers addObject:wrapper];
            break;
        }
    }

    return headers;
}

- (id)accessibilityElementForRow:(NSInteger)row andColumn:(NSInteger)column
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AccessibilityTable* table = [self tableParent];
    if (!table)
        return nil;

    auto* cell = table->cellForColumnAndRow(column, row);
    return cell ? cell->wrapper() : nil;
}

- (NSUInteger)accessibilityRowCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    return table->rowCount();
}

- (NSUInteger)accessibilityColumnCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    return table->columnCount();
}

- (NSUInteger)accessibilityARIARowCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    NSInteger rowCount = table->axRowCount();
    return rowCount > 0 ? rowCount : 0;
}

- (NSUInteger)accessibilityARIAColumnCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    NSInteger colCount = table->axColumnCount();
    return colCount > 0 ? colCount : 0;
}

- (NSUInteger)accessibilityARIARowIndex
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSNotFound;
    
    NSInteger rowIndex = tableCell->axRowIndex();
    return rowIndex > 0 ? rowIndex : NSNotFound;
}

- (NSUInteger)accessibilityARIAColumnIndex
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSNotFound;
    
    NSInteger columnIndex = tableCell->axColumnIndex();
    return columnIndex > 0 ? columnIndex : NSNotFound;
}

- (NSRange)accessibilityRowRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    if (self.axBackingObject->isRadioButton()) {
        auto radioButtonSiblings = self.axBackingObject->linkedObjects();
        if (radioButtonSiblings.size() <= 1)
            return NSMakeRange(NSNotFound, 0);

        return NSMakeRange(radioButtonSiblings.find(self.axBackingObject), radioButtonSiblings.size());
    }

    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSMakeRange(NSNotFound, 0);

    auto rowRange = tableCell->rowIndexRange();
    return NSMakeRange(rowRange.first, rowRange.second);
}

- (NSRange)accessibilityColumnRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSMakeRange(NSNotFound, 0);

    auto columnRange = tableCell->columnIndexRange();
    return NSMakeRange(columnRange.first, columnRange.second);
}

- (NSUInteger)accessibilityBlockquoteLevel
{
    if (![self _prepareAccessibilityCall])
        return 0;
    return self.axBackingObject->blockquoteLevel();
}

- (NSString *)accessibilityDatetimeValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (auto* parent = Accessibility::findAncestor<AXCoreObject>(*self.axBackingObject, true, [] (const AXCoreObject& object) {
        return object.supportsDatetimeAttribute();
    }))
        return parent->datetimeAttributeValue();

    return nil;
}

- (NSString *)accessibilityPlaceholderValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->placeholderValue();
}

- (NSString *)accessibilityColorStringValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (self.axBackingObject->isColorWell()) {
        auto color = convertColor<SRGBA<float>>(self.axBackingObject->colorValue()).resolved();
        return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", color.red, color.green, color.blue];
    }

    return nil;
}

- (NSString *)accessibilityValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // check if the value was overridden
    NSString *value = [super accessibilityValue];
    if (value)
        return value;

    Ref<AccessibilityObject> backingObject = *self.axBackingObject;
    if (backingObject->supportsCheckedState()) {
        switch (backingObject->checkboxOrRadioValue()) {
        case AccessibilityButtonState::Off:
            return [NSString stringWithFormat:@"%d", 0];
        case AccessibilityButtonState::On:
            return [NSString stringWithFormat:@"%d", 1];
        case AccessibilityButtonState::Mixed:
            return [NSString stringWithFormat:@"%d", 2];
        }
        ASSERT_NOT_REACHED();
        return [NSString stringWithFormat:@"%d", 0];
    }

    if (backingObject->isButton() && backingObject->isPressed())
        return [NSString stringWithFormat:@"%d", 1];

    // If self has the header trait, value should be the heading level.
    if (self.accessibilityTraits & self._axHeaderTrait) {
        auto* heading = Accessibility::findAncestor(backingObject.get(), true, [] (const auto& ancestor) {
            return ancestor.roleValue() == AccessibilityRole::Heading;
        });
        ASSERT(heading);

        if (heading)
            return [NSString stringWithFormat:@"%d", heading->headingLevel()];
    }

    // rdar://8131388 WebKit should expose the same info as UIKit for its password fields.
    if (backingObject->isPasswordField() && ![self _accessibilityIsStrongPasswordField]) {
        int passwordLength = backingObject->accessibilityPasswordFieldLength();
        NSMutableString* string = [NSMutableString string];
        for (int k = 0; k < passwordLength; ++k)
            [string appendString:@"•"];
        return string;
    }

    // A text control should return its text data as the axValue (per iPhone AX API).
    if (![self stringValueShouldBeUsedInLabel])
        return backingObject->stringValue();

    if (backingObject->isRangeControl()) {
        // Prefer a valueDescription if provided by the author (through aria-valuetext).
        String valueDescription = backingObject->valueDescription();
        if (!valueDescription.isEmpty())
            return valueDescription;

        return [NSString stringWithFormat:@"%.2f", backingObject->valueForRange()];
    }

    if (is<AccessibilityAttachment>(backingObject.get()) && downcast<AccessibilityAttachment>(backingObject.get()).hasProgress())
        return [NSString stringWithFormat:@"%.2f", backingObject->valueForRange()];

    return nil;
}

- (BOOL)accessibilityIsIndeterminate
{
    if (![self _prepareAccessibilityCall])
        return NO;
    return self.axBackingObject->isIndeterminate();
}

- (BOOL)accessibilityIsAttachmentElement
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return is<AccessibilityAttachment>(self.axBackingObject);
}

- (BOOL)accessibilityIsComboBox
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->roleValue() == AccessibilityRole::ComboBox;
}

- (NSString *)accessibilityHint
{
    if (![self _prepareAccessibilityCall])
        return nil;

    NSMutableString *result = [NSMutableString string];
    appendStringToResult(result, [self baseAccessibilityHelpText]);
    
    if ([self accessibilityIsShowingValidationMessage])
        appendStringToResult(result, self.axBackingObject->validationMessage());
    
    return result;
}

- (NSURL *)accessibilityURL
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    URL url = self.axBackingObject->url();
    if (url.isNull())
        return nil;
    return (NSURL*)url;
}

- (CGPoint)_accessibilityConvertPointToViewSpace:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return point;
    
    auto floatPoint = FloatPoint(point);
    auto floatRect = FloatRect(floatPoint, FloatSize());
    return [self convertRectToSpace:floatRect space:AccessibilityConversionSpace::Screen].origin;
}

- (BOOL)_accessibilityScrollToVisible
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    self.axBackingObject->scrollToMakeVisible();
    return YES;
}


- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
    if (![self _prepareAccessibilityCall])
        return NO;

    AccessibilityObject::ScrollByPageDirection scrollDirection;
    switch (direction) {
    case UIAccessibilityScrollDirectionRight:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Right;
        break;
    case UIAccessibilityScrollDirectionLeft:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Left;
        break;
    case UIAccessibilityScrollDirectionUp:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Up;
        break;
    case UIAccessibilityScrollDirectionDown:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Down;
        break;
    default:
        return NO;
    }

    BOOL result = self.axBackingObject->scrollByPage(scrollDirection);

    if (result) {
        auto notificationName = AXObjectCache::notificationPlatformName(AXObjectCache::AXNotification::AXPageScrolled).createNSString();
        [self postNotification:notificationName.get()];

        CGPoint scrollPos = [self _accessibilityScrollPosition];
        NSString *testString = [NSString stringWithFormat:@"AXScroll [position: %.2f %.2f]", scrollPos.x, scrollPos.y];
        [self accessibilityPostedNotification:notificationName.get() userInfo:@{ @"status" : testString }];
    }

    // This means that this object handled the scroll and no other ancestor should attempt scrolling.
    return result;
}

- (CGRect)_accessibilityRelativeFrame
{
    auto rect = FloatRect(snappedIntRect(self.axBackingObject->elementRect()));
    return [self convertRectToSpace:rect space:AccessibilityConversionSpace::Page];
}

// Used by UIKit accessibility bundle to help determine distance during a hit-test.
- (CGRect)accessibilityElementRect
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    LayoutRect rect = self.axBackingObject->elementRect();
    return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

- (CGRect)accessibilityVisibleContentRect
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    return [self convertRectToSpace:self.axBackingObject->unobscuredContentRect() space:AccessibilityConversionSpace::Screen];
}

// The "center point" is where VoiceOver will "press" an object. This may not be the actual
// center of the accessibilityFrame
- (CGPoint)accessibilityActivationPoint
{
    if (![self _prepareAccessibilityCall])
        return CGPointZero;

    IntPoint point = self.axBackingObject->clickPoint();
    return [self _accessibilityConvertPointToViewSpace:point];
}

- (CGRect)accessibilityFrame
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    auto rect = FloatRect(snappedIntRect(self.axBackingObject->elementRect()));
    return [self convertRectToSpace:rect space:AccessibilityConversionSpace::Screen];
}

// Checks whether a link contains only static text and images (and has been divided unnaturally by <spans> and other nefarious mechanisms).
- (BOOL)containsUnnaturallySegmentedChildren
{
    if (!self.axBackingObject)
        return NO;
    
    AccessibilityRole role = self.axBackingObject->roleValue();
    if (role != AccessibilityRole::Link && role != AccessibilityRole::WebCoreLink)
        return NO;
    
    const auto& children = self.axBackingObject->children();
    unsigned childrenSize = children.size();

    // If there's only one child, then it doesn't have segmented children. 
    if (childrenSize == 1)
        return NO;
    
    for (unsigned i = 0; i < childrenSize; ++i) {
        AccessibilityRole role = children[i]->roleValue();
        if (role != AccessibilityRole::StaticText && role != AccessibilityRole::Image && role != AccessibilityRole::Group && role != AccessibilityRole::TextGroup)
            return NO;
    }
    
    return YES;
}

- (id)accessibilityContainer
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AXAttributeCacheEnabler enableCache(self.axBackingObject->axObjectCache());
    
    // As long as there's a parent wrapper, that's the correct chain to climb.
    AXCoreObject* parent = self.axBackingObject->parentObjectUnignored();
    if (parent)
        return parent->wrapper();

    // Mock objects can have their parents detached but still exist in the cache.
    if (self.axBackingObject->isDetachedFromParent())
        return nil;

    // The only object without a parent wrapper at this point should be a scroll view.
    ASSERT(self.axBackingObject->isScrollView());

    // Verify this is the top document. If not, we might need to go through the platform widget.
    FrameView* frameView = self.axBackingObject->documentFrameView();
    Document* document = self.axBackingObject->document();
    if (document && frameView && document != &document->topDocument())
        return frameView->platformWidget();
    
    // The top scroll view's parent is the web document view.
    return [self _accessibilityWebDocumentView];
}

- (id)accessibilityFocusedUIElement
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto* focus = self.axBackingObject->focusedUIElement();
    return focus ? focus->wrapper() : nil;
}

- (id)_accessibilityWebDocumentView
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // This method performs the crucial task of connecting to the UIWebDocumentView.
    // This is needed to correctly calculate the screen position of the AX object.
    static Class webViewClass = nil;
    if (!webViewClass)
        webViewClass = NSClassFromString(@"WebView");
    if (!webViewClass)
        return nil;

    auto* frameView = self.axBackingObject->documentFrameView();
    if (!frameView)
        return nil;

    // If this is the top level frame, the UIWebDocumentView should be returned.
    id parentView = frameView->documentView();
    while (parentView && ![parentView isKindOfClass:webViewClass])
        parentView = [parentView superview];

    // The parentView should have an accessibilityContainer, if the UIKit accessibility bundle was loaded.
    // The exception is DRT, which tests accessibility without the entire system turning accessibility on. Hence,
    // this check should be valid for everything except DRT.
    ASSERT([parentView accessibilityContainer] || IOSApplication::isDumpRenderTree());

    return [parentView accessibilityContainer];
}

- (NSArray *)_accessibilityNextElementsWithCount:(UInt32)count
{    
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [[self _accessibilityWebDocumentView] _accessibilityNextElementsWithCount:count];
}

- (NSArray *)_accessibilityPreviousElementsWithCount:(UInt32)count
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [[self _accessibilityWebDocumentView] _accessibilityPreviousElementsWithCount:count];
}

- (NSDictionary<NSString *, id> *)_accessibilityResolvedEditingStyles
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self baseAccessibilityResolvedEditingStyles];
}

- (BOOL)accessibilityCanSetValue
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->canSetValueAttribute();
}

- (NSString *)_accessibilityPhotoDescription
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return self.axBackingObject->embeddedImageDescription();
}

- (NSArray *)accessibilityImageOverlayElements
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto imageOverlayElements = self.axBackingObject->imageOverlayElements();
    return imageOverlayElements ? accessibleElementsForObjects(*imageOverlayElements) : nil;
}

- (NSString *)accessibilityLinkRelationshipType
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return self.axBackingObject->linkRelValue();
}

- (BOOL)accessibilityRequired
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isRequired();
}

- (NSArray *)accessibilityFlowToElements
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return createNSArray(self.axBackingObject->flowToObjects(), [] (auto& child) -> id {
        auto wrapper = child->wrapper();
        ASSERT(wrapper);

        if (child->isAttachment()) {
            if (auto attachmentView = wrapper.attachmentView)
                return attachmentView;
        }

        return wrapper;
    }).autorelease();
}

static NSArray *accessibleElementsForObjects(const AXCoreObject::AccessibilityChildrenVector& objects)
{
    AXCoreObject::AccessibilityChildrenVector accessibleElements;
    for (const auto& object : objects) {
        if (!object)
            continue;

        Accessibility::enumerateDescendants<AXCoreObject>(*object, true, [&accessibleElements] (AXCoreObject& descendant) {
            if (descendant.wrapper().isAccessibilityElement)
                accessibleElements.append(&descendant);
        });
    }

    return makeNSArray(accessibleElements);
}

- (NSArray *)accessibilityDetailsElements
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return accessibleElementsForObjects(self.axBackingObject->detailedByObjects());
}

- (NSArray *)accessibilityErrorMessageElements
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return accessibleElementsForObjects(self.axBackingObject->errorMessageObjects());
}

- (id)accessibilityLinkedElement
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // If this static text inside of a link, it should use its parent's linked element.
    auto* backingObject = self.axBackingObject;
    if (backingObject->roleValue() == AccessibilityRole::StaticText && backingObject->parentObjectUnignored()->isLink())
        backingObject = backingObject->parentObjectUnignored();

    auto linkedObjects = backingObject->linkedObjects();
    if (linkedObjects.isEmpty() || !linkedObjects[0])
        return nil;

    // AXCoreObject::linkedObject may return an object that is exposed in other platforms but not on iOS, i.e., grouping or structure elements like <div> or <p>.
    // Thus find the next accessible object that is exposed on iOS.
    auto linkedObject = firstAccessibleObjectFromNode(linkedObjects[0]->node(), [] (const auto& accessible) {
        return accessible.wrapper().isAccessibilityElement;
    });
    return linkedObject ? linkedObject->wrapper() : nil;
}

- (BOOL)isAttachment
{
    if (!self.axBackingObject)
        return NO;
    
    return self.axBackingObject->isAttachment();
}

- (NSString *)accessibilityTextualContext
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (self.axBackingObject->node() && self.axBackingObject->node()->hasTagName(codeTag))
        return UIAccessibilityTextualContextSourceCode;
    
    return nil;
}

- (BOOL)accessibilityPerformEscape
{
    if (![self _prepareAccessibilityCall])
        return NO;
    return self.axBackingObject->performDismissAction();
}

- (BOOL)_accessibilityActivate
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->press();
}

- (id)attachmentView
{
    if (![self _prepareAccessibilityCall])
        return nil;

    ASSERT([self isAttachment]);
    Widget* widget = self.axBackingObject->widgetForAttachmentView();
    if (!widget)
        return nil;
    return widget->platformWidget();    
}

static RenderObject* rendererForView(WAKView* view)
{
    if (![view conformsToProtocol:@protocol(WebCoreFrameView)])
        return nil;
    
    WAKView<WebCoreFrameView>* frameView = (WAKView<WebCoreFrameView>*)view;
    Frame* frame = [frameView _web_frame];
    if (!frame)
        return nil;
    
    Node* node = frame->document()->ownerElement();
    if (!node)
        return nil;
    
    return node->renderer();
}

- (id)_accessibilityParentForSubview:(id)subview
{   
    RenderObject* renderer = rendererForView(subview);
    if (!renderer)
        return nil;
    
    AccessibilityObject* obj = renderer->document().axObjectCache()->getOrCreate(renderer);
    if (obj)
        return obj->parentObjectUnignored()->wrapper();
    return nil;
}

- (void)postNotification:(NSString *)notificationName
{
    // The UIKit accessibility wrapper will override and post appropriate notification.
}

// These will be used by the UIKit wrapper to calculate an appropriate description of scroll status.
- (CGPoint)_accessibilityScrollPosition
{
    if (![self _prepareAccessibilityCall])
        return CGPointZero;
    
    return self.axBackingObject->scrollPosition();
}

- (CGSize)_accessibilityScrollSize
{
    if (![self _prepareAccessibilityCall])
        return CGSizeZero;
    
    return self.axBackingObject->scrollContentsSize();
}

- (CGRect)_accessibilityScrollVisibleRect
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    return self.axBackingObject->scrollVisibleContentRect();
}

- (AXCoreObject*)detailParentForSummaryObject:(AccessibilityObject*)object
{
    // Use this to check if an object is the child of a summary object.
    // And return the summary's parent, which is the expandable details object.
    if (const AccessibilityObject* summary = Accessibility::findAncestor<AccessibilityObject>(*object, true, [] (const AccessibilityObject& object) {
        return object.hasTagName(summaryTag);
    }))
        return summary->parentObject();
    return nil;
}

- (AXCoreObject*)detailParentForObject:(AccessibilityObject*)object
{
    // Use this to check if an object is inside a details object.
    if (AccessibilityObject* details = Accessibility::findAncestor<AccessibilityObject>(*object, true, [] (const AccessibilityObject& object) {
        return object.hasTagName(detailsTag);
    }))
        return details;
    return nil;
}

- (AXCoreObject*)treeItemParentForObject:(AXCoreObject*)object
{
    // Use this to check if an object is inside a treeitem object.
    if (AXCoreObject* parent = Accessibility::findAncestor<AXCoreObject>(*object, true, [] (const AXCoreObject& object) {
        return object.roleValue() == AccessibilityRole::TreeItem;
    }))
        return parent;
    return nil;
}

- (NSArray<WebAccessibilityObjectWrapper *> *)accessibilityFindMatchingObjects:(NSDictionary *)parameters
{
    AccessibilitySearchCriteria criteria = accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(parameters);
    AccessibilityObject::AccessibilityChildrenVector results;
    self.axBackingObject->findMatchingObjects(&criteria, results);
    return makeNSArray(results);
}

- (void)accessibilityModifySelection:(TextGranularity)granularity increase:(BOOL)increase
{
    if (![self _prepareAccessibilityCall])
        return;
    
    FrameSelection& frameSelection = self.axBackingObject->document()->frame()->selection();
    VisibleSelection selection = self.axBackingObject->selection();
    VisiblePositionRange range = self.axBackingObject->visiblePositionRange();
    
    // Before a selection with length exists, the cursor position needs to move to the right starting place.
    // That should be the beginning of this element (range.start). However, if the cursor is already within the 
    // range of this element (the cursor is represented by selection), then the cursor does not need to move.
    if (frameSelection.isNone() && (selection.visibleStart() < range.start || selection.visibleEnd() > range.end))
        frameSelection.moveTo(range.start, UserTriggered);
    
    frameSelection.modify(FrameSelection::AlterationExtend, (increase) ? SelectionDirection::Right : SelectionDirection::Left, granularity, UserTriggered);
}

- (void)accessibilityIncreaseSelection:(TextGranularity)granularity
{
    [self accessibilityModifySelection:granularity increase:YES];
}

- (void)_accessibilitySetFocus:(BOOL)focus
{
    if (auto* backingObject = self.axBackingObject)
        backingObject->setFocused(focus);
}

- (BOOL)_accessibilityIsFocusedForTesting
{
    if (auto* backingObject = self.axBackingObject)
        return backingObject->isFocused();
    return false;
}

- (void)accessibilityDecreaseSelection:(TextGranularity)granularity
{
    [self accessibilityModifySelection:granularity increase:NO];
}

- (void)accessibilityMoveSelectionToMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return;
    
    VisiblePosition visiblePosition = [marker visiblePosition];
    if (visiblePosition.isNull())
        return;

    FrameSelection& frameSelection = self.axBackingObject->document()->frame()->selection();
    frameSelection.moveTo(visiblePosition, UserTriggered);
}

- (void)accessibilityIncrement
{
    if (![self _prepareAccessibilityCall])
        return;

    self.axBackingObject->increment();
}

- (void)accessibilityDecrement
{
    if (![self _prepareAccessibilityCall])
        return;

    self.axBackingObject->decrement();
}

#pragma mark Accessibility Text Marker Handlers

- (void)_accessibilitySetValue:(NSString *)string
{
    if (![self _prepareAccessibilityCall])
        return;
    self.axBackingObject->setValue(string);
}

- (NSString *)stringForTextMarkers:(NSArray *)markers
{
    if (![self _prepareAccessibilityCall])
        return nil;
    auto range = [self rangeForTextMarkers:markers];
    if (!range)
        return nil;
    return self.axBackingObject->stringForRange(*range);
}

// This method is intended to return an array of strings and accessibility elements that
// represent the objects on one line of rendered web content. The array of markers sent
// in should be ordered and contain only a start and end marker.
- (NSArray *)arrayOfTextForTextMarkers:(NSArray *)markers attributed:(BOOL)attributed
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if ([markers count] != 2)
        return nil;

    WebAccessibilityTextMarker* startMarker = [markers objectAtIndex:0];
    WebAccessibilityTextMarker* endMarker = [markers objectAtIndex:1];
    if (![startMarker isKindOfClass:[WebAccessibilityTextMarker class]] || ![endMarker isKindOfClass:[WebAccessibilityTextMarker class]])
        return nil;

    auto range = makeSimpleRange([startMarker visiblePosition], [endMarker visiblePosition]);
    return range ? [self contentForSimpleRange:*range attributed:attributed] : nil;
}


// This method is intended to take a text marker representing a VisiblePosition and convert it
// into a normalized location within the document.
- (NSInteger)positionForTextMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;

    if (!marker)
        return NSNotFound;    

    if (AXObjectCache* cache = self.axBackingObject->axObjectCache()) {
        CharacterOffset characterOffset = [marker characterOffset];
        // Create a collapsed range from the CharacterOffset object.
        auto range = cache->rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
        if (!range)
            return NSNotFound;
        return makeNSRange(range).location;
    }
    return NSNotFound;
}

- (NSArray *)textMarkerRange
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return [self textMarkersForRange:self.axBackingObject->elementRange()];
}

// A method to get the normalized text cursor range of an element. Used in DumpRenderTree.
- (NSRange)elementTextRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    NSArray *markers = [self textMarkerRange];
    if ([markers count] != 2)
        return NSMakeRange(NSNotFound, 0);
    
    WebAccessibilityTextMarker *startMarker = [markers objectAtIndex:0];
    WebAccessibilityTextMarker *endMarker = [markers objectAtIndex:1];
    
    NSInteger startPosition = [self positionForTextMarker:startMarker];
    NSInteger endPosition = [self positionForTextMarker:endMarker];
    
    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (AccessibilityObjectWrapper *)accessibilityObjectForTextMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    AccessibilityObject* obj = [marker accessibilityObject];
    if (!obj)
        return nil;
    
    return AccessibilityUnignoredAncestor(obj->wrapper());
}

- (NSArray *)textMarkerRangeForSelection
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    VisibleSelection selection = self.axBackingObject->selection();
    if (selection.isNone())
        return nil;
    
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;
    
    auto range = selection.toNormalizedRange();
    if (!range)
        return nil;

    CharacterOffset start = cache->startOrEndCharacterOffsetForRange(*range, true);
    CharacterOffset end = cache->startOrEndCharacterOffsetForRange(*range, false);

    WebAccessibilityTextMarker* startMarker = [WebAccessibilityTextMarker textMarkerWithCharacterOffset:start cache:cache];
    WebAccessibilityTextMarker* endMarker = [WebAccessibilityTextMarker textMarkerWithCharacterOffset:end cache:cache];
    if (!startMarker || !endMarker)
        return nil;
    
    return @[startMarker, endMarker];
}

- (WebAccessibilityTextMarker *)textMarkerForPosition:(NSInteger)position
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto range = makeDOMRange(self.axBackingObject->document(), NSMakeRange(position, 0));
    if (!range)
        return nil;

    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;
    
    CharacterOffset characterOffset = cache->startOrEndCharacterOffsetForRange(*range, true);
    return [WebAccessibilityTextMarker textMarkerWithCharacterOffset:characterOffset cache:cache];
}

- (id)_stringFromStartMarker:(WebAccessibilityTextMarker*)startMarker toEndMarker:(WebAccessibilityTextMarker*)endMarker attributed:(BOOL)attributed
{
    if (!startMarker || !endMarker)
        return nil;
    
    NSArray* array = [self arrayOfTextForTextMarkers:@[startMarker, endMarker] attributed:attributed];
    Class returnClass = attributed ? [NSMutableAttributedString class] : [NSMutableString class];
    auto returnValue = adoptNS([(NSString *)[returnClass alloc] init]);
    
    const unichar attachmentChar = NSAttachmentCharacter;
    NSInteger count = [array count];
    for (NSInteger k = 0; k < count; ++k) {
        auto object = retainPtr([array objectAtIndex:k]);

        if (attributed && [object isKindOfClass:[WebAccessibilityObjectWrapper class]])
            object = adoptNS([[NSMutableAttributedString alloc] initWithString:[NSString stringWithCharacters:&attachmentChar length:1] attributes:@{ UIAccessibilityTokenAttachment : object.get() }]);
        
        if (![object isKindOfClass:returnClass])
            continue;
        
        if (attributed)
            [(NSMutableAttributedString *)returnValue.get() appendAttributedString:object.get()];
        else
            [(NSMutableString *)returnValue.get() appendString:object.get()];
    }
    return returnValue.autorelease();
}

- (id)_stringForRange:(NSRange)range attributed:(BOOL)attributed
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    WebAccessibilityTextMarker* startMarker = [self textMarkerForPosition:range.location];
    WebAccessibilityTextMarker* endMarker = [self textMarkerForPosition:NSMaxRange(range)];
    
    // Clients don't always know the exact range, rather than force them to compute it,
    // allow clients to overshoot and use the max text marker range.
    if (!startMarker || !endMarker) {
        NSArray *markers = [self textMarkerRange];
        if ([markers count] != 2)
            return nil;
        if (!startMarker)
            startMarker = [markers objectAtIndex:0];
        if (!endMarker)
            endMarker = [markers objectAtIndex:1];
    }
    
    return [self _stringFromStartMarker:startMarker toEndMarker:endMarker attributed:attributed];
}

// A convenience method for getting the text of a NSRange.
- (NSString *)stringForRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall])
        return nil;
    auto webRange = makeDOMRange(self.axBackingObject->document(), range);
    if (!webRange)
        return nil;
    return self.axBackingObject->stringForRange(*webRange);
}

- (NSAttributedString *)attributedStringForRange:(NSRange)range
{
    return [self _stringForRange:range attributed:YES];
}

- (NSAttributedString *)attributedStringForElement
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    NSArray *markers = [self textMarkerRange];
    if ([markers count] != 2)
        return nil;
    
    return [self _stringFromStartMarker:markers.firstObject toEndMarker:markers.lastObject attributed:YES];
}

- (NSRange)_accessibilitySelectedTextRange
{
    if (![self _prepareAccessibilityCall] || !self.axBackingObject->isTextControl())
        return NSMakeRange(NSNotFound, 0);

    PlainTextRange textRange = self.axBackingObject->selectedTextRange();
    if (textRange.isNull())
        return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(textRange.start, textRange.length);
}

- (void)_accessibilitySetSelectedTextRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall])
        return;
    
    self.axBackingObject->setSelectedTextRange(PlainTextRange(range.location, range.length));
}

- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->replaceTextInRange(string, PlainTextRange(range));
}

- (BOOL)accessibilityInsertText:(NSString *)text
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->insertText(text);
}

// A convenience method for getting the accessibility objects of a NSRange. Currently used only by DRT.
- (NSArray *)elementsForRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    WebAccessibilityTextMarker* startMarker = [self textMarkerForPosition:range.location];
    WebAccessibilityTextMarker* endMarker = [self textMarkerForPosition:NSMaxRange(range)];
    if (!startMarker || !endMarker)
        return nil;
    
    NSArray* array = [self arrayOfTextForTextMarkers:@[startMarker, endMarker] attributed:NO];
    NSMutableArray* elements = [NSMutableArray array];
    for (id element in array) {
        if (![element isKindOfClass:[AccessibilityObjectWrapper class]])
            continue;
        [elements addObject:element];
    }
    return elements;
}

- (NSString *)selectionRangeString
{
    NSArray *markers = [self textMarkerRangeForSelection];
    return [self stringForTextMarkers:markers];
}

- (WebAccessibilityTextMarker *)selectedTextMarker
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    VisibleSelection selection = self.axBackingObject->selection();
    VisiblePosition position = selection.visibleStart();
    
    // if there's no selection, start at the top of the document
    if (position.isNull())
        position = startOfDocument(self.axBackingObject->document());
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:position cache:self.axBackingObject->axObjectCache()];
}

// This method is intended to return the marker at the end of the line starting at
// the marker that is passed into the method.
- (WebAccessibilityTextMarker *)lineEndMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    VisiblePosition start = [marker visiblePosition];
    VisiblePosition lineEnd = self.axBackingObject->nextLineEndPosition(start);
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:lineEnd cache:self.axBackingObject->axObjectCache()];
}

// Returns start/end markers for the line based on position
- (NSArray<WebAccessibilityTextMarker *> *)lineMarkersForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;

    auto range = self.axBackingObject->lineRangeForPosition([marker visiblePosition]);
    auto* startMarker = [WebAccessibilityTextMarker textMarkerWithVisiblePosition:range.start cache:self.axBackingObject->axObjectCache()];
    auto* endMarker = [WebAccessibilityTextMarker textMarkerWithVisiblePosition:range.end cache:self.axBackingObject->axObjectCache()];
    if (!startMarker || !endMarker)
        return nil;
    
    return @[ startMarker, endMarker ];
}

// This method is intended to return the marker at the start of the line starting at
// the marker that is passed into the method.
- (WebAccessibilityTextMarker *)lineStartMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    VisiblePosition start = [marker visiblePosition];
    VisiblePosition lineStart = self.axBackingObject->previousLineStartPosition(start);
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:lineStart cache:self.axBackingObject->axObjectCache()];
}

- (NSArray *)misspellingTextMarkerRange:(NSArray *)startTextMarkerRange forward:(BOOL)forward
{
    if (![self _prepareAccessibilityCall])
        return nil;
    auto startRange = [self rangeForTextMarkers:startTextMarkerRange];
    if (!startRange)
        return nil;
    auto misspellingRange = self.axBackingObject->misspellingRange(*startRange,
        forward ? AccessibilitySearchDirection::Next : AccessibilitySearchDirection::Previous);
    return [self textMarkersForRange:misspellingRange];
}

- (WebAccessibilityTextMarker *)nextMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    CharacterOffset start = [marker characterOffset];
    return [self nextMarkerForCharacterOffset:start];
}

- (WebAccessibilityTextMarker *)previousMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    CharacterOffset start = [marker characterOffset];
    return [self previousMarkerForCharacterOffset:start];
}

- (CGRect)frameForRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    auto webRange = makeDOMRange(self.axBackingObject->document(), range);
    if (!webRange)
        return CGRectZero;
    auto rect = self.axBackingObject->boundsForRange(*webRange);
    return [self convertRectToSpace:rect space:AccessibilityConversionSpace::Screen];
}

// This method is intended to return the bounds of a text marker range in screen coordinates.
- (CGRect)frameForTextMarkers:(NSArray *)array
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return CGRectZero;
    auto range = [self rangeForTextMarkers:array];
    if (!range)
        return CGRectZero;
    auto rect = self.axBackingObject->boundsForRange(*range);
    return [self convertRectToSpace:rect space:AccessibilityConversionSpace::Screen];
}

- (std::optional<SimpleRange>)rangeFromMarkers:(NSArray *)markers withText:(NSString *)text
{
    auto originalRange = [self rangeForTextMarkers:markers];
    if (!originalRange)
        return std::nullopt;
    
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return std::nullopt;
    
    return cache->rangeMatchesTextNearRange(*originalRange, text);
}

// This is only used in the layout test.
- (NSArray *)textMarkerRangeFromMarkers:(NSArray *)markers withText:(NSString *)text
{
    return [self textMarkersForRange:[self rangeFromMarkers:markers withText:text]];
}

- (NSArray *)lineRectsForTextMarkerRange:(NSArray *)markers
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto range = [self rangeForTextMarkers:markers];
    if (!range || range->collapsed())
        return nil;

    auto rects = RenderObject::absoluteTextRects(*range);
    if (rects.isEmpty())
        return nil;

    rects.removeAllMatching([] (const auto& rect) -> bool {
        return rect.width() <= 1 || rect.height() <= 1;
    });

    return createNSArray(rects, [self] (const auto& rect) {
        return [NSValue valueWithRect:[self convertRectToSpace:FloatRect(rect) space:AccessibilityConversionSpace::Screen]];
    }).autorelease();
}

- (NSArray *)textRectsFromMarkers:(NSArray *)markers withText:(NSString *)text
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto range = [self rangeFromMarkers:markers withText:text];
    if (!range || range->collapsed())
        return nil;

    auto geometries = RenderObject::collectSelectionGeometriesWithoutUnionInteriorLines(*range);
    if (geometries.isEmpty())
        return nil;
    return createNSArray(geometries, [&] (auto& geometry) {
        return [NSValue valueWithRect:[self convertRectToSpace:FloatRect(geometry.rect()) space:AccessibilityConversionSpace::Screen]];
    }).autorelease();
}

- (WebAccessibilityTextMarker *)textMarkerForPoint:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;
    CharacterOffset characterOffset = cache->characterOffsetForPoint(IntPoint(point), self.axBackingObject);
    return [WebAccessibilityTextMarker textMarkerWithCharacterOffset:characterOffset cache:cache];
}

- (WebAccessibilityTextMarker *)nextMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForNextCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache]).autorelease();
}

- (WebAccessibilityTextMarker *)previousMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForPreviousCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache]).autorelease();
}

- (std::optional<SimpleRange>)rangeForTextMarkers:(NSArray *)textMarkers
{
    if ([textMarkers count] != 2)
        return std::nullopt;
    
    WebAccessibilityTextMarker *startMarker = [textMarkers objectAtIndex:0];
    WebAccessibilityTextMarker *endMarker = [textMarkers objectAtIndex:1];
    
    if (![startMarker isKindOfClass:[WebAccessibilityTextMarker class]] || ![endMarker isKindOfClass:[WebAccessibilityTextMarker class]])
        return std::nullopt;
    
    AXObjectCache* cache = self.axBackingObject->axObjectCache();
    if (!cache)
        return std::nullopt;
    
    CharacterOffset startCharacterOffset = [startMarker characterOffset];
    CharacterOffset endCharacterOffset = [endMarker characterOffset];
    return cache->rangeForUnorderedCharacterOffsets(startCharacterOffset, endCharacterOffset);
}

- (NSInteger)lengthForTextMarkers:(NSArray *)textMarkers
{
    if (![self _prepareAccessibilityCall])
        return 0;
    
    auto range = [self rangeForTextMarkers:textMarkers];
    if (!range)
        return 0;

    int length = AXObjectCache::lengthForRange(SimpleRange { *range });
    return length < 0 ? 0 : length;
}

- (WebAccessibilityTextMarker *)startOrEndTextMarkerForTextMarkers:(NSArray *)textMarkers isStart:(BOOL)isStart
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    auto range = [self rangeForTextMarkers:textMarkers];
    if (!range)
        return nil;
    
    return [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:isStart cache:self.axBackingObject->axObjectCache()];
}

- (NSArray *)textMarkerRangeForMarkers:(NSArray *)textMarkers
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self textMarkersForRange:[self rangeForTextMarkers:textMarkers]];
}

- (NSArray *)textMarkersForRange:(const std::optional<SimpleRange>&)range
{
    if (!range)
        return nil;
    
    WebAccessibilityTextMarker* start = [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:YES cache:self.axBackingObject->axObjectCache()];
    WebAccessibilityTextMarker* end = [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:NO cache:self.axBackingObject->axObjectCache()];
    if (!start || !end)
        return nil;
    return @[start, end];
}

- (NSString *)accessibilityExpandedTextValue
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return self.axBackingObject->expandedTextValue();
}

- (NSString *)accessibilityIdentifier
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return self.axBackingObject->identifierAttribute();
}

- (BOOL)accessibilityIsInsertion
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return Accessibility::findAncestor(*self.axBackingObject, false, [] (const auto& object) {
        return object.roleValue() == AccessibilityRole::Insertion;
    }) != nullptr;
}

- (BOOL)accessibilityIsDeletion
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return Accessibility::findAncestor(*self.axBackingObject, false, [] (const auto& object) {
        return object.roleValue() == AccessibilityRole::Deletion;
    }) != nullptr;
}

- (BOOL)accessibilityIsFirstItemInSuggestion
{
    if (![self _prepareAccessibilityCall])
        return NO;

    auto* object = self.axBackingObject;
    auto* parent = object->parentObjectUnignored();
    
    while (parent) {
        if (!parent->children().size() || parent->children()[0] != object)
            return NO;
        if (parent->roleValue() == AccessibilityRole::Suggestion)
            return YES;
        object = parent;
        parent = object->parentObjectUnignored();
    }
    return NO;
}

- (BOOL)accessibilityIsLastItemInSuggestion
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    auto* object = self.axBackingObject;
    auto* parent = object->parentObjectUnignored();
    
    while (parent) {
        if (!parent->children().size() || parent->children()[parent->children().size() - 1] != object)
            return NO;
        if (parent->roleValue() == AccessibilityRole::Suggestion)
            return YES;
        object = parent;
        parent = object->parentObjectUnignored();
    }
    return NO;
}

- (BOOL)accessibilityIsMarkAnnotation
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return ancestorWithRole(*self.axBackingObject, { AccessibilityRole::Mark }) != nullptr;
}

- (NSArray<NSString *> *)accessibilitySpeechHint
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return [self baseAccessibilitySpeechHint];
}

- (BOOL)accessibilityARIAIsBusy
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->isBusy();
}

- (NSString *)accessibilityARIALiveRegionStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->liveRegionStatus();
}

- (NSString *)accessibilityARIARelevantStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return self.axBackingObject->liveRegionRelevant();
}

- (BOOL)accessibilityARIALiveRegionIsAtomic
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->liveRegionAtomic();
}

- (BOOL)accessibilitySupportsARIAPressed
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->supportsPressed();
}

- (BOOL)accessibilityIsPressed
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->isPressed();
}

- (BOOL)accessibilitySupportsARIAExpanded
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    // Since details element is ignored on iOS, we should expose the expanded status on its
    // summary's accessible children.
    if (auto* detailParent = [self detailParentForSummaryObject:self.axBackingObject])
        return detailParent->supportsExpanded();
    
    if (AXCoreObject* treeItemParent = [self treeItemParentForObject:self.axBackingObject])
        return treeItemParent->supportsExpanded();
    
    return self.axBackingObject->supportsExpanded();
}

- (BOOL)accessibilityIsExpanded
{
    if (![self _prepareAccessibilityCall])
        return NO;

    // Since details element is ignored on iOS, we should expose the expanded status on its
    // summary's accessible children.
    if (auto* detailParent = [self detailParentForSummaryObject:self.axBackingObject])
        return detailParent->isExpanded();
    
    if (AXCoreObject* treeItemParent = [self treeItemParentForObject:self.axBackingObject])
        return treeItemParent->isExpanded();
    
    return self.axBackingObject->isExpanded();
}

- (BOOL)accessibilityIsShowingValidationMessage
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return self.axBackingObject->isShowingValidationMessage();
}

- (NSString *)accessibilityInvalidStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return self.axBackingObject->invalidStatus();
}

- (NSString *)accessibilityCurrentState
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->currentValue();
}

- (NSString *)accessibilitySortDirection
{
    if (![self _prepareAccessibilityCall])
        return nil;

    switch (self.axBackingObject->sortDirection()) {
    case AccessibilitySortDirection::Ascending:
        return @"AXAscendingSortDirection";
    case AccessibilitySortDirection::Descending:
        return @"AXDescendingSortDirection";
    default:
        return @"AXUnknownSortDirection";
    }
}

- (WebAccessibilityObjectWrapper *)accessibilityMathRootIndexObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathRootIndexObject() ? self.axBackingObject->mathRootIndexObject()->wrapper() : 0;
}

- (NSArray *)accessibilityMathRadicand
{
    if (![self _prepareAccessibilityCall])
        return nil;

    auto radicand = self.axBackingObject->mathRadicand();
    return radicand ? makeNSArray(*radicand) : nil;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathNumeratorObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathNumeratorObject() ? self.axBackingObject->mathNumeratorObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathDenominatorObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathDenominatorObject() ? self.axBackingObject->mathDenominatorObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathBaseObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathBaseObject() ? self.axBackingObject->mathBaseObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathSubscriptObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathSubscriptObject() ? self.axBackingObject->mathSubscriptObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathSuperscriptObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathSuperscriptObject() ? self.axBackingObject->mathSuperscriptObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathUnderObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathUnderObject() ? self.axBackingObject->mathUnderObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathOverObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathOverObject() ? self.axBackingObject->mathOverObject()->wrapper() : 0;
}

- (NSString *)accessibilityPlatformMathSubscriptKey
{
    return @"AXMSubscriptObject";
}

- (NSString *)accessibilityPlatformMathSuperscriptKey
{
    return @"AXMSuperscriptObject";
}

- (NSArray *)accessibilityMathPostscripts
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self accessibilityMathPostscriptPairs];
}

- (NSArray *)accessibilityMathPrescripts
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self accessibilityMathPrescriptPairs];
}

- (NSString *)accessibilityMathFencedOpenString
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathFencedOpenString();
}

- (NSString *)accessibilityMathFencedCloseString
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return self.axBackingObject->mathFencedCloseString();
}

- (BOOL)accessibilityIsMathTopObject
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return self.axBackingObject->roleValue() == AccessibilityRole::DocumentMath;
}

- (NSInteger)accessibilityMathLineThickness
{
    if (![self _prepareAccessibilityCall])
        return 0;

    return self.axBackingObject->mathLineThickness();
}

- (NSString *)accessibilityMathType
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (self.axBackingObject->roleValue() == AccessibilityRole::MathElement) {
        if (self.axBackingObject->isMathFraction())
            return @"AXMathFraction";
        if (self.axBackingObject->isMathFenced())
            return @"AXMathFenced";
        if (self.axBackingObject->isMathSubscriptSuperscript())
            return @"AXMathSubscriptSuperscript";
        if (self.axBackingObject->isMathRow())
            return @"AXMathRow";
        if (self.axBackingObject->isMathUnderOver())
            return @"AXMathUnderOver";
        if (self.axBackingObject->isMathSquareRoot())
            return @"AXMathSquareRoot";
        if (self.axBackingObject->isMathRoot())
            return @"AXMathRoot";
        if (self.axBackingObject->isMathText())
            return @"AXMathText";
        if (self.axBackingObject->isMathNumber())
            return @"AXMathNumber";
        if (self.axBackingObject->isMathIdentifier())
            return @"AXMathIdentifier";
        if (self.axBackingObject->isMathTable())
            return @"AXMathTable";
        if (self.axBackingObject->isMathTableRow())
            return @"AXMathTableRow";
        if (self.axBackingObject->isMathTableCell())
            return @"AXMathTableCell";
        if (self.axBackingObject->isMathFenceOperator())
            return @"AXMathFenceOperator";
        if (self.axBackingObject->isMathSeparatorOperator())
            return @"AXMathSeparatorOperator";
        if (self.axBackingObject->isMathOperator())
            return @"AXMathOperator";
        if (self.axBackingObject->isMathMultiscript())
            return @"AXMathMultiscript";
    }
    
    return nil;
}

- (CGPoint)accessibilityClickPoint
{
    return self.axBackingObject->clickPoint();
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@: %@", [self class], [self accessibilityLabel]];
}

@end

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)
