/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Trolltech ASA
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Frame.h"

#import "AXObjectCache.h"
#import "BeforeUnloadEvent.h"
#import "BlockExceptions.h"
#import "CSSHelper.h"
#import "Cache.h"
#import "Chrome.h"
#import "ClipboardEvent.h"
#import "ClipboardMac.h"
#import "ColorMac.h"
#import "Cursor.h"
#import "DOMInternal.h"
#import "DocumentLoader.h"
#import "EditCommand.h"
#import "EditorClient.h"
#import "Event.h"
#import "EventNames.h"
#import "FloatRect.h"
#import "FoundationExtras.h"
#import "FrameLoadRequest.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "FrameLoaderTypes.h"
#import "FramePrivate.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLDocument.h"
#import "HTMLFormElement.h"
#import "HTMLGenericFormElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLTableCellElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "KeyboardEvent.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "Page.h"
#import "PlatformKeyboardEvent.h"
#import "PlatformScrollBar.h"
#import "PlatformWheelEvent.h"
#import "Plugin.h"
#import "RegularExpression.h"
#import "RenderImage.h"
#import "RenderListItem.h"
#import "RenderPart.h"
#import "RenderTableCell.h"
#import "RenderTheme.h"
#import "RenderView.h"
#import "ResourceHandle.h"
#import "Settings.h"
#import "SimpleFontData.h"
#import "SystemTime.h"
#import "TextResourceDecoder.h"
#import "UserStyleSheetLoader.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreViewFactory.h"
#import "WebDashboardRegion.h"
#import "WebScriptObjectPrivate.h"
#import "kjs_proxy.h"
#import "kjs_window.h"
#import "visible_units.h"
#import <Carbon/Carbon.h>
#import <JavaScriptCore/NP_jsobject.h>
#import <JavaScriptCore/npruntime_impl.h>

#undef _webcore_TIMING

@interface NSObject (WebPlugIn)
- (id)objectForWebScript;
- (NPObject *)createPluginScriptableObject;
@end
 
@interface NSView (WebCoreHTMLDocumentView)
- (void)drawSingleRect:(NSRect)rect;
@end
 
using namespace std;
using namespace KJS::Bindings;

using KJS::JSLock;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

void Frame::setBridge(WebCoreFrameBridge* bridge)
{ 
    if (d->m_bridge == bridge)
        return;

    if (!bridge) {
        [d->m_bridge clearFrame];
        HardRelease(d->m_bridge);
        d->m_bridge = nil;
        return;
    }
    HardRetain(bridge);
    HardRelease(d->m_bridge);
    d->m_bridge = bridge;
}

WebCoreFrameBridge* Frame::bridge() const
{
    return d->m_bridge;
}

// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
RegularExpression* regExpForLabels(NSArray* labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions


    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NSMutableArray* regExpLabels = nil;
    static Vector<RegularExpression*> regExps;
    static RegularExpression wordRegExp = RegularExpression("\\w");

    RegularExpression* result;
    if (!regExpLabels)
        regExpLabels = [[NSMutableArray alloc] initWithCapacity:regExpCacheSize];
    CFIndex cacheHit = [regExpLabels indexOfObject:labels];
    if (cacheHit != NSNotFound)
        result = regExps.at(cacheHit);
    else {
        DeprecatedString pattern("(");
        unsigned int numLabels = [labels count];
        unsigned int i;
        for (i = 0; i < numLabels; i++) {
            DeprecatedString label = DeprecatedString::fromNSString((NSString*)[labels objectAtIndex:i]);

            bool startsWithWordChar = false;
            bool endsWithWordChar = false;
            if (label.length() != 0) {
                startsWithWordChar = wordRegExp.search(label.at(0)) >= 0;
                endsWithWordChar = wordRegExp.search(label.at(label.length() - 1)) >= 0;
            }
            
            if (i != 0)
                pattern.append("|");
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages
            // such as Japanese.
            if (startsWithWordChar) {
                pattern.append("\\b");
            }
            pattern.append(label);
            if (endsWithWordChar) {
                pattern.append("\\b");
            }
        }
        pattern.append(")");
        result = new RegularExpression(pattern, false);
    }

    // add regexp to the cache, making sure it is at the front for LRU ordering
    if (cacheHit != 0) {
        if (cacheHit != NSNotFound) {
            // remove from old spot
            [regExpLabels removeObjectAtIndex:cacheHit];
            regExps.remove(cacheHit);
        }
        // add to start
        [regExpLabels insertObject:labels atIndex:0];
        regExps.insert(0, result);
        // trim if too big
        if ([regExpLabels count] > regExpCacheSize) {
            [regExpLabels removeObjectAtIndex:regExpCacheSize];
            RegularExpression* last = regExps.last();
            regExps.removeLast();
            delete last;
        }
    }
    return result;
}

