/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AccessibilityObject.h"

#include "AccessibilityRenderObject.h"
#include "AXObjectCache.h"
#include "CharacterNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LocalizedStrings.h"
#include "NodeList.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

AccessibilityObject::AccessibilityObject()
    : m_id(0)
    , m_haveChildren(false)
#if PLATFORM(GTK)
    , m_wrapper(0)
#endif
{
}

AccessibilityObject::~AccessibilityObject()
{
    ASSERT(isDetached());
}

void AccessibilityObject::detach()
{
#if HAVE(ACCESSIBILITY)
    setWrapper(0);
#endif    
}

AccessibilityObject* AccessibilityObject::firstChild() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::lastChild() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::previousSibling() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::nextSibling() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::parentObject() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::parentObjectUnignored() const
{
    AccessibilityObject* parent;
    for (parent = parentObject(); parent && parent->accessibilityIsIgnored(); parent = parent->parentObject())
        ;
    return parent;
}

AccessibilityObject* AccessibilityObject::parentObjectIfExists() const
{
    return 0;
}
    
int AccessibilityObject::layoutCount() const
{
    return 0;
}
    
String AccessibilityObject::text() const
{
    return String();
}
    
String AccessibilityObject::helpText() const
{
    return String();   
}

String AccessibilityObject::textUnderElement() const
{
    return String();
}

bool AccessibilityObject::isARIAInput(AccessibilityRole ariaRole)
{
    return ariaRole == RadioButtonRole || ariaRole == CheckBoxRole || ariaRole == TextFieldRole;
}    
    
bool AccessibilityObject::isARIAControl(AccessibilityRole ariaRole)
{
    return isARIAInput(ariaRole) || ariaRole == TextAreaRole || ariaRole == ButtonRole 
    || ariaRole == ComboBoxRole || ariaRole == SliderRole; 
}
    
int AccessibilityObject::intValue() const
{
    return 0;
}

String AccessibilityObject::stringValue() const
{
    return String();
}

String AccessibilityObject::ariaAccessiblityName(const String&) const
{
    return String();
}

String AccessibilityObject::ariaLabeledByAttribute() const
{
    return String();
}

String AccessibilityObject::title() const
{
    return String();
}

String AccessibilityObject::ariaDescribedByAttribute() const
{
    return String();
}

String AccessibilityObject::accessibilityDescription() const
{
    return String();
}

IntRect AccessibilityObject::boundingBoxRect() const
{
    return IntRect();
}

IntRect AccessibilityObject::elementRect() const
{
    return IntRect();
}

IntSize AccessibilityObject::size() const
{
    return IntSize();
}

IntPoint AccessibilityObject::clickPoint() const
{
    IntRect rect = elementRect();
    return IntPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
}
    
void AccessibilityObject::linkedUIElements(AccessibilityChildrenVector&) const
{
    return;
}
    
AccessibilityObject* AccessibilityObject::titleUIElement() const
{
     return 0;   
}

int AccessibilityObject::textLength() const
{
    return 0;
}

PassRefPtr<Range> AccessibilityObject::ariaSelectedTextDOMRange() const
{
    return 0;
}

String AccessibilityObject::selectedText() const
{
    return String();
}

const AtomicString& AccessibilityObject::accessKey() const
{
    return nullAtom;
}

VisibleSelection AccessibilityObject::selection() const
{
    return VisibleSelection();
}

PlainTextRange AccessibilityObject::selectedTextRange() const
{
    return PlainTextRange();
}

unsigned AccessibilityObject::selectionStart() const
{
    return selectedTextRange().start;
}

unsigned AccessibilityObject::selectionEnd() const
{
    return selectedTextRange().length;
}

void AccessibilityObject::setSelectedText(const String&)
{
    // TODO: set selected text (ReplaceSelectionCommand). <rdar://problem/4712125>
    notImplemented();
}

void AccessibilityObject::setSelectedTextRange(const PlainTextRange&)
{
}

void AccessibilityObject::makeRangeVisible(const PlainTextRange&)
{
    // TODO: make range visible (scrollRectToVisible).  <rdar://problem/4712101>
    notImplemented();
}

