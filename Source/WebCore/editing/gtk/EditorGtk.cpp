/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
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

#include "config.h"
#include "Editor.h"

#include "CachedImage.h"
#include "DataObjectGtk.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Pasteboard.h"
#include "RenderImage.h"
#include "SVGElement.h"
#include "XLinkNames.h"
#include "markup.h"

namespace WebCore {

static PassRefPtr<DocumentFragment> createFragmentFromPasteboardData(Pasteboard& pasteboard, Frame& frame, Range& range, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    if (!pasteboard.hasData())
        return nullptr;

    RefPtr<DataObjectGtk> dataObject = pasteboard.dataObject();
    if (dataObject->hasMarkup() && frame.document()) {
        if (RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(*frame.document(), dataObject->markup(), emptyString(), DisallowScriptingAndPluginContent))
            return fragment.release();
    }

    if (!allowPlainText)
        return nullptr;

    if (dataObject->hasText()) {
        chosePlainText = true;
        if (RefPtr<DocumentFragment> fragment = createFragmentFromText(range, dataObject->text()))
            return fragment.release();
    }

    return nullptr;
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText, MailBlockquoteHandling mailBlockquoteHandling)
{
    RefPtr<Range> range = selectedRange();
    if (!range)
        return;

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = createFragmentFromPasteboardData(*pasteboard, m_frame, *range, allowPlainText, chosePlainText);
    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(*pasteboard), chosePlainText, mailBlockquoteHandling);
}

static const AtomicString& elementURL(Element& element)
{
    if (isHTMLImageElement(element) || isHTMLInputElement(element))
        return element.fastGetAttribute(HTMLNames::srcAttr);
    if (isSVGImageElement(element))
        return element.fastGetAttribute(XLinkNames::hrefAttr);
    if (isHTMLEmbedElement(element) || isHTMLObjectElement(element))
        return element.imageSourceURL();
    return nullAtom;
}

static bool getImageForElement(Element& element, RefPtr<Image>& image)
{
    auto renderer = element.renderer();
    if (!renderer || !renderer->isRenderImage())
        return false;

    CachedImage* cachedImage = toRenderImage(*renderer).cachedImage();
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
    pasteboardImage.url.markup = createMarkup(imageElement, IncludeNode, nullptr, ResolveAllURLs);
    pasteboard.write(pasteboardImage);
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    PasteboardWebContent pasteboardContent;
    pasteboardContent.canSmartCopyOrDelete = canSmartCopyOrDelete();
    pasteboardContent.text = selectedTextForDataTransfer();
    pasteboardContent.markup = createMarkup(*selectedRange(), nullptr, AnnotateForInterchange, false, ResolveNonLocalURLs);
    pasteboardContent.callback = nullptr;
    pasteboard.write(pasteboardContent);
}

} // namespace WebCore