NSString* Frame::searchForNSLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement* aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node* n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        DeprecatedString nodeString = n->nodeValue().deprecatedString();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.mid(pos, regExp->matchedLength()).getNSString();
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return nil;
}

NSString* Frame::searchForLabelsBeforeElement(NSArray* labels, Element* element)
{
    RegularExpression* regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement* startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node* n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElement*>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString* result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (result && [result length] > 0)
                return result;
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            DeprecatedString nodeString = n->nodeValue().deprecatedString();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.mid(pos, regExp->matchedLength()).getNSString();

            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
        NSString* result = searchForLabelsAboveCell(regExp, startingTableCell);
        if (result && [result length] > 0)
            return result;
    }
    
    return nil;
}

NSString* Frame::matchLabelsAgainstElement(NSArray* labels, Element* element)
{
    DeprecatedString name = element->getAttribute(nameAttr).deprecatedString();
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    name.replace(RegularExpression("\\d"), " ");
    name.replace('_', ' ');
    
    RegularExpression* regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos+1;
        }
    } while (pos != -1);

    if (bestPos != -1)
        return name.mid(bestPos, bestLength).getNSString();
    return nil;
}

NSImage* Frame::imageFromRect(NSRect rect) const
{
    NSView* view = d->m_view->getDocumentView();
    if (!view)
        return nil;
    if (![view respondsToSelector:@selector(drawSingleRect:)])
        return nil;
    
    NSImage* resultImage;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSRect bounds = [view bounds];
    
    // Round image rect size in window coordinate space to avoid pixel cracks at HiDPI (4622794)
    rect = [view convertRect:rect toView:nil];
    rect.size.height = roundf(rect.size.height);
    rect.size.width = roundf(rect.size.width);
    rect = [view convertRect:rect fromView:nil];
    
    resultImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];

    if (rect.size.width != 0 && rect.size.height != 0) {
        [resultImage setFlipped:YES];
        [resultImage lockFocus];
        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, bounds.origin.x - rect.origin.x, bounds.origin.y - rect.origin.y);

        // Note: Must not call drawRect: here, because drawRect: assumes that it's called from AppKit's
        // display machinery. It calls getRectsBeingDrawn:count:, which can only be called inside
        // when a real AppKit display is underway.
        [view drawSingleRect:rect];

        CGContextRestoreGState(context);
        [resultImage unlockFocus];
        [resultImage setFlipped:NO];
    }

    return resultImage;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    return nil;
}

NSImage* Frame::selectionImage(bool forceBlackText) const
{
    d->m_paintRestriction = forceBlackText ? PaintRestrictionSelectionOnlyBlackText : PaintRestrictionSelectionOnly;
    d->m_doc->updateLayout();
    NSImage* result = imageFromRect(selectionRect());
    d->m_paintRestriction = PaintRestrictionNone;
    return result;
}

NSImage* Frame::snapshotDragImage(Node* node, NSRect* imageRect, NSRect* elementRect) const
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;
    
    renderer->updateDragState(true);    // mark dragged nodes (so they pick up the right CSS)
    d->m_doc->updateLayout();        // forces style recalc - needed since changing the drag state might
                                        // imply new styles, plus JS could have changed other things
    IntRect topLevelRect;
    NSRect paintingRect = renderer->paintingRootRect(topLevelRect);

    d->m_elementToDraw = node;              // invoke special sub-tree drawing mode
    NSImage* result = imageFromRect(paintingRect);
    renderer->updateDragState(false);
    d->m_doc->updateLayout();
    d->m_elementToDraw = 0;

    if (elementRect)
        *elementRect = topLevelRect;
    if (imageRect)
        *imageRect = paintingRect;
    return result;
}

