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

#include "ClipboardChromium.h"
#include "ClipboardUtilitiesChromium.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Image.h"
#include "KURL.h"
#include "NativeImageSkia.h"
#include "Range.h"
#include "RenderImage.h"
#include "markup.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

#include <public/Platform.h>
#include <public/WebClipboard.h>
#include <public/WebDragData.h>

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
    String html = createMarkup(selectedRange, 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
    ExceptionCode ec = 0;
    KURL url = selectedRange->startContainer(ec)->document()->url();
    String plainText = frame->editor()->selectedText();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(plainText);
#endif
    replaceNBSPWithSpace(plainText);

    WebKit::Platform::current()->clipboard()->writeHTML(html, url, plainText, canSmartCopyOrDelete);
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    // FIXME: add support for smart replace
#if OS(WINDOWS)
    String plainText(text);
    replaceNewlinesWithWindowsStyleNewlines(plainText);
    WebKit::Platform::current()->clipboard()->writePlainText(plainText);
#else
    WebKit::Platform::current()->clipboard()->writePlainText(text);
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

    WebKit::Platform::current()->clipboard()->writeURL(url, title);
}

void Pasteboard::writeImage(Node* node, const KURL&, const String& title)
{
    ASSERT(node);

    if (!(node->renderer() && node->renderer()->isImage()))
        return;

    RenderImage* renderer = toRenderImage(node->renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->imageForRenderer(renderer);
    ASSERT(image);

    NativeImagePtr bitmap = image->nativeImageForCurrentFrame();
    if (!bitmap)
        return;

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
    KURL url = urlString.isEmpty() ? KURL() : node->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
    WebKit::WebImage webImage = bitmap->bitmap();
    WebKit::Platform::current()->clipboard()->writeImage(webImage, WebKit::WebURL(url), WebKit::WebString(title));
}

void Pasteboard::writeClipboard(Clipboard* clipboard)
{
    WebKit::WebDragData dragData = static_cast<ClipboardChromium*>(clipboard)->dataObject();
    WebKit::Platform::current()->clipboard()->writeDataObject(dragData);
}

bool Pasteboard::canSmartReplace()
{
    return WebKit::Platform::current()->clipboard()->isFormatAvailable(WebKit::WebClipboard::FormatSmartPaste, m_selectionMode ? WebKit::WebClipboard::BufferSelection : WebKit::WebClipboard::BufferStandard);
}

String Pasteboard::plainText(Frame* frame)
{
    return WebKit::Platform::current()->clipboard()->readPlainText(m_selectionMode ? WebKit::WebClipboard::BufferSelection : WebKit::WebClipboard::BufferStandard);
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    WebKit::WebClipboard::Buffer buffer = m_selectionMode ? WebKit::WebClipboard::BufferSelection : WebKit::WebClipboard::BufferStandard;

    if (WebKit::Platform::current()->clipboard()->isFormatAvailable(WebKit::WebClipboard::FormatHTML, buffer)) {
        WebKit::WebString markup;
        unsigned fragmentStart = 0;
        unsigned fragmentEnd = 0;
        WebKit::WebURL url;
        markup = WebKit::Platform::current()->clipboard()->readHTML(buffer, &url, &fragmentStart, &fragmentEnd);
        if (!markup.isEmpty()) {
          RefPtr<DocumentFragment> fragment =
              createFragmentFromMarkupWithContext(frame->document(), markup, fragmentStart, fragmentEnd, KURL(url), DisallowScriptingContent);
          if (fragment)
              return fragment.release();
        }
    }

    if (allowPlainText) {
        String markup = WebKit::Platform::current()->clipboard()->readPlainText(buffer);
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
