/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WKTextExtractionTestingHelpers.h"

#import "_WKTextExtractionInternal.h"
#import <objc/runtime.h>
#import <wtf/Scope.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>
#import <wtf/text/WTFString.h>
#import <wtf/text/cf/StringConcatenateCF.h>

namespace WTR {

#define downcastItem(item, className) \
    ([item isKindOfClass:objc_getClass(#className)] ? (className *)item : nil)

ASCIILiteral description(WKTextExtractionContainer container)
{
    switch (container) {
    case WKTextExtractionContainerRoot:
        return "ROOT"_s;
    case WKTextExtractionContainerViewportConstrained:
        return "VIEWPORTCONSTRAINED"_s;
    case WKTextExtractionContainerList:
        return "LIST"_s;
    case WKTextExtractionContainerListItem:
        return "LISTITEM"_s;
    case WKTextExtractionContainerBlockQuote:
        return "BLOCKQUOTE"_s;
    case WKTextExtractionContainerArticle:
        return "ARTICLE"_s;
    case WKTextExtractionContainerSection:
        return "SECTION"_s;
    case WKTextExtractionContainerNav:
        return "NAV"_s;
    case WKTextExtractionContainerButton:
        return "BUTTON"_s;
    case WKTextExtractionContainerCanvas:
        return "CANVAS"_s;
    case WKTextExtractionContainerGeneric:
        return "GENERIC"_s;
    }
}

static String descriptionForRect(CGRect rect)
{
    auto roundedInt = [](CGFloat value) {
        return static_cast<int>(std::round(value));
    };
    return makeString('[', roundedInt(rect.origin.x), ',', roundedInt(rect.origin.y), ',', roundedInt(rect.size.width), ',', roundedInt(rect.size.height), ']');
}

static void buildDescriptionIgnoringChildren(TextStream& stream, WKTextExtractionItem *item, IncludeRects includeRects)
{
    auto addRectIfNeeded = makeScopeExit([&] {
        if (includeRects == IncludeRects::Yes)
            stream << " rect="_s << descriptionForRect(item.rectInWebView);
    });

    if (auto container = downcastItem(item, WKTextExtractionContainerItem)) {
        stream << description(container.container);
        return;
    }

    if (auto text = downcastItem(item, WKTextExtractionTextItem)) {
        stream << "TEXT "_s << makeString("\""_s, makeStringByReplacingAll(text.content, '\n', "\\n"_s), "\""_s);
        if (auto range = text.selectedRange; range.location != NSNotFound)
            stream << " {selected=["_s << range.location << ","_s << (range.location + range.length) << "]}";

        if (WKTextExtractionEditable *editable = text.editable) {
            stream << " {editable"_s;
            if (editable.label.length)
                stream << " label=\""_s << editable.label << "\""_s;
            if (editable.placeholder.length)
                stream << " placeholder=\""_s << editable.placeholder << "\""_s;
            if (editable.secure)
                stream << " (secure)"_s;
            if (editable.focused)
                stream << " (focused)"_s;
            stream << "}"_s;
        }

        if (text.links.count) {
            stream << " {"_s;
            bool isFirstLink = true;
            for (WKTextExtractionLink *link in text.links) {
                if (!std::exchange(isFirstLink, false))
                    stream << ", "_s;
                stream << "<"_s << link.url.absoluteString << "> in [" <<
                    link.range.location << ","_s << (link.range.location + link.range.length) << "]";
            }
            stream << "}"_s;
        }
        return;
    }

    if (auto scrollable = downcastItem(item, WKTextExtractionScrollableItem)) {
        auto contentSize = scrollable.contentSize;
        stream << makeString("SCROLLER contentSize={"_s, contentSize.width, ", "_s, contentSize.height, "}"_s);
        return;
    }

    if (auto image = downcastItem(item, WKTextExtractionImageItem)) {
        stream << makeString("IMAGE \""_s, image.name ?: @"", "\", altText=\""_s, image.altText ?: @"", "\""_s);
        return;
    }

    ASSERT_NOT_REACHED();
}

static void buildRecursiveDescription(TextStream& stream, WKTextExtractionItem *item, IncludeRects includeRects)
{
    TextStream::GroupScope scope(stream);
    buildDescriptionIgnoringChildren(stream, item, includeRects);
    for (WKTextExtractionItem *child in item.children)
        buildRecursiveDescription(stream, child, includeRects);
}

NSString *recursiveDescription(WKTextExtractionItem *item, IncludeRects includeRects)
{
    TextStream stream { TextStream::LineMode::MultipleLine };
    buildRecursiveDescription(stream, item, includeRects);
    return stream.release().createNSString().autorelease();
}

} // namespace WTR