NSDictionary* Frame::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];

    if (style->backgroundColor().isValid() && style->backgroundColor().alpha() != 0)
        [result setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

    if (style->font().primaryFont()->getNSFont())
        [result setObject:style->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];

    if (style->color().isValid() && style->color() != Color::black)
        [result setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];

    ShadowData* shadow = style->textShadow();
    if (shadow) {
        NSShadow* s = [[NSShadow alloc] init];
        [s setShadowOffset:NSMakeSize(shadow->x, shadow->y)];
        [s setShadowBlurRadius:shadow->blur];
        [s setShadowColor:nsColor(shadow->color)];
        [result setObject:s forKey:NSShadowAttributeName];
    }

    int decoration = style->textDecorationsInEffect();
    if (decoration & LINE_THROUGH)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];

    int superscriptInt = 0;
    switch (style->verticalAlign()) {
        case BASELINE:
        case BOTTOM:
        case BASELINE_MIDDLE:
        case LENGTH:
        case MIDDLE:
        case TEXT_BOTTOM:
        case TEXT_TOP:
        case TOP:
            break;
        case SUB:
            superscriptInt = -1;
            break;
        case SUPER:
            superscriptInt = 1;
            break;
    }
    if (superscriptInt)
        [result setObject:[NSNumber numberWithInt:superscriptInt] forKey:NSSuperscriptAttributeName];

    if (decoration & UNDERLINE)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        ASSERT(ec == 0);
    }

    return result;
}

NSWritingDirection Frame::baseWritingDirectionForSelectionStart() const
{
    NSWritingDirection result = NSWritingDirectionLeftToRight;

    Position pos = selectionController()->selection().visibleStart().deepEquivalent();
    Node* node = pos.node();
    if (!node || !node->renderer() || !node->renderer()->containingBlock())
        return result;
    RenderStyle* style = node->renderer()->containingBlock()->style();
    if (!style)
        return result;
        
    switch (style->direction()) {
        case LTR:
            result = NSWritingDirectionLeftToRight;
            break;
        case RTL:
            result = NSWritingDirectionRightToLeft;
            break;
    }

    return result;
}

void Frame::issuePasteCommand()
{
    [d->m_bridge issuePasteCommand];
}

const short enableRomanKeyboardsOnly = -23;
void Frame::setUseSecureKeyboardEntry(bool enable)
{
    if (enable == IsSecureEventInputEnabled())
        return;
    if (enable) {
        EnableSecureEventInput();
#ifdef BUILDING_ON_TIGER
        KeyScript(enableRomanKeyboardsOnly);
#else
        CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
        TSMSetDocumentProperty(TSMGetActiveDocument(), kTSMDocumentEnabledInputSourcesPropertyTag, sizeof(CFArrayRef), &inputSources);
        CFRelease(inputSources);
#endif
    } else {
        DisableSecureEventInput();
#ifdef BUILDING_ON_TIGER
        KeyScript(smKeyEnableKybds);
#else
        TSMRemoveDocumentProperty(TSMGetActiveDocument(), kTSMDocumentEnabledInputSourcesPropertyTag);
#endif
    }
}

NSMutableDictionary* Frame::dashboardRegionsDictionary()
{
    Document* doc = document();
    if (!doc)
        return nil;

    const Vector<DashboardRegionValue>& regions = doc->dashboardRegions();
    size_t n = regions.size();

    // Convert the Vector<DashboardRegionValue> into a NSDictionary of WebDashboardRegions
    NSMutableDictionary* webRegions = [NSMutableDictionary dictionaryWithCapacity:n];
    for (size_t i = 0; i < n; i++) {
        const DashboardRegionValue& region = regions[i];

        if (region.type == StyleDashboardRegion::None)
            continue;
        
        NSString *label = region.label;
        WebDashboardRegionType type = WebDashboardRegionTypeNone;
        if (region.type == StyleDashboardRegion::Circle)
            type = WebDashboardRegionTypeCircle;
        else if (region.type == StyleDashboardRegion::Rectangle)
            type = WebDashboardRegionTypeRectangle;
        NSMutableArray *regionValues = [webRegions objectForKey:label];
        if (!regionValues) {
            regionValues = [[NSMutableArray alloc] initWithCapacity:1];
            [webRegions setObject:regionValues forKey:label];
            [regionValues release];
        }
        
        WebDashboardRegion *webRegion = [[WebDashboardRegion alloc] initWithRect:region.bounds clip:region.clip type:type];
        [regionValues addObject:webRegion];
        [webRegion release];
    }
    
    return webRegions;
}