KURL AccessibilityObject::url() const
{
    return KURL();
}

void AccessibilityObject::setFocused(bool)
{
}

void AccessibilityObject::setValue(const String&)
{
}

void AccessibilityObject::setSelected(bool)
{
}

bool AccessibilityObject::press() const
{
    Element* actionElem = actionElement();
    if (!actionElem)
        return false;
    if (Frame* f = actionElem->document()->frame())
        f->loader()->resetMultipleFormSubmissionProtection();
    actionElem->accessKeyAction(true);
    return true;
}

AXObjectCache* AccessibilityObject::axObjectCache() const
{
    return 0;
}

Widget* AccessibilityObject::widget() const
{
    return 0;
}

Widget* AccessibilityObject::widgetForAttachmentView() const
{
    return 0;
}

Element* AccessibilityObject::anchorElement() const
{
    return 0;   
}

Element* AccessibilityObject::actionElement() const
{
    return 0;
}

// This function is like a cross-platform version of - (WebCoreTextMarkerRange*)textMarkerRange. It returns
// a Range that we can convert to a WebCoreRange in the Obj-C file
VisiblePositionRange AccessibilityObject::visiblePositionRange() const
{
    return VisiblePositionRange();
}

VisiblePositionRange AccessibilityObject::visiblePositionRangeForLine(unsigned) const
{
    return VisiblePositionRange();
}

VisiblePosition AccessibilityObject::visiblePositionForIndex(int) const
{
    return VisiblePosition();
}
    
int AccessibilityObject::indexForVisiblePosition(const VisiblePosition&) const
{
    return 0;
}
    
