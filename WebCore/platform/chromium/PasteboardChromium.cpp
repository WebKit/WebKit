/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "Pasteboard.h"

#include "ChromiumBridge.h"
#include "ClipboardUtilitiesChromium.h"
#include "DocumentFragment.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Image.h"
#include "KURL.h"
#include "markup.h"
#include "NativeImageSkia.h"
#include "Range.h"
#include "RenderImage.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
    : m_selectionMode(false)
{
}

void Pasteboard::clear()
{
    // The ScopedClipboardWriter class takes care of clearing the clipboard's
    // previous contents.
}

bool Pasteboard::isSelectionMode() const
{
    return m_selectionMode;
}

void Pasteboard::setSelectionMode(bool selectionMode)
{
    m_selectionMode = selectionMode;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    String html = createMarkup(selectedRange, 0, AnnotateForInterchange);
#if PLATFORM(DARWIN)
    html = String("<meta charset='utf-8'>") + html;
#endif
    ExceptionCode ec = 0;
    KURL url = selectedRange->startContainer(ec)->document()->url();
    String plainText = frame->selectedText();
#if PLATFORM(WIN_OS)
    replaceNewlinesWithWindowsStyleNewlines(plainText);
#endif
    replaceNBSPWithSpace(plainText);

    ChromiumBridge::clipboardWriteSelection(html, url, plainText, canSmartCopyOrDelete);
}

void Pasteboard::writePlainText(const String& text)
{
#if PLATFORM(WIN_OS)
    String plainText(text);
    replaceNewlinesWithWindowsStyleNewlines(plainText);
    ChromiumBridge::clipboardWritePlainText(plainText);
#else
    ChromiumBridge::clipboardWritePlainText(text);
#endif
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    ASSERT(!url.isEmpty());

    String title(titleStr);
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    ChromiumBridge::clipboardWriteURL(url, title);
}

void Pasteboard::writeImage(Node* node, const KURL&, const String& title)
{
    ASSERT(node);
    ASSERT(node->renderer());
    ASSERT(node->renderer()->isImage());
    RenderImage* renderer = toRenderImage(node->renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    // If the image is wrapped in a link, |url| points to the target of the
    // link.  This isn't useful to us, so get the actual image URL.
    AtomicString urlString;
    if (node->hasTagName(HTMLNames::imgTag) || node->hasTagName(HTMLNames::inputTag))
        urlString = static_cast<Element*>(node)->getAttribute(HTMLNames::srcAttr);
#if ENABLE(SVG)
    else if (node->hasTagName(SVGNames::imageTag))
        urlString = static_cast<Element*>(node)->getAttribute(XLinkNames::hrefAttr);
#endif
    else if (node->hasTagName(HTMLNames::embedTag) || node->hasTagName(HTMLNames::objectTag)) {
        Element* element = static_cast<Element*>(node);
        urlString = element->getAttribute(element->imageSourceAttributeName());
    }
    KURL url = urlString.isEmpty() ? KURL() : node->document()->completeURL(deprecatedParseURL(urlString));

    NativeImagePtr bitmap = image->nativeImageForCurrentFrame();
    ChromiumBridge::clipboardWriteImage(bitmap, url, title);
}

bool Pasteboard::canSmartReplace()
{
    return ChromiumBridge::clipboardIsFormatAvailable(PasteboardPrivate::WebSmartPasteFormat, m_selectionMode ? PasteboardPrivate::SelectionBuffer : PasteboardPrivate::StandardBuffer);
}

String Pasteboard::plainText(Frame* frame)
{
    return ChromiumBridge::clipboardReadPlainText(m_selectionMode ? PasteboardPrivate::SelectionBuffer : PasteboardPrivate::StandardBuffer);
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    PasteboardPrivate::ClipboardBuffer buffer = m_selectionMode ? PasteboardPrivate::SelectionBuffer : PasteboardPrivate::StandardBuffer;

    if (ChromiumBridge::clipboardIsFormatAvailable(PasteboardPrivate::HTMLFormat, buffer)) {
        String markup;
        KURL srcURL;
        ChromiumBridge::clipboardReadHTML(buffer, &markup, &srcURL);

        RefPtr<DocumentFragment> fragment =
            createFragmentFromMarkup(frame->document(), markup, srcURL);
        if (fragment)
            return fragment.release();
    }

    if (allowPlainText) {
        String markup = ChromiumBridge::clipboardReadPlainText(buffer);
        if (!markup.isEmpty()) {
            chosePlainText = true;

            RefPtr<DocumentFragment> fragment =
                createFragmentFromText(context.get(), markup);
            if (fragment)
                return fragment.release();
        }
    }

    return 0;
}

} // namespace WebCore
