/*
 * Copyright (C) 2014 Haiku, Inc.
 *
 * All rights reserved.
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

#include "config.h"
#include "Editor.h"

#include "DOMURL.h"
#include "DocumentFragment.h"
#include "HTMLEmbedElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "RenderImage.h"
#include "SVGImageElement.h"
#include "XLinkNames.h"
#include "markup.h"


namespace WebCore {

void Editor::pasteWithPasteboard(Pasteboard* pasteboard,
    WTF::OptionSet<WebCore::Editor::PasteOption> options)
{
    auto range = selectedRange();
    if (!range)
        return;

    bool chosePlainText;
    auto fragment = webContentFromPasteboard(*pasteboard,
        *range, options.contains(PasteOption::AllowPlainText), chosePlainText);

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, *range,
            EditorInsertAction::Pasted))
    {
        pasteAsFragment(fragment.releaseNonNull(), canSmartReplaceWithPasteboard(*pasteboard),
            chosePlainText, options.contains(PasteOption::IgnoreMailBlockquote) ?
                MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote);
    }
}

static const AtomString& elementURL(Element& element)
{
    if (is<HTMLImageElement>(element) || is<HTMLInputElement>(element))
        return element.attributeWithoutSynchronization(HTMLNames::srcAttr);
    if (is<SVGImageElement>(element))
        return element.attributeWithoutSynchronization(XLinkNames::hrefAttr);
    if (is<HTMLEmbedElement>(element) || is<HTMLObjectElement>(element))
        return element.imageSourceURL();
    return nullAtom();
}

static bool getImageForElement(Element& element, RefPtr<Image>& image)
{
    auto* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return false;

    CachedImage* cachedImage = downcast<RenderImage>(*renderer).cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return false;

    image = cachedImage->imageForRenderer(renderer);
    return image;
}

void Editor::writeImageToPasteboard(Pasteboard& pasteboard, Element& imageElement, const URL&, const String& title)
{
    PasteboardImage pasteboardImage;

    if (!getImageForElement(imageElement, pasteboardImage.image))
        return;
    ASSERT(pasteboardImage.image);

    pasteboardImage.url.url = imageElement.document().completeURL(stripLeadingAndTrailingHTMLSpaces(elementURL(imageElement)));
    pasteboardImage.url.title = title;
    //pasteboardImage.url.markup = createMarkup(imageElement, IncludeNode, nullptr, ResolveAllURLs);
    pasteboard.write(pasteboardImage);

	notImplemented();
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    PasteboardWebContent pasteboardContent;
    pasteboardContent.text = selectedTextForDataTransfer();
    pasteboardContent.markup = serializePreservingVisualAppearance(
		*selectedRange(), nullptr, AnnotateForInterchange::Yes);
    pasteboard.write(pasteboardContent);
}

RefPtr<DocumentFragment> Editor::webContentFromPasteboard(Pasteboard&, const SimpleRange&, bool /*allowPlainText*/, bool& /*chosePlainText*/)
{
    return nullptr;
}

} // namespace WebCore

