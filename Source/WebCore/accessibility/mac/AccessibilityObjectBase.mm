/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "AccessibilityObject.h"

#if HAVE(ACCESSIBILITY) && PLATFORM(COCOA)

#import "WebAccessibilityObjectWrapperBase.h"

namespace WebCore {

String AccessibilityObject::speechHintAttributeValue() const
{
    auto speak = speakAsProperty();
    NSMutableArray<NSString *> *hints = [NSMutableArray array];
    [hints addObject:(speak & SpeakAs::SpellOut) ? @"spell-out" : @"normal"];
    if (speak & SpeakAs::Digits)
        [hints addObject:@"digits"];
    if (speak & SpeakAs::LiteralPunctuation)
        [hints addObject:@"literal-punctuation"];
    if (speak & SpeakAs::NoPunctuation)
        [hints addObject:@"no-punctuation"];
    return [hints componentsJoinedByString:@" "];
}

static bool isVisibleText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

static bool isDescriptiveText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

String AccessibilityObject::descriptionAttributeValue() const
{
    // Static text objects should not have a description. Its content is communicated in its AXValue.
    // One exception is the media control labels that have a value and a description. Those are set programatically.
    if (roleValue() == AccessibilityRole::StaticText && !isMediaControlLabel())
        return { };

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    // Determine if any visible text is available, which influences our usage of title tag.
    bool visibleTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isVisibleText(text.textSource)) {
            visibleTextAvailable = true;
            break;
        }
    }

    NSMutableString *returnText = [NSMutableString string];
    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative) {
            [returnText appendString:text.text];
            break;
        }

        switch (text.textSource) {
        // These are sub-components of one element (Attachment) that are re-combined in OSX and iOS.
        case AccessibilityTextSource::Title:
        case AccessibilityTextSource::Subtitle:
        case AccessibilityTextSource::Action: {
            if (!text.text.length())
                break;
            if ([returnText length])
                [returnText appendString:@", "];
            [returnText appendString:text.text];
            break;
        }
        default:
            break;
        }

        if (text.textSource == AccessibilityTextSource::TitleTag && !visibleTextAvailable) {
            [returnText appendString:text.text];
            break;
        }
    }

    return returnText;
}

String AccessibilityObject::titleAttributeValue() const
{
    // Static text objects should not have a title. Its content is communicated in its AXValue.
    if (roleValue() == AccessibilityRole::StaticText)
        return String();

    // A file upload button presents a challenge because it has button text and a value, but the
    // API doesn't support this paradigm.
    // The compromise is to return the button type in the role description and the value of the file path in the title
    if (isFileUploadButton() && fileUploadButtonReturnsValueInTitle())
        return stringValue();

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    for (const auto& text : textOrder) {
        // If we have alternative text, then we should not expose a title.
        if (text.textSource == AccessibilityTextSource::Alternative)
            break;

        // Once we encounter visible text, or the text from our children that should be used foremost.
        if (text.textSource == AccessibilityTextSource::Visible || text.textSource == AccessibilityTextSource::Children)
            return text.text;

        // If there's an element that labels this object and it's not exposed, then we should use
        // that text as our title.
        if (text.textSource == AccessibilityTextSource::LabelByElement && !exposesTitleUIElement())
            return text.text;
    }

    return { };
}

String AccessibilityObject::helpTextAttributeValue() const
{
    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    // Determine if any descriptive text is available, which influences our usage of title tag.
    bool descriptiveTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isDescriptiveText(text.textSource)) {
            descriptiveTextAvailable = true;
            break;
        }
    }

    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Help || text.textSource == AccessibilityTextSource::Summary)
            return text.text;

        // If an element does NOT have other descriptive text the title tag should be used as its descriptive text.
        // But, if those ARE available, then the title tag should be used for help text instead.
        if (text.textSource == AccessibilityTextSource::TitleTag && descriptiveTextAvailable)
            return text.text;
    }

    return { };
}

}; // namespace WebCore

#endif // HAVE(ACCESSIBILITY) && PLATFORM(COCOA)
