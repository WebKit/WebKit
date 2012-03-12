/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCue.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "HTMLDivElement.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "WebVTTParser.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const int invalidCueIndex = -1;

static const String& startKeyword()
{
    DEFINE_STATIC_LOCAL(const String, start, ("start"));
    return start;
}

static const String& middleKeyword()
{
    DEFINE_STATIC_LOCAL(const String, middle, ("middle"));
    return middle;
}

static const String& endKeyword()
{
    DEFINE_STATIC_LOCAL(const String, end, ("end"));
    return end;
}

static const String& horizontalKeyword()
{
    return emptyString();
}

static const String& verticalGrowingLeftKeyword()
{
    DEFINE_STATIC_LOCAL(const String, vertical, ("rl"));
    return vertical;
}

static const String& verticalGrowingRightKeyword()
{
    DEFINE_STATIC_LOCAL(const String, verticallr, ("lr"));
    return verticallr;
}

// FIXME: remove this once https://bugs.webkit.org/show_bug.cgi?id=78706 has been fixed.
static const String& verticalKeywordOLD()
{
    DEFINE_STATIC_LOCAL(const String, vertical, ("vertical"));
    return vertical;
}

// FIXME: remove this once https://bugs.webkit.org/show_bug.cgi?id=78706 has been fixed.
static const String& verticallrKeywordOLD()
{
    DEFINE_STATIC_LOCAL(const String, verticallr, ("vertical-lr"));
    return verticallr;
}
    

TextTrackCue::TextTrackCue(ScriptExecutionContext* context, const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit)
    : m_id(id)
    , m_startTime(start)
    , m_endTime(end)
    , m_content(content)
    , m_linePosition(-1)
    , m_textPosition(50)
    , m_cueSize(100)
    , m_cueIndex(invalidCueIndex)
    , m_writingDirection(Horizontal)
    , m_cueAlignment(Middle)
    , m_scriptExecutionContext(context)
    , m_isActive(false)
    , m_pauseOnExit(pauseOnExit)
    , m_snapToLines(true)
    , m_displayTreeShouldChange(true)
    , m_displayTree(HTMLDivElement::create(static_cast<Document*>(context)))
{
    ASSERT(m_scriptExecutionContext->isDocument());
    parseSettings(settings);
}

TextTrackCue::~TextTrackCue()
{
}

void TextTrackCue::cueWillChange()
{
    if (m_track)
        m_track->cueWillChange(this);
}

void TextTrackCue::cueDidChange()
{
    if (m_track)
        m_track->cueDidChange(this);

    m_displayTreeShouldChange = true;
}

TextTrack* TextTrackCue::track() const
{
    return m_track.get();
}

void TextTrackCue::setTrack(PassRefPtr<TextTrack>track)
{
    m_track = track;
}

void TextTrackCue::setId(const String& id)
{
    if (m_id == id)
        return;

    cueWillChange();
    m_id = id;
    cueDidChange();
}

void TextTrackCue::setStartTime(double value)
{
    if (m_startTime == value)
        return;
    
    cueWillChange();
    m_startTime = value;
    cueDidChange();
}
    
void TextTrackCue::setEndTime(double value)
{
    if (m_endTime == value)
        return;
    
    cueWillChange();
    m_endTime = value;
    cueDidChange();
}
    
void TextTrackCue::setPauseOnExit(bool value)
{
    if (m_pauseOnExit == value)
        return;
    
    cueWillChange();
    m_pauseOnExit = value;
    cueDidChange();
}