VisiblePositionRange AccessibilityObject::visiblePositionRangeForUnorderedPositions(const VisiblePosition& visiblePos1, const VisiblePosition& visiblePos2) const
{
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return VisiblePositionRange();

    VisiblePosition startPos;
    VisiblePosition endPos;
    bool alreadyInOrder;

    // upstream is ordered before downstream for the same position
    if (visiblePos1 == visiblePos2 && visiblePos2.affinity() == UPSTREAM)
        alreadyInOrder = false;

    // use selection order to see if the positions are in order
    else
        alreadyInOrder = VisibleSelection(visiblePos1, visiblePos2).isBaseFirst();

    if (alreadyInOrder) {
        startPos = visiblePos1;
        endPos = visiblePos2;
    } else {
        startPos = visiblePos2;
        endPos = visiblePos1;
    }

    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityObject::positionOfLeftWord(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::positionOfRightWord(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition updateAXLineStartForVisiblePosition(const VisiblePosition& visiblePosition)
{
    // A line in the accessibility sense should include floating objects, such as aligned image, as part of a line.
    // So let's update the position to include that.
    VisiblePosition tempPosition;
    VisiblePosition startPosition = visiblePosition;
    Position p;
    RenderObject* renderer;
    while (true) {
        tempPosition = startPosition.previous();
        if (tempPosition.isNull())
            break;
        p = tempPosition.deepEquivalent();
        if (!p.node())
            break;
        renderer = p.node()->renderer();
        if (!renderer || (renderer->isRenderBlock() && !p.deprecatedEditingOffset()))
            break;
        InlineBox* box;
        int ignoredCaretOffset;
        p.getInlineBoxAndOffset(tempPosition.affinity(), box, ignoredCaretOffset);
        if (box)
            break;
        startPosition = tempPosition;
    }

    return startPosition;
}

VisiblePositionRange AccessibilityObject::leftLineVisiblePositionRange(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make a caret selection for the position before marker position (to make sure
    // we move off of a line start)
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // keep searching for a valid line start position.  Unless the VisiblePosition is at the very beginning, there should
    // always be a valid line range.  However, startOfLine will return null for position next to a floating object,
    // since floating object doesn't really belong to any line.
    // This check will reposition the marker before the floating object, to ensure we get a line start.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(prevVisiblePos);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::rightLineVisiblePositionRange(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(nextVisiblePos);

    // fetch for a valid line start position
    if (startPosition.isNull() ) {
        startPosition = visiblePos;
        nextVisiblePos = nextVisiblePos.next();
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // Unless the VisiblePosition is at the very end, there should always be a valid line range.  However, endOfLine will
    // return null for position by a floating object, since floating object doesn't really belong to any line.
    // This check will reposition the marker after the floating object, to ensure we get a line end.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::sentenceForPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::paragraphForPosition(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition startOfStyleRange(const VisiblePosition visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* startRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse backward by renderer to look for style change
    for (RenderObject* r = renderer->previousInPreOrder(); r; r = r->previousInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        startRenderer = r;
    }

    return VisiblePosition(startRenderer->node(), 0, VP_DEFAULT_AFFINITY);
}

static VisiblePosition endOfStyleRange(const VisiblePosition& visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* endRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse forward by renderer to look for style change
    for (RenderObject* r = renderer->nextInPreOrder(); r; r = r->nextInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        endRenderer = r;
    }

    return lastDeepEditingPositionForNode(endRenderer->node());
}

VisiblePositionRange AccessibilityObject::styleRangeForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    return VisiblePositionRange(startOfStyleRange(visiblePos), endOfStyleRange(visiblePos));
}

// NOTE: Consider providing this utility method as AX API
VisiblePositionRange AccessibilityObject::visiblePositionRangeForRange(const PlainTextRange& range) const
{
    if (range.start + range.length > text().length())
        return VisiblePositionRange();

    VisiblePosition startPosition = visiblePositionForIndex(range.start);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = visiblePositionForIndex(range.start + range.length);
    return VisiblePositionRange(startPosition, endPosition);
}

static bool replacedNodeNeedsCharacter(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !replacedNode->renderer() || !replacedNode->renderer()->isReplaced() || replacedNode->isTextNode()) {
        return false;
    }

    // create an AX object, but skip it if it is not supposed to be seen
    AccessibilityObject* object = replacedNode->renderer()->document()->axObjectCache()->getOrCreate(replacedNode->renderer());
    if (object->accessibilityIsIgnored())
        return false;

    return true;
}

String AccessibilityObject::stringForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return String();

    Vector<UChar> resultVector;
    RefPtr<Range> range = makeRange(visiblePositionRange.start, visiblePositionRange.end);
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            resultVector.append(it.characters(), it.length());
        } else {
            // locate the node and starting offset for this replaced range
            int exception = 0;
            Node* node = it.range()->startContainer(exception);
            ASSERT(node == it.range()->endContainer(exception));
            int offset = it.range()->startOffset(exception);

            if (replacedNodeNeedsCharacter(node->childNode(offset))) {
                resultVector.append(objectReplacementCharacter);
            }
        }
    }

    return String::adopt(resultVector);
}

IntRect AccessibilityObject::boundsForVisiblePositionRange(const VisiblePositionRange&) const
{
    return IntRect();
}

int AccessibilityObject::lengthForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    // FIXME: Multi-byte support
    if (visiblePositionRange.isNull())
        return -1;
    
    int length = 0;
    RefPtr<Range> range = makeRange(visiblePositionRange.start, visiblePositionRange.end);
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            length += it.length();
        } else {
            // locate the node and starting offset for this replaced range
            int exception = 0;
            Node* node = it.range()->startContainer(exception);
            ASSERT(node == it.range()->endContainer(exception));
            int offset = it.range()->startOffset(exception);

            if (replacedNodeNeedsCharacter(node->childNode(offset)))
                length++;
        }
    }
    
    return length;
}

void AccessibilityObject::setSelectedVisiblePositionRange(const VisiblePositionRange&) const
{
}

VisiblePosition AccessibilityObject::visiblePositionForPoint(const IntPoint&) const
{
    return VisiblePosition();
}

VisiblePosition AccessibilityObject::nextVisiblePosition(const VisiblePosition& visiblePos) const
{
    return visiblePos.next();
}

VisiblePosition AccessibilityObject::previousVisiblePosition(const VisiblePosition& visiblePos) const
{
    return visiblePos.previous();
}

