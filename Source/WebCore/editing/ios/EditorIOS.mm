/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Editor.h"

#if PLATFORM(IOS_FAMILY)

#import "CSSComputedStyleDeclaration.h"
#import "CSSPrimitiveValueMappings.h"
#import "CachedImage.h"
#import "DataTransfer.h"
#import "DictationCommandIOS.h"
#import "DocumentFragment.h"
#import "DocumentMarkerController.h"
#import "Editing.h"
#import "EditorClient.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLTextAreaElement.h"
#import "LocalFrame.h"
#import "MutableStyleProperties.h"
#import "Pasteboard.h"
#import "RenderBlock.h"
#import "RenderImage.h"
#import "SharedBuffer.h"
#import "Text.h"
#import "TypingCommand.h"
#import "WAKAppKitStubs.h"
#import "WebContentReader.h"
#import "markup.h"
#import <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

void Editor::setTextAlignmentForChangedBaseWritingDirection(WritingDirection direction)
{
    // Note that the passed-in argument is the direction that has been changed to by
    // some code or user interaction outside the scope of this function. The former
    // direction is not known, nor is it required for the kind of text alignment
    // changes done by this function.
    //
    // Rules:
    // When text has no explicit alignment, set to alignment to match the writing direction.
    // If the text has left or right alignment, flip left->right and right->left. 
    // Otherwise, do nothing.

    Ref document = protectedDocument();
    auto selectionStyle = EditingStyle::styleAtSelectionStart(document->selection().selection());
    if (!selectionStyle || !selectionStyle->style())
         return;

    auto value = selectionStyle->style()->propertyAsValueID(CSSPropertyTextAlign);
    if (!value)
        return;
        
    CSSValueID newValue;
    switch (*value) {
    case CSSValueStart:
    case CSSValueEnd:
        switch (direction) {
        case WritingDirection::Natural:
            // no-op
            return;
        case WritingDirection::LeftToRight:
            newValue = CSSValueLeft;
            break;
        case WritingDirection::RightToLeft:
            newValue = CSSValueRight;
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
        break;
    case CSSValueLeft:
    case CSSValueWebkitLeft:
        newValue = CSSValueRight;
        break;
    case CSSValueRight:
    case CSSValueWebkitRight:
        newValue = CSSValueLeft;
        break;
    case CSSValueCenter:
    case CSSValueWebkitCenter:
    case CSSValueJustify:
        // no-op
        return;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr focusedElement = document->focusedElement();
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(focusedElement); (input && (input->isTextField() || input->isSearchField()))
        || is<HTMLTextAreaElement>(focusedElement)) {
        if (direction == WritingDirection::Natural)
            return;
        focusedElement->setAttributeWithoutSynchronization(alignAttr, nameString(newValue));
        document->updateStyleIfNeeded();
        return;
    }

    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.ptr());
}

void Editor::removeUnchangeableStyles()
{
    // This function removes styles that the user cannot modify by applying their default values.
    
    auto editingStyle = EditingStyle::create(document().bodyOrFrameset());
    auto defaultStyle = editingStyle->style()->mutableCopy();
    
    // Text widgets implement background color via the UIView property. Their body element will not have one.
    defaultStyle->setProperty(CSSPropertyBackgroundColor, "rgba(255, 255, 255, 0.0)"_s);
    
    // Remove properties that the user can modify, like font-weight. 
    // Also remove font-family, per HI spec.
    // FIXME: it'd be nice if knowledge about which styles were unchangeable was not hard-coded here.
    defaultStyle->removeProperty(CSSPropertyFontWeight);
    defaultStyle->removeProperty(CSSPropertyFontStyle);
    defaultStyle->removeProperty(CSSPropertyFontVariantCaps);
    // FIXME: we should handle also pasted quoted text, strikethrough, etc. <rdar://problem/9255115>
    defaultStyle->removeProperty(CSSPropertyTextDecorationLine);
    defaultStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect); // implements underline

    // FIXME add EditAction::MatchStlye <rdar://problem/9156507> Undo rich text's paste & match style should say "Undo Match Style"
    applyStyleToSelection(defaultStyle.ptr(), EditAction::ChangeAttributes);
}

static void getImage(Element& imageElement, RefPtr<Image>& image, CachedImage*& cachedImage)
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(imageElement.renderer());
    if (!renderImage)
        return;

    CachedResourceHandle tentativeCachedImage = renderImage->cachedImage();
    if (!tentativeCachedImage || tentativeCachedImage->errorOccurred())
        return;

    image = tentativeCachedImage->imageForRenderer(renderImage.get());
    if (!image)
        return;

    cachedImage = tentativeCachedImage.get();
}

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element& imageElement, const URL& url, const String& title)
{
    PasteboardImage pasteboardImage;

    RefPtr<Image> image;
    CachedImage* cachedImage = nullptr;
    getImage(imageElement, image, cachedImage);
    if (!image)
        return;
    ASSERT(cachedImage);

    auto imageSourceURL = imageElement.document().completeURL(imageElement.imageSourceURL());

    auto pasteboardImageURL = url.isEmpty() ? imageSourceURL : url;
    if (!pasteboardImageURL.protocolIsFile()) {
        pasteboardImage.url.url = pasteboardImageURL;
        pasteboardImage.url.title = title;
    }
    pasteboardImage.suggestedName = imageSourceURL.lastPathComponent().toString();
    pasteboardImage.imageSize = image->size();
    pasteboardImage.resourceMIMEType = pasteboard.resourceMIMEType(cachedImage->response().mimeType());
    if (auto* buffer = cachedImage->resourceBuffer())
        pasteboardImage.resourceData = buffer->makeContiguous();

    if (!pasteboard.isStatic())
        client()->getClientPasteboardData(makeRangeSelectingNode(imageElement), pasteboardImage.clientTypesAndData);

    pasteboard.write(pasteboardImage);
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, OptionSet<PasteOption> options)
{
    auto range = selectedRange();
    bool allowPlainText = options.contains(PasteOption::AllowPlainText);
    WebContentReader reader(*document().frame(), *range, allowPlainText);
    int numberOfPasteboardItems = client()->getPasteboardItemsCount();
    for (int i = 0; i < numberOfPasteboardItems; ++i) {
        auto fragment = client()->documentFragmentFromDelegate(i);
        if (!fragment)
            continue;
        reader.addFragment(fragment.releaseNonNull());
    }

    auto fragment = reader.takeFragment();
    if (!fragment) {
        bool chosePlainTextIgnored;
        fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainTextIgnored);
    }

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, range, EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote);
}

void Editor::platformCopyFont()
{
}

void Editor::platformPasteFont()
{
}

void Editor::insertDictationPhrases(Vector<Vector<String>>&& dictationPhrases, id metadata)
{
    Ref document = protectedDocument();
    if (document->selection().isNone())
        return;

    if (dictationPhrases.isEmpty())
        return;

    DictationCommandIOS::create(WTFMove(document), WTFMove(dictationPhrases), metadata)->apply();
}

void Editor::setDictationPhrasesAsChildOfElement(const Vector<Vector<String>>& dictationPhrases, id metadata, Element& element)
{
    // Clear the composition.
    clear();

    // Clear the Undo stack, since the operations that follow are not Undoable, and will corrupt the stack.
    // Some day we could make them Undoable, and let callers clear the Undo stack explicitly if they wish.
    clearUndoRedoOperations();

    Ref document = protectedDocument();
    document->selection().clear();

    element.removeChildren();

    if (dictationPhrases.isEmpty()) {
        client()->respondToChangedContents();
        return;
    }

    auto context = makeRangeSelectingNodeContents(element);

    StringBuilder dictationPhrasesBuilder;
    for (auto& interpretations : dictationPhrases)
        dictationPhrasesBuilder.append(interpretations[0]);

    element.appendChild(createFragmentFromText(context, dictationPhrasesBuilder.toString()));

    WeakPtr weakElement { element };

    // We need a layout in order to add markers below.
    document->updateLayout();

    if (!weakElement)
        return;

    RefPtr textNode = dynamicDowncast<Text>(*element.firstChild());
    ASSERT(textNode);
    if (!textNode)
        return;

    unsigned previousDictationPhraseStart = 0;
    for (auto& interpretations : dictationPhrases) {
        auto dictationPhraseLength = interpretations[0].length();
        if (interpretations.size() > 1) {
            auto alternatives = interpretations;
            alternatives.remove(0);
            addMarker(*textNode, previousDictationPhraseStart, dictationPhraseLength, DocumentMarker::Type::DictationPhraseWithAlternatives, WTFMove(alternatives));
        }
        previousDictationPhraseStart += dictationPhraseLength;
    }

    addMarker(*textNode, 0, textNode->length(), DocumentMarker::Type::DictationResult, retainPtr(metadata));

    client()->respondToChangedContents();
}

void Editor::confirmMarkedText()
{
    // FIXME: This is a hacky workaround for the keyboard calling this method too late -
    // after the selection and focus have already changed. See <rdar://problem/5975559>.
    Ref document = protectedDocument();
    RefPtr focused = document->focusedElement();
    RefPtr composition = compositionNode();
    if (composition && focused && !composition->isDescendantOrShadowDescendantOf(*focused)) {
        cancelComposition();
        document->setFocusedElement(focused.get());
    } else
        confirmComposition();
}

void Editor::setTextAsChildOfElement(String&& text, Element& element)
{
    // Clear the composition
    clear();

    // Clear the Undo stack, since the operations that follow are not Undoable, and will corrupt the stack.
    // Some day we could make them Undoable, and let callers clear the Undo stack explicitly if they wish.
    clearUndoRedoOperations();

    // If the element is empty already and we're not adding text, we can early return and avoid clearing/setting
    // a selection at [0, 0] and the expense involved in creating VisiblePositions.
    if (!element.firstChild() && text.isEmpty())
        return;

    // As a side effect this function sets a caret selection after the inserted content.
    // What follows is more expensive if there is a selection, so clear it since it's going to change anyway.
    Ref document = protectedDocument();
    document->selection().clear();

    element.stringReplaceAll(WTFMove(text));

    VisiblePosition afterContents = makeContainerOffsetPosition(&element, element.countChildNodes());
    if (afterContents.isNull())
        return;
    document->selection().setSelection(afterContents);

    client()->respondToChangedContents();
}

// If the selection is adjusted from UIKit without closing the typing, the typing command may
// have a stale selection.
void Editor::ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping()
{
    Ref document = protectedDocument();
    TypingCommand::ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping(document, document->selection().selection());
}

bool Editor::writingSuggestionsSupportsSuffix()
{
    return false;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
