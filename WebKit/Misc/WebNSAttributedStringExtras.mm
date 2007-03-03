/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "WebNSAttributedStringExtras.h"

#import "DOMRangeInternal.h"
#import "WebDataSourcePrivate.h"
#import "WebFrame.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import <WebCore/csshelper.h>
#import <WebCore/BlockExceptions.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/FontData.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/Image.h>
#import <WebCore/InlineTextBox.h>
#import <WebCore/KURL.h>
#import <WebCore/Range.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderListItem.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderStyle.h>
#import <WebCore/RenderText.h>
#import <WebCore/Text.h>

using namespace WebCore;
using namespace HTMLNames;

struct ListItemInfo {
    unsigned start;
    unsigned end;
};

static Element* listParent(Element* item)
{
    while (!item->hasTagName(ulTag) && !item->hasTagName(olTag)) {
        item = static_cast<Element*>(item->parentNode());
        if (!item)
            break;
    }
    return item;
}

static Node* isTextFirstInListItem(Node* e)
{
    if (!e->isTextNode())
        return 0;
    Node* par = e->parentNode();
    while (par) {
        if (par->firstChild() != e)
            return 0;
        if (par->hasTagName(liTag))
            return par;
        e = par;
        par = par->parentNode();
    }
    return 0;
}

static NSFileWrapper *fileWrapperForElement(Element* e)
{
    NSFileWrapper *wrapper = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    const AtomicString& attr = e->getAttribute(srcAttr);
    if (!attr.isEmpty()) {
        NSURL *URL = KURL(e->document()->completeURL(attr.deprecatedString())).getNSURL();
        wrapper = [[kit(e->document()->frame()) dataSource] _fileWrapperForURL:URL];
    }
    if (!wrapper) {
        RenderImage* renderer = static_cast<RenderImage*>(e->renderer());
        if (renderer->cachedImage() && !renderer->cachedImage()->errorOccurred()) {
            wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:(NSData *)(renderer->cachedImage()->image()->getTIFFRepresentation())];
            [wrapper setPreferredFilename:@"image.tiff"];
            [wrapper autorelease];
        }
    }

    return wrapper;

    END_BLOCK_OBJC_EXCEPTIONS;

    return nil;
}

@implementation NSAttributedString (WebKitExtras)

- (NSAttributedString *)_web_attributedStringByStrippingAttachmentCharacters
{
    // This code was originally copied from NSTextView
    NSRange attachmentRange;
    NSString *originalString = [self string];
    static NSString *attachmentCharString = nil;
    
    if (!attachmentCharString) {
        unichar chars[2];
        if (!attachmentCharString) {
            chars[0] = NSAttachmentCharacter;
            chars[1] = 0;
            attachmentCharString = [[NSString alloc] initWithCharacters:chars length:1];
        }
    }
    
    attachmentRange = [originalString rangeOfString:attachmentCharString];
    if (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
        NSMutableAttributedString *newAttributedString = [[self mutableCopyWithZone:NULL] autorelease];
        
        while (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
            [newAttributedString replaceCharactersInRange:attachmentRange withString:@""];
            attachmentRange = [[newAttributedString string] rangeOfString:attachmentCharString];
        }
        return newAttributedString;
    }
    
    return self;
}

// FIXME: Use WebCore::TextIterator to iterate text runs.