VisiblePosition AccessibilityObject::nextWordEnd(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    return endOfWord(nextVisiblePos, LeftWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::previousWordStart(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    return startOfWord(prevVisiblePos, RightWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::nextLineEndPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // to make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // There are cases like when the position is next to a floating object that'll return null for end of line. This code will avoid returning null.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return endPosition;
}

VisiblePosition AccessibilityObject::previousLineStartPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // as long as the position hasn't reached the beginning of the doc,  keep searching for a valid line start position
    // There are cases like when the position is next to a floating object that'll return null for start of line. This code will avoid returning null.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    return startPosition;
}

VisiblePosition AccessibilityObject::nextSentenceEndPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    // an empty line is considered a sentence. If it's skipped, then the sentence parser will not
    // see this empty line.  Instead, return the end position of the empty line.
    VisiblePosition endPosition;
    
    String lineString = plainText(makeRange(startOfLine(nextVisiblePos), endOfLine(nextVisiblePos)).get());
    if (lineString.isEmpty())
        endPosition = nextVisiblePos;
    else
        endPosition = endOfSentence(nextVisiblePos);

    return endPosition;
}

VisiblePosition AccessibilityObject::previousSentenceStartPosition(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence start
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return VisiblePosition();

    // treat empty line as a separate sentence.
    VisiblePosition startPosition;
    
    String lineString = plainText(makeRange(startOfLine(previousVisiblePos), endOfLine(previousVisiblePos)).get());
    if (lineString.isEmpty())
        startPosition = previousVisiblePos;
    else
        startPosition = startOfSentence(previousVisiblePos);

    return startPosition;
}

VisiblePosition AccessibilityObject::nextParagraphEndPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph end
    VisiblePosition nextPos = visiblePos.next();
    if (nextPos.isNull())
        return VisiblePosition();

    return endOfParagraph(nextPos);
}

VisiblePosition AccessibilityObject::previousParagraphStartPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph start
    VisiblePosition previousPos = visiblePos.previous();
    if (previousPos.isNull())
        return VisiblePosition();

    return startOfParagraph(previousPos);
}

// NOTE: Consider providing this utility method as AX API
VisiblePosition AccessibilityObject::visiblePositionForIndex(unsigned, bool) const
{
    return VisiblePosition();
}

AccessibilityObject* AccessibilityObject::accessibilityObjectForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return 0;

    RenderObject* obj = visiblePos.deepEquivalent().node()->renderer();
    if (!obj)
        return 0;

    return obj->document()->axObjectCache()->getOrCreate(obj);
}

int AccessibilityObject::lineForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return 0;

    unsigned lineCount = 0;
    VisiblePosition currentVisiblePos = visiblePos;
    VisiblePosition savedVisiblePos;

    // move up until we get to the top
    // FIXME: This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos))) {
        ++lineCount;
        savedVisiblePos = currentVisiblePos;
        VisiblePosition prevVisiblePos = previousLinePosition(currentVisiblePos, 0);
        currentVisiblePos = prevVisiblePos;
    }

    return lineCount - 1;
}

// NOTE: Consider providing this utility method as AX API
PlainTextRange AccessibilityObject::plainTextRangeForVisiblePositionRange(const VisiblePositionRange& positionRange) const
{
    int index1 = index(positionRange.start);
    int index2 = index(positionRange.end);
    if (index1 < 0 || index2 < 0 || index1 > index2)
        return PlainTextRange();

    return PlainTextRange(index1, index2 - index1);
}