void Frame::dashboardRegionsChanged()
{
    NSMutableDictionary *webRegions = dashboardRegionsDictionary();
    [d->m_bridge dashboardRegionsChanged:webRegions];
}

void Frame::willPopupMenu(NSMenu * menu)
{
    [d->m_bridge willPopupMenu:menu];
}

FloatRect Frame::customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect, Node* node)
{
    return [d->m_bridge customHighlightRect:type forLine:lineRect representedNode:node];
}

void Frame::paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line, Node* node)
{
    [d->m_bridge paintCustomHighlight:type forBox:boxRect onLine:lineRect behindText:text entireLine:line representedNode:node];
}

DragImageRef Frame::dragImageForSelection() 
{
    if (!selectionController()->isRange())
        return nil;
    return selectionImage();
}


KJS::Bindings::Instance* Frame::createScriptInstanceForWidget(WebCore::Widget* widget)
{
    NSView* aView = widget->getView();
    if (!aView)
        return 0;

    void* nativeHandle = aView;
    CreateRootObjectFunction createRootObject = RootObject::createRootObject();
    RefPtr<RootObject> rootObject = createRootObject(nativeHandle);

    if ([aView respondsToSelector:@selector(objectForWebScript)]) {
        id objectForWebScript = [aView objectForWebScript];
        if (objectForWebScript)
            return Instance::createBindingForLanguageInstance(Instance::ObjectiveCLanguage, objectForWebScript, rootObject.release());
        return 0;
    } else if ([aView respondsToSelector:@selector(createPluginScriptableObject)]) {
#if USE(NPOBJECT)
        NPObject* npObject = [aView createPluginScriptableObject];
        if (npObject) {
            Instance* instance = Instance::createBindingForLanguageInstance(Instance::CLanguage, npObject, rootObject.release());

            // -createPluginScriptableObject returns a retained NPObject.  The caller is expected to release it.
            _NPN_ReleaseObject(npObject);
            return instance;
        }
#endif
        return 0;
    }

    jobject applet;
    
    // Get a pointer to the actual Java applet instance.
    if ([d->m_bridge respondsToSelector:@selector(getAppletInView:)])
        applet = [d->m_bridge getAppletInView:aView];
    else
        applet = [d->m_bridge pollForAppletInView:aView];
    
    if (applet) {
        // Wrap the Java instance in a language neutral binding and hand
        // off ownership to the APPLET element.
        Instance* instance = Instance::createBindingForLanguageInstance(Instance::JavaLanguage, applet, rootObject.release());
        return instance;
    }
    
    return 0;
}

WebScriptObject* Frame::windowScriptObject()
{
    if (!scriptProxy()->isEnabled())
        return 0;

    if (!d->m_windowScriptObject) {
        KJS::JSLock lock;
        KJS::JSObject* win = KJS::Window::retrieveWindow(this);
        KJS::Bindings::RootObject *root = bindingRootObject();
        d->m_windowScriptObject = [WebScriptObject scriptObjectForJSObject:toRef(win) originRootObject:root rootObject:root];
    }

    return d->m_windowScriptObject.get();
}

void Frame::clearPlatformScriptObjects()
{
    if (d->m_windowScriptObject) {
        KJS::Bindings::RootObject* root = bindingRootObject();
        [d->m_windowScriptObject.get() _setOriginRootObject:root andRootObject:root];
    }
}

void Frame::setUserStyleSheetLocation(const KURL& url)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc && d->m_doc->docLoader())
        d->m_userStyleSheetLoader = new UserStyleSheetLoader(d->m_doc, url.string());
}

void Frame::setUserStyleSheet(const String& styleSheet)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc)
        d->m_doc->setUserStyleSheet(styleSheet);
}

} // namespace WebCore