+ (NSAttributedString *)_web_attributedStringFromRange:(Range*)range
{
    ListItemInfo info;
    ExceptionCode ec = 0; // dummy variable -- we ignore DOM exceptions
    NSMutableAttributedString *result;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (!range || !range->boundaryPointsValid())
        return nil;
    
    Node* firstNode = range->startNode();
    if (!firstNode)
        return nil;
    Node* pastEndNode = range->pastEndNode();
    
    int startOffset = range->startOffset(ec);
    int endOffset = range->endOffset(ec);
    Node* endNode = range->endContainer(ec);

    result = [[[NSMutableAttributedString alloc] init] autorelease];
    
    bool hasNewLine = true;
    bool addedSpace = true;
    NSAttributedString *pendingStyledSpace = nil;
    bool hasParagraphBreak = true;
    const Element *linkStartNode = 0;
    unsigned linkStartLocation = 0;
    Vector<Element*> listItems;
    Vector<ListItemInfo> listItemLocations;
    float maxMarkerWidth = 0;
    
    Node *currentNode = firstNode;
    
    // If the first item is the entire text of a list item, use the list item node as the start of the 
    // selection, not the text node.  The user's intent was probably to select the list.
    if (currentNode->isTextNode() && startOffset == 0) {
        Node *startListNode = isTextFirstInListItem(firstNode);
        if (startListNode){
            firstNode = startListNode;
            currentNode = firstNode;
        }
    }
    
    while (currentNode && currentNode != pastEndNode) {
        RenderObject *renderer = currentNode->renderer();
        if (renderer) {
            RenderStyle *style = renderer->style();
            NSFont *font = style->font().primaryFont()->getNSFont();
            bool needSpace = pendingStyledSpace != nil;
            if (currentNode->isTextNode()) {
                if (hasNewLine) {
                    addedSpace = true;
                    needSpace = false;
                    [pendingStyledSpace release];
                    pendingStyledSpace = nil;
                    hasNewLine = false;
                }
                DeprecatedString text;
                DeprecatedString str = currentNode->nodeValue().deprecatedString();
                int start = (currentNode == firstNode) ? startOffset : -1;
                int end = (currentNode == endNode) ? endOffset : -1;
                if (renderer->isText()) {
                    if (!style->collapseWhiteSpace()) {
                        if (needSpace && !addedSpace) {
                            if (text.isEmpty() && linkStartLocation == [result length])
                                ++linkStartLocation;
                            [result appendAttributedString:pendingStyledSpace];
                        }
                        int runStart = (start == -1) ? 0 : start;
                        int runEnd = (end == -1) ? str.length() : end;
                        text += str.mid(runStart, runEnd-runStart);
                        [pendingStyledSpace release];
                        pendingStyledSpace = nil;
                        addedSpace = u_charDirection(str[runEnd - 1].unicode()) == U_WHITE_SPACE_NEUTRAL;
                    }
                    else {
                        RenderText* textObj = static_cast<RenderText*>(renderer);
                        if (!textObj->firstTextBox() && str.length() > 0 && !addedSpace) {
                            // We have no runs, but we do have a length.  This means we must be
                            // whitespace that collapsed away at the end of a line.
                            text += ' ';
                            addedSpace = true;
                        }
                        else {
                            addedSpace = false;
                            for (InlineTextBox* box = textObj->firstTextBox(); box; box = box->nextTextBox()) {
                                int runStart = (start == -1) ? box->m_start : start;
                                int runEnd = (end == -1) ? box->m_start + box->m_len : end;
                                if (runEnd > box->m_start + box->m_len)
                                    runEnd = box->m_start + box->m_len;
                                if (runStart >= box->m_start &&
                                    runStart < box->m_start + box->m_len) {
                                    if (box == textObj->firstTextBox() && box->m_start == runStart && runStart > 0)
                                        needSpace = true; // collapsed space at the start
                                    if (needSpace && !addedSpace) {
                                        if (pendingStyledSpace != nil) {
                                            if (text.isEmpty() && linkStartLocation == [result length])
                                                ++linkStartLocation;
                                            [result appendAttributedString:pendingStyledSpace];
                                        } else
                                            text += ' ';
                                    }
                                    DeprecatedString runText = str.mid(runStart, runEnd - runStart);
                                    runText.replace('\n', ' ');
                                    text += runText;
                                    int nextRunStart = box->nextTextBox() ? box->nextTextBox()->m_start : str.length(); // collapsed space between runs or at the end
                                    needSpace = nextRunStart > runEnd;
                                    [pendingStyledSpace release];
                                    pendingStyledSpace = nil;
                                    addedSpace = u_charDirection(str[runEnd - 1].unicode()) == U_WHITE_SPACE_NEUTRAL;
                                    start = -1;
                                }
                                if (end != -1 && runEnd >= end)
                                    break;
                            }
                        }
                    }
                }
                
                text.replace('\\', renderer->backslashAsCurrencySymbol());
    
                if (text.length() > 0 || needSpace) {
                    NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
                    [attrs setObject:font forKey:NSFontAttributeName];
                    if (style && style->color().isValid() && style->color().alpha() != 0)
                        [attrs setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];
                    if (style && style->backgroundColor().isValid() && style->backgroundColor().alpha() != 0)
                        [attrs setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

                    if (text.length() > 0) {
                        hasParagraphBreak = false;
                        NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString() attributes:attrs];
                        [result appendAttributedString: partialString];                
                        [partialString release];
                    }

                    if (needSpace) {
                        [pendingStyledSpace release];
                        pendingStyledSpace = [[NSAttributedString alloc] initWithString:@" " attributes:attrs];
                    }

                    [attrs release];
                }
            } else {
                // This is our simple HTML -> ASCII transformation:
                DeprecatedString text;
                if (currentNode->hasTagName(aTag)) {
                    // Note the start of the <a> element.  We will add the NSLinkAttributeName
                    // attribute to the attributed string when navigating to the next sibling 
                    // of this node.
                    linkStartLocation = [result length];
                    linkStartNode = static_cast<Element*>(currentNode);
                } else if (currentNode->hasTagName(brTag)) {
                    text += "\n";
                    hasNewLine = true;
                } else if (currentNode->hasTagName(liTag)) {
                    DeprecatedString listText;
                    Element *itemParent = listParent(static_cast<Element*>(currentNode));
                    
                    if (!hasNewLine)
                        listText += '\n';
                    hasNewLine = true;

                    listItems.append(static_cast<Element*>(currentNode));
                    info.start = [result length];
                    info.end = 0;
                    listItemLocations.append (info);
                    
                    listText += '\t';
                    if (itemParent && renderer->isListItem()) {
                        RenderListItem* listRenderer = static_cast<RenderListItem*>(renderer);

                        maxMarkerWidth = MAX([font pointSize], maxMarkerWidth);

                        String marker = listRenderer->markerText();
                        if (!marker.isEmpty()) {
                            listText += marker.deprecatedString();
                            // Use AppKit metrics, since this will be rendered by AppKit.
                            NSString *markerNSString = marker;
                            float markerWidth = [markerNSString sizeWithAttributes:[NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName]].width;
                            maxMarkerWidth = MAX(markerWidth, maxMarkerWidth);
                        }

                        listText += ' ';
                        listText += '\t';

                        NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
                        [attrs setObject:font forKey:NSFontAttributeName];
                        if (style && style->color().isValid())
                            [attrs setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];
                        if (style && style->backgroundColor().isValid())
                            [attrs setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];

                        NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:listText.getNSString() attributes:attrs];
                        [attrs release];
                        [result appendAttributedString: partialString];                
                        [partialString release];
                    }
                } else if (currentNode->hasTagName(olTag) || currentNode->hasTagName(ulTag)) {
                    if (!hasNewLine)
                        text += "\n";
                    hasNewLine = true;
                } else if (currentNode->hasTagName(blockquoteTag)
                        || currentNode->hasTagName(ddTag)
                        || currentNode->hasTagName(divTag)
                        || currentNode->hasTagName(dlTag)
                        || currentNode->hasTagName(dtTag)
                        || currentNode->hasTagName(hrTag)
                        || currentNode->hasTagName(listingTag)
                        || currentNode->hasTagName(preTag)
                        || currentNode->hasTagName(tdTag)
                        || currentNode->hasTagName(thTag)) {
                    if (!hasNewLine)
                        text += '\n';
                    hasNewLine = true;
                } else if (currentNode->hasTagName(h1Tag)
                        || currentNode->hasTagName(h2Tag)
                        || currentNode->hasTagName(h3Tag)
                        || currentNode->hasTagName(h4Tag)
                        || currentNode->hasTagName(h5Tag)
                        || currentNode->hasTagName(h6Tag)
                        || currentNode->hasTagName(pTag)
                        || currentNode->hasTagName(trTag)) {
                    if (!hasNewLine)
                        text += '\n';
                    
                    // In certain cases, emit a paragraph break.
                    int bottomMargin = renderer->collapsedMarginBottom();
                    int fontSize = style->fontDescription().computedPixelSize();
                    if (bottomMargin * 2 >= fontSize) {
                        if (!hasParagraphBreak) {
                            text += '\n';
                            hasParagraphBreak = true;
                        }
                    }
                    
                    hasNewLine = true;
                }
                else if (currentNode->hasTagName(imgTag)) {
                    if (pendingStyledSpace != nil) {
                        if (linkStartLocation == [result length])
                            ++linkStartLocation;
                        [result appendAttributedString:pendingStyledSpace];
                        [pendingStyledSpace release];
                        pendingStyledSpace = nil;
                    }
                    NSFileWrapper *fileWrapper = fileWrapperForElement(static_cast<Element*>(currentNode));
                    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
                    NSAttributedString *iString = [NSAttributedString attributedStringWithAttachment:attachment];
                    [result appendAttributedString: iString];
                    [attachment release];
                }

                NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString()];
                [result appendAttributedString: partialString];
                [partialString release];
            }
        }

        Node *nextNode = currentNode->firstChild();
        if (!nextNode)
            nextNode = currentNode->nextSibling();

        while (!nextNode && currentNode->parentNode()) {
            DeprecatedString text;
            currentNode = currentNode->parentNode();
            if (currentNode == pastEndNode)
                break;
            nextNode = currentNode->nextSibling();

            if (currentNode->hasTagName(aTag)) {
                // End of a <a> element.  Create an attributed string NSLinkAttributeName attribute
                // for the range of the link.  Note that we create the attributed string from the DOM, which
                // will have corrected any illegally nested <a> elements.
                if (linkStartNode && currentNode == linkStartNode) {
                    String href = parseURL(linkStartNode->getAttribute(hrefAttr));
                    KURL kURL = linkStartNode->document()->frame()->loader()->completeURL(href.deprecatedString());
                    
                    NSURL *URL = kURL.getNSURL();
                    NSRange tempRange = { linkStartLocation, [result length]-linkStartLocation }; // workaround for 4213314
                    [result addAttribute:NSLinkAttributeName value:URL range:tempRange];
                    linkStartNode = 0;
                }
            }
            else if (currentNode->hasTagName(olTag) || currentNode->hasTagName(ulTag)) {
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (currentNode->hasTagName(liTag)) {
                
                int i, count = listItems.size();
                for (i = 0; i < count; i++){
                    if (listItems[i] == currentNode){
                        listItemLocations[i].end = [result length];
                        break;
                    }
                }
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (currentNode->hasTagName(blockquoteTag) ||
                       currentNode->hasTagName(ddTag) ||
                       currentNode->hasTagName(divTag) ||
                       currentNode->hasTagName(dlTag) ||
                       currentNode->hasTagName(dtTag) ||
                       currentNode->hasTagName(hrTag) ||
                       currentNode->hasTagName(listingTag) ||
                       currentNode->hasTagName(preTag) ||
                       currentNode->hasTagName(tdTag) ||
                       currentNode->hasTagName(thTag)) {
                if (!hasNewLine)
                    text += '\n';
                hasNewLine = true;
            } else if (currentNode->hasTagName(pTag) ||
                       currentNode->hasTagName(trTag) ||
                       currentNode->hasTagName(h1Tag) ||
                       currentNode->hasTagName(h2Tag) ||
                       currentNode->hasTagName(h3Tag) ||
                       currentNode->hasTagName(h4Tag) ||
                       currentNode->hasTagName(h5Tag) ||
                       currentNode->hasTagName(h6Tag)) {
                if (!hasNewLine)
                    text += '\n';
                // An extra newline is needed at the start, not the end, of these types of tags,
                // so don't add another here.
                hasNewLine = true;
            }
            
            NSAttributedString *partialString = [[NSAttributedString alloc] initWithString:text.getNSString()];
            [result appendAttributedString:partialString];
            [partialString release];
        }

        currentNode = nextNode;
    }
    
    [pendingStyledSpace release];
    
    // Apply paragraph styles from outside in.  This ensures that nested lists correctly
    // override their parent's paragraph style.
    {
        unsigned i, count = listItems.size();
        Element *e;

#ifdef POSITION_LIST
        Node *containingBlock;
        int containingBlockX, containingBlockY;
        
        // Determine the position of the outermost containing block.  All paragraph
        // styles and tabs should be relative to this position.  So, the horizontal position of 
        // each item in the list (in the resulting attributed string) will be relative to position 
        // of the outermost containing block.
        if (count > 0){
            containingBlock = firstNode;
            while (containingBlock->renderer()->isInline()){
                containingBlock = containingBlock->parentNode();
            }
            containingBlock->renderer()->absolutePosition(containingBlockX, containingBlockY);
        }
#endif
        
        for (i = 0; i < count; i++){
            e = listItems[i];
            info = listItemLocations[i];
            
            if (info.end < info.start)
                info.end = [result length];
                
            RenderObject *r = e->renderer();
            RenderStyle *style = r->style();

            int rx;
            NSFont *font = style->font().primaryFont()->getNSFont();
            float pointSize = [font pointSize];

#ifdef POSITION_LIST
            int ry;
            r->absolutePosition(rx, ry);
            rx -= containingBlockX;
            
            // Ensure that the text is indented at least enough to allow for the markers.
            rx = MAX(rx, (int)maxMarkerWidth);
#else
            rx = (int)MAX(maxMarkerWidth, pointSize);
#endif

            // The bullet text will be right aligned at the first tab marker, followed
            // by a space, followed by the list item text.  The space is arbitrarily
            // picked as pointSize*2/3.  The space on the first line of the text item
            // is established by a left aligned tab, on subsequent lines it's established
            // by the head indent.
            NSMutableParagraphStyle *mps = [[NSMutableParagraphStyle alloc] init];
            [mps setFirstLineHeadIndent: 0];
            [mps setHeadIndent: rx];
            [mps setTabStops:[NSArray arrayWithObjects:
                        [[[NSTextTab alloc] initWithType:NSRightTabStopType location:rx-(pointSize*2/3)] autorelease],
                        [[[NSTextTab alloc] initWithType:NSLeftTabStopType location:rx] autorelease],
                        nil]];
            NSRange tempRange = { info.start, info.end-info.start }; // workaround for 4213314
            [result addAttribute:NSParagraphStyleAttributeName value:mps range:tempRange];
            [mps release];
        }
    }

    return result;

    END_BLOCK_OBJC_EXCEPTIONS;

    return nil;
}

@end