// NOTE: Consider providing this utility method as AX API
int AccessibilityObject::index(const VisiblePosition&) const
{
    return -1;
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
PlainTextRange AccessibilityObject::doAXRangeForLine(unsigned) const
{
    return PlainTextRange();
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given screen coordinates. This parameterized attribute returns the
// complete range of characters (including surrogate pairs of multi-byte glyphs) at the given
// screen coordinates.
// NOTE: This varies from AppKit when the point is below the last line. AppKit returns an
// an error in that case. We return textControl->text().length(), 1. Does this matter?
PlainTextRange AccessibilityObject::doAXRangeForPosition(const IntPoint& point) const
{
    int i = index(visiblePositionForPoint(point));
    if (i < 0)
        return PlainTextRange();

    return PlainTextRange(i, 1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
PlainTextRange AccessibilityObject::doAXRangeForIndex(unsigned) const
{
    return PlainTextRange();
}

// Given a character index, the range of text associated with this accessibility object
// over which the style in effect at that character index applies.
PlainTextRange AccessibilityObject::doAXStyleRangeForIndex(unsigned index) const
{
    VisiblePositionRange range = styleRangeForPosition(visiblePositionForIndex(index, false));
    return plainTextRangeForVisiblePositionRange(range);
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
String AccessibilityObject::doAXStringForRange(const PlainTextRange&) const
{
    return String();
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
IntRect AccessibilityObject::doAXBoundsForRange(const PlainTextRange&) const
{
    return IntRect();
}

// Given an indexed character, the line number of the text associated with this accessibility
// object that contains the character.
unsigned AccessibilityObject::doAXLineForIndex(unsigned index)
{
    return lineForPosition(visiblePositionForIndex(index, false));
}

FrameView* AccessibilityObject::documentFrameView() const 
{ 
    const AccessibilityObject* object = this;
    while (object && !object->isAccessibilityRenderObject()) 
        object = object->parentObject();
        
    if (!object)
        return 0;

    return object->documentFrameView();
}    

AccessibilityObject* AccessibilityObject::doAccessibilityHitTest(const IntPoint&) const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::focusedUIElement() const
{
    return 0;
}

AccessibilityObject* AccessibilityObject::observableObject() const
{
    return 0;
}

AccessibilityRole AccessibilityObject::roleValue() const
{
    return UnknownRole;
}
    
AccessibilityRole AccessibilityObject::ariaRoleAttribute() const
{
    return UnknownRole;
}

bool AccessibilityObject::isPresentationalChildOfAriaRole() const
{
    return false;
}

bool AccessibilityObject::ariaRoleHasPresentationalChildren() const
{
    return false;
}

void AccessibilityObject::clearChildren()
{
    m_haveChildren = false;
    m_children.clear();
}

void AccessibilityObject::childrenChanged()
{
    return;
}

void AccessibilityObject::addChildren()
{
}

void AccessibilityObject::selectedChildren(AccessibilityChildrenVector&)
{
}

void AccessibilityObject::visibleChildren(AccessibilityChildrenVector&)
{
}
    
unsigned AccessibilityObject::axObjectID() const
{
    return m_id;
}

void AccessibilityObject::setAXObjectID(unsigned axObjectID)
{
    m_id = axObjectID;
}

const String& AccessibilityObject::actionVerb() const
{
    // FIXME: Need to add verbs for select elements.
    DEFINE_STATIC_LOCAL(const String, buttonAction, (AXButtonActionVerb()));
    DEFINE_STATIC_LOCAL(const String, textFieldAction, (AXTextFieldActionVerb()));
    DEFINE_STATIC_LOCAL(const String, radioButtonAction, (AXRadioButtonActionVerb()));
    DEFINE_STATIC_LOCAL(const String, checkedCheckBoxAction, (AXCheckedCheckBoxActionVerb()));
    DEFINE_STATIC_LOCAL(const String, uncheckedCheckBoxAction, (AXUncheckedCheckBoxActionVerb()));
    DEFINE_STATIC_LOCAL(const String, linkAction, (AXLinkActionVerb()));
    DEFINE_STATIC_LOCAL(const String, noAction, ());

    switch (roleValue()) {
        case ButtonRole:
            return buttonAction;
        case TextFieldRole:
        case TextAreaRole:
            return textFieldAction;
        case RadioButtonRole:
            return radioButtonAction;
        case CheckBoxRole:
            return isChecked() ? checkedCheckBoxAction : uncheckedCheckBoxAction;
        case LinkRole:
        case WebCoreLinkRole:
            return linkAction;
        default:
            return noAction;
    }
}

void AccessibilityObject::updateBackingStore()
{
}
    
} // namespace WebCore