const String& TextTrackCue::vertical() const
{
    switch (m_writingDirection) {
    case Horizontal: 
        return horizontalKeyword();
    case VerticalGrowingLeft:
        return verticalGrowingLeftKeyword();
    case VerticalGrowingRight:
        return verticalGrowingRightKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void TextTrackCue::setVertical(const String& value, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-vertical
    // On setting, the text track cue writing direction must be set to the value given 
    // in the first cell of the row in the table above whose second cell is a 
    // case-sensitive match for the new value, if any. If none of the values match, then
    // the user agent must instead throw a SyntaxError exception.
    
    WritingDirection direction = m_writingDirection;
    if (value == horizontalKeyword())
        direction = Horizontal;
    else if (value == verticalGrowingLeftKeyword())
        direction = VerticalGrowingLeft;
    else if (value == verticalGrowingRightKeyword())
        direction = VerticalGrowingRight;
    else
        ec = SYNTAX_ERR;
    
    if (direction == m_writingDirection)
        return;

    cueWillChange();
    m_writingDirection = direction;
    cueDidChange();
}

void TextTrackCue::setSnapToLines(bool value)
{
    if (m_snapToLines == value)
        return;
    
    cueWillChange();
    m_snapToLines = value;
    cueDidChange();
}

void TextTrackCue::setLine(int position, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-line
    // On setting, if the text track cue snap-to-lines flag is not set, and the new
    // value is negative or greater than 100, then throw an IndexSizeError exception.
    if (!m_snapToLines && (position < 0 || position > 100)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // Otherwise, set the text track cue line position to the new value.
    if (m_linePosition == position)
        return;
    
    cueWillChange();
    m_linePosition = position; 
    cueDidChange();
}

void TextTrackCue::setPosition(int position, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-position
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError exception.
    // Otherwise, set the text track cue text position to the new value.
    if (position < 0 || position > 100) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // Otherwise, set the text track cue line position to the new value.
    if (m_textPosition == position)
        return;
    
    cueWillChange();
    m_textPosition = position; 
    cueDidChange();
}

void TextTrackCue::setSize(int size, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-size
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError
    // exception. Otherwise, set the text track cue size to the new value.
    if (size < 0 || size > 100) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // Otherwise, set the text track cue line position to the new value.
    if (m_cueSize == size)
        return;
    
    cueWillChange();
    m_cueSize = size;
    cueDidChange();
}

const String& TextTrackCue::align() const
{
    switch (m_cueAlignment) {
    case Start:
        return startKeyword();
    case Middle:
        return middleKeyword();
    case End:
        return endKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void TextTrackCue::setAlign(const String& value, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-align
    // On setting, the text track cue alignment must be set to the value given in the 
    // first cell of the row in the table above whose second cell is a case-sensitive
    // match for the new value, if any. If none of the values match, then the user
    // agent must instead throw a SyntaxError exception.
    
    Alignment alignment = m_cueAlignment;
    if (value == startKeyword())
        alignment = Start;
    else if (value == middleKeyword())
        alignment = Middle;
    else if (value == endKeyword())
        alignment = End;
    else
        ec = SYNTAX_ERR;
    
    if (alignment == m_cueAlignment)
        return;

    cueWillChange();
    m_cueAlignment = alignment;
    cueDidChange();
}
    
void TextTrackCue::setText(const String& text)
{
    if (m_content == text)
        return;
    
    cueWillChange();
    // Clear the document fragment but don't bother to create it again just yet as we can do that
    // when it is requested.
    m_documentFragment = 0;
    m_content = text;
    cueDidChange();
}

int TextTrackCue::cueIndex()
{
    if (m_cueIndex == invalidCueIndex)
        m_cueIndex = track()->cues()->getCueIndex(this);

    return m_cueIndex;
}

void TextTrackCue::invalidateCueIndex()
{
    m_cueIndex = invalidCueIndex;
}

PassRefPtr<DocumentFragment> TextTrackCue::getCueAsHTML()
{
    RefPtr<DocumentFragment> clonedFragment;
    Document* document;

    if (!m_documentFragment)
        m_documentFragment = WebVTTParser::create(0, m_scriptExecutionContext)->createDocumentFragmentFromCueText(m_content);

    document = static_cast<Document*>(m_scriptExecutionContext);

    clonedFragment = DocumentFragment::create(document);
    m_documentFragment->cloneChildNodes(clonedFragment.get());

    return clonedFragment.release();
}

bool TextTrackCue::dispatchEvent(PassRefPtr<Event> event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::DISABLED)
        return false;

    return EventTarget::dispatchEvent(event);
}

bool TextTrackCue::dispatchEvent(PassRefPtr<Event> event, ExceptionCode &ec)
{
    return EventTarget::dispatchEvent(event, ec);
}

bool TextTrackCue::isActive()
{
    return m_isActive && track() && track()->mode() != TextTrack::DISABLED;
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;

    if (!active) {
        // Remove the display tree as soon as the cue becomes inactive.
        ExceptionCode ec;
        m_displayTree->remove(ec);
    }
}

void TextTrackCue::determineDisplayParameters()
{
    // FIXME(BUG 79749): Determine the text direction using the BIDI algorithm.
    // Steps 10.2, 10.3

    // FIXME(BUG 79747): Determine the display parameters from the rules.
    // Steps 10.1, 10.4 - 10.10
}

PassRefPtr<HTMLDivElement> TextTrackCue::getDisplayTree()
{
    if (!m_displayTreeShouldChange)
        return m_displayTree;

    // 10.1 - 10.10
    determineDisplayParameters();

    // 10.11. Apply the terms of the CSS specifications to nodes within the
    // following constraints, thus obtaining a set of CSS boxes positioned
    // relative to an initial containing block:
    m_displayTree->removeChildren();

    // The document tree is the tree of WebVTT Node Objects rooted at nodes.

    // The children of the nodes must be wrapped in an anonymous box whose
    // 'display' property has the value 'inline'. This is the WebVTT cue
    // background box.
    m_displayTree->setShadowPseudoId(AtomicString("-webkit-media-text-track-display"), ASSERT_NO_EXCEPTION);
    m_displayTree->appendChild(getCueAsHTML(), ASSERT_NO_EXCEPTION, true);

    // FIXME(BUG 79916): Runs of children of WebVTT Ruby Objects that are not
    // WebVTT Ruby Text Objects must be wrapped in anonymous boxes whose
    // 'display' property has the value 'ruby-base'.

    // FIXME(BUG 79916): Text runs must be wrapped according to the CSS
    // line-wrapping rules, except that additionally, regardless of the value of
    // the 'white-space' property, lines must be wrapped at the edge of their
    // containing blocks, even if doing so requires splitting a word where there
    // is no line breaking opportunity. (Thus, normally text wraps as needed,
    // but if there is a particularly long word, it does not overflow as it
    // normally would in CSS, it is instead forcibly wrapped at the box's edge.)

    // FIXME(BUG 79750, 79751): Steps 10.12 - 10.14

    m_displayTreeShouldChange = false;

    // 10.15. Let cue's text track cue display state have the CSS boxes in
    // boxes.
    return m_displayTree;
}

void TextTrackCue::parseSettings(const String& input)
{
    // 4.8.10.13.3 Parse the WebVTT settings.
    // 1 - Initial setup.
    unsigned position = 0;
    while (position < input.length()) {
        // Discard any space characters between or after settings (not in the spec, but we think it should be).
        while (position < input.length() && WebVTTParser::isASpace(input[position]))
            position++;

        // 2-4 Settings - get the next character representing a settings.
        char setting = input[position++];
        if (position >= input.length())
            return;

        // 5-7 - If the character at position is not ':', set setting to empty string.
        if (input[position++] != ':')
            setting = '\0';
        if (position >= input.length())
            return;

        // 8 - Gather settings based on value of setting.
        switch (setting) {
        case 'D':
            {
            // 1-3 - Collect the next word and set the writing direction accordingly.
            String writingDirection = WebVTTParser::collectWord(input, &position);
            if (writingDirection == verticalKeywordOLD())
                m_writingDirection = VerticalGrowingLeft;
            else if (writingDirection == verticallrKeywordOLD())
                m_writingDirection = VerticalGrowingRight;
            }
            break;
        case 'L':
            {
            // 1-2 - Collect chars that are either '-', '%', or a digit.
            StringBuilder linePositionBuilder;
            while (position < input.length() && (input[position] == '-' || input[position] == '%' || isASCIIDigit(input[position])))
                linePositionBuilder.append(input[position++]);
            if (position < input.length() && !WebVTTParser::isASpace(input[position]))
                goto Otherwise;
            String linePosition = linePositionBuilder.toString();

            // 3-5 - If there is not at least one digit character,
            //       a '-' occurs anywhere other than the front, or
            //       a '%' occurs anywhere other than the end, then
            //       ignore this setting and keep looking.
            if (linePosition.find('-', 1) != notFound || linePosition.reverseFind("%", linePosition.length() - 2) != notFound)
                break;

            // 6 - If the first char is a '-' and the last char is a '%', ignore and keep looking.
            if (linePosition[0] == '-' && linePosition[linePosition.length() - 1] == '%')
                break;

            // 7 - Interpret as number (toInt ignores trailing non-digit characters,
            //     such as a possible '%').
            bool validNumber;
            int number = linePosition.toInt(&validNumber);
            if (!validNumber)
                break;

            // 8 - If the last char is a '%' and the value is not between 0 and 100, ignore and keep looking.
            if (linePosition[linePosition.length() - 1] == '%') {
                if (number < 0 || number > 100)
                    break;

                // 10 - If '%' then set snap-to-lines flag to false.
                m_snapToLines = false;
            }

            // 9 - Set cue line position to the number found.
            m_linePosition = number;
            }
            break;
        case 'T':
            {
            // 1-2 - Collect characters that are digits.
            String textPosition = WebVTTParser::collectDigits(input, &position);
            if (position >= input.length())
                break;

            // 3 - Character at end must be '%', otherwise ignore and keep looking.
            if (input[position++] != '%')
                goto Otherwise;

            // 4-6 - Ensure no other chars in this setting and setting is not empty.
            if (position < input.length() && !WebVTTParser::isASpace(input[position]))
                goto Otherwise;
            if (textPosition.isEmpty())
                break;

            // 7-8 - Interpret as number and make sure it is between 0 and 100
            // (toInt ignores trailing non-digit characters, such as a possible '%').
            bool validNumber;
            int number = textPosition.toInt(&validNumber);
            if (!validNumber)
                break;
            if (number < 0 || number > 100)
              break;

            // 9 - Set cue text position to the number found.
            m_textPosition = number;
            }
            break;
        case 'S':
            {
            // 1-2 - Collect characters that are digits.
            String cueSize = WebVTTParser::collectDigits(input, &position);
            if (position >= input.length())
                break;

            // 3 - Character at end must be '%', otherwise ignore and keep looking.
            if (input[position++] != '%')
                goto Otherwise;

            // 4-6 - Ensure no other chars in this setting and setting is not empty.
            if (position < input.length() && !WebVTTParser::isASpace(input[position]))
                goto Otherwise;
            if (cueSize.isEmpty())
                break;

            // 7-8 - Interpret as number and make sure it is between 0 and 100.
            bool validNumber;
            int number = cueSize.toInt(&validNumber);
            if (!validNumber)
                break;
            if (number < 0 || number > 100)
                break;

            // 9 - Set cue size to the number found.
            m_cueSize = number;
            }
            break;
        case 'A':
            {
            // 1-4 - Collect the next word and set the cue alignment accordingly.
            String cueAlignment = WebVTTParser::collectWord(input, &position);
            if (cueAlignment == startKeyword())
                m_cueAlignment = Start;
            else if (cueAlignment == middleKeyword())
                m_cueAlignment = Middle;
            else if (cueAlignment == endKeyword())
                m_cueAlignment = End;
            }
            break;
        }

        continue;

Otherwise:
        // Collect a sequence of characters that are not space characters and discard them.
        WebVTTParser::collectWord(input, &position);
    }
}

const AtomicString& TextTrackCue::interfaceName() const
{
    return eventNames().interfaceForTextTrackCue;
}

ScriptExecutionContext* TextTrackCue::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

EventTargetData* TextTrackCue::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* TextTrackCue::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif
