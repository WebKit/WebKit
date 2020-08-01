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
#import "Frame.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLParserIdioms.h"
#import "HTMLTextAreaElement.h"
#import "Pasteboard.h"
#import "RenderBlock.h"
#import "RenderImage.h"
#import "SharedBuffer.h"
#import "StyleProperties.h"
#import "Text.h"
#import "TypingCommand.h"
#import "WAKAppKitStubs.h"
#import "WebContentReader.h"
#import "markup.h"
#import <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

void Editor::showFontPanel()
{
}

void Editor::showStylesPanel()
{
}

void Editor::showColorPanel()
{
}

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

    auto selectionStyle = EditingStyle::styleAtSelectionStart(m_document.selection().selection());
    if (!selectionStyle || !selectionStyle->style())
         return;

    RefPtr<CSSPrimitiveValue> value = static_pointer_cast<CSSPrimitiveValue>(selectionStyle->style()->getPropertyCSSValue(CSSPropertyTextAlign));
    if (!value)
        return;
        
    const char *newValue = nullptr;
    TextAlignMode textAlign = *value;
    switch (textAlign) {
    case TextAlignMode::Start:
    case TextAlignMode::End: {
        switch (direction) {
        case WritingDirection::Natural:
            // no-op
            break;
        case WritingDirection::LeftToRight:
            newValue = "left";
            break;
        case WritingDirection::RightToLeft:
            newValue = "right";
            break;
        }
        break;
    }
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        newValue = "right";
        break;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        newValue = "left";
        break;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
    case TextAlignMode::Justify:
        // no-op
        break;
    }

    if (!newValue)
        return;

    Element* focusedElement = m_document.focusedElement();
    if (focusedElement && (is<HTMLTextAreaElement>(*focusedElement) || (is<HTMLInputElement>(*focusedElement)
        && (downcast<HTMLInputElement>(*focusedElement).isTextField()
            || downcast<HTMLInputElement>(*focusedElement).isSearchField())))) {
        if (direction == WritingDirection::Natural)
            return;
        downcast<HTMLElement>(*focusedElement).setAttributeWithoutSynchronization(alignAttr, newValue);
        m_document.updateStyleIfNeeded();
        return;
    }

    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.ptr());
}

void Editor::removeUnchangeableStyles()
{
    // This function removes styles that the user cannot modify by applying their default values.
    
    auto editingStyle = EditingStyle::create(m_document.bodyOrFrameset());
    auto defaultStyle = editingStyle->style()->mutableCopy();
    
    // Text widgets implement background color via the UIView property. Their body element will not have one.
    defaultStyle->setProperty(CSSPropertyBackgroundColor, "rgba(255, 255, 255, 0.0)");
    
    // Remove properties that the user can modify, like font-weight. 
    // Also remove font-family, per HI spec.
    // FIXME: it'd be nice if knowledge about which styles were unchangeable was not hard-coded here.
    defaultStyle->removeProperty(CSSPropertyFontWeight);
    defaultStyle->removeProperty(CSSPropertyFontStyle);
    defaultStyle->removeProperty(CSSPropertyFontVariantCaps);
    // FIXME: we should handle also pasted quoted text, strikethrough, etc. <rdar://problem/9255115>
    defaultStyle->removeProperty(CSSPropertyTextDecoration);
    defaultStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect); // implements underline

    // FIXME add EditAction::MatchStlye <rdar://problem/9156507> Undo rich text's paste & match style should say "Undo Match Style"
    applyStyleToSelection(defaultStyle.ptr(), EditAction::ChangeAttributes);
}

static void getImage(Element& imageElement, RefPtr<Image>& image, CachedImage*& cachedImage)
{
    auto* renderer = imageElement.renderer();
    if (!is<RenderImage>(renderer))
        return;

    CachedImage* tentativeCachedImage = downcast<RenderImage>(*renderer).cachedImage();
    if (!tentativeCachedImage || tentativeCachedImage->errorOccurred())
        return;

    image = tentativeCachedImage->imageForRenderer(renderer);
    if (!image)
        return;
    
    cachedImage = tentativeCachedImage;
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

    auto imageSourceURL = imageElement.document().completeURL(stripLeadingAndTrailingHTMLSpaces(imageElement.imageSourceURL()));

    auto pasteboardImageURL = url.isEmpty() ? imageSourceURL : url;
    if (!pasteboardImageURL.isLocalFile()) {
        pasteboardImage.url.url = pasteboardImageURL;
        pasteboardImage.url.title = title;
    }
    pasteboardImage.suggestedName = imageSourceURL.lastPathComponent().toString();
    pasteboardImage.imageSize = image->size();
    pasteboardImage.resourceMIMEType = pasteboard.resourceMIMEType(cachedImage->response().mimeType());
    pasteboardImage.resourceData = cachedImage->resourceBuffer();

    if (!pasteboard.isStatic())
        client()->getClientPasteboardData(makeRangeSelectingNode(imageElement), pasteboardImage.clientTypes, pasteboardImage.clientData);

    pasteboard.write(pasteboardImage);
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, OptionSet<PasteOption> options)
{
    auto range = selectedRange();
    bool allowPlainText = options.contains(PasteOption::AllowPlainText);
    WebContentReader reader(*m_document.frame(), *range, allowPlainText);
    int numberOfPasteboardItems = client()->getPasteboardItemsCount();
    for (int i = 0; i < numberOfPasteboardItems; ++i) {
        auto fragment = client()->documentFragmentFromDelegate(i);
        if (!fragment)
            continue;
        reader.addFragment(fragment.releaseNonNull());
    }

    auto fragment = WTFMove(reader.fragment);
    if (!fragment) {
        bool chosePlainTextIgnored;
        fragment = webContentFromPasteboard(*pasteboard, *range, allowPlainText, chosePlainTextIgnored);
    }

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, range, EditorInsertAction::Pasted))
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard), false, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote);
}

void Editor::insertDictationPhrases(Vector<Vector<String>>&& dictationPhrases, id metadata)
{
    if (m_document.selection().isNone())
        return;

    if (dictationPhrases.isEmpty())
        return;

    DictationCommandIOS::create(document(), WTFMove(dictationPhrases), metadata)->apply();
}

void Editor::setDictationPhrasesAsChildOfElement(const Vector<Vector<String>>& dictationPhrases, id metadata, Element& element)
{
    // Clear the composition.
    clear();

    // Clear the Undo stack, since the operations that follow are not Undoable, and will corrupt the stack.
    // Some day we could make them Undoable, and let callers clear the Undo stack explicitly if they wish.
    clearUndoRedoOperations();

    m_document.selection().clear();

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

    auto weakElement = makeWeakPtr(element);

    // We need a layout in order to add markers below.
    document().updateLayout();

    if (!weakElement)
        return;

    if (!element.firstChild()->isTextNode()) {
        // Shouldn't happen.
        ASSERT(element.firstChild()->isTextNode());
        return;
    }

    auto& textNode = downcast<Text>(*element.firstChild());
    unsigned previousDictationPhraseStart = 0;
    for (auto& interpretations : dictationPhrases) {
        auto dictationPhraseLength = interpretations[0].length();
        if (interpretations.size() > 1) {
            auto alternatives = interpretations;
            alternatives.remove(0);
            addMarker(textNode, previousDictationPhraseStart, dictationPhraseLength, DocumentMarker::DictationPhraseWithAlternatives, WTFMove(alternatives));
        }
        previousDictationPhraseStart += dictationPhraseLength;
    }

    addMarker(textNode, 0, textNode.length(), DocumentMarker::DictationResult, retainPtr(metadata));

    client()->respondToChangedContents();
}

void Editor::confirmMarkedText()
{
    // FIXME: This is a hacky workaround for the keyboard calling this method too late -
    // after the selection and focus have already changed. See <rdar://problem/5975559>.
    Element* focused = document().focusedElement();
    Node* composition = compositionNode();
    if (composition && focused && focused != composition && !composition->isDescendantOrShadowDescendantOf(focused)) {
        cancelComposition();
        document().setFocusedElement(focused);
    } else
        confirmComposition();
}

void Editor::setTextAsChildOfElement(const String& text, Element& element)
{
    // Clear the composition
    clear();

    // Clear the Undo stack, since the operations that follow are not Undoable, and will corrupt the stack.
    // Some day we could make them Undoable, and let callers clear the Undo stack explicitly if they wish.
    clearUndoRedoOperations();

    // If the element is empty already and we're not adding text, we can early return and avoid clearing/setting
    // a selection at [0, 0] and the expense involved in creation VisiblePositions.
    if (!element.firstChild() && text.isEmpty())
        return;

    // As a side effect this function sets a caret selection after the inserted content. Much of what
    // follows is more expensive if there is a selection, so clear it since it's going to change anyway.
    m_document.selection().clear();

    // clear out all current children of element
    element.removeChildren();

    if (text.length()) {
        // insert new text
        // remove element from tree while doing it
        // FIXME: The element we're inserting into is often the body element. It seems strange to be removing it
        // (even if it is only temporary). ReplaceSelectionCommand doesn't bother doing this when it inserts
        // content, why should we here?
        RefPtr<Node> parent = element.parentNode();
        RefPtr<Node> siblingAfter = element.nextSibling();
        if (parent)
            element.remove();

        auto context = document().createRange();
        context->selectNodeContents(element);
        element.appendChild(createFragmentFromText(context, text));

        // restore element to document
        if (parent) {
            if (siblingAfter)
                parent->insertBefore(element, siblingAfter.get());
            else
                parent->appendChild(element);
        }
    }

    // set the selection to the end
    VisibleSelection selection;

    Position pos = createLegacyEditingPosition(&element, element.countChildNodes());

    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    if (visiblePos.isNull())
        return;

    selection.setBase(visiblePos);
    selection.setExtent(visiblePos);

    m_document.selection().setSelection(selection);

    client()->respondToChangedContents();
}

// If the selection is adjusted from UIKit without closing the typing, the typing command may
// have a stale selection.
void Editor::ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping()
{
    TypingCommand::ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping(m_document, m_document.selection().selection());
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
