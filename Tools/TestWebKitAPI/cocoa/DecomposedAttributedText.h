/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/ObjectIdentifier.h>
#import <wtf/Variant.h>
#import <wtf/Vector.h>
#import <wtf/text/TextStream.h>
#import <wtf/text/WTFString.h>

@class NSAttributedString;

struct _DecomposedAttributedTextBold;
struct _DecomposedAttributedTextItalic;
struct _DecomposedAttributedTextOrderedList;
struct _DecomposedAttributedTextUnorderedList;

struct DecomposedAttributedText {
    struct ListMarker {
        enum class Type: uint8_t {
            Circle,
            Decimal,
            Disc,
            LowercaseRoman,
        };

        using enum Type;

        ListMarker(Type type)
            : data(type)
        {
        }

        explicit ListMarker(String&& string)
            : data(string)
        {
        }

        Variant<Type, String> data;
    };

    enum class ListIDTag { };
    using ListID = ObjectIdentifier<ListIDTag>;

    using Bold = _DecomposedAttributedTextBold;
    using Italic = _DecomposedAttributedTextItalic;
    using OrderedList = _DecomposedAttributedTextOrderedList;
    using UnorderedList = _DecomposedAttributedTextUnorderedList;

    using Element = Variant<Bold, Italic, OrderedList, UnorderedList, String>;

    Vector<Element> children;
};

struct _DecomposedAttributedTextBold {
    Vector<DecomposedAttributedText::Element> children;

    explicit _DecomposedAttributedTextBold(Vector<DecomposedAttributedText::Element>&& children)
        : children(children)
    {
    }
};

struct _DecomposedAttributedTextItalic {
    Vector<DecomposedAttributedText::Element> children;

    explicit _DecomposedAttributedTextItalic(Vector<DecomposedAttributedText::Element>&& children)
        : children(children)
    {
    }
};

struct _DecomposedAttributedTextOrderedList {
    DecomposedAttributedText::ListID identifier { MarkableTraits<DecomposedAttributedText::ListID>::emptyValue() };
    DecomposedAttributedText::ListMarker marker { DecomposedAttributedText::ListMarker::Decimal };
    int startingItemNumber { 1 };
    Vector<DecomposedAttributedText::Element> children;

    explicit _DecomposedAttributedTextOrderedList(const DecomposedAttributedText::ListID& identifier)
        : identifier(identifier)
    {
    }

    _DecomposedAttributedTextOrderedList(int startingItemNumber, DecomposedAttributedText::ListMarker marker, Vector<DecomposedAttributedText::Element>&& children)
        : marker(marker)
        , startingItemNumber(startingItemNumber)
        , children(children)
    {
    }

    _DecomposedAttributedTextOrderedList(int startingItemNumber, Vector<DecomposedAttributedText::Element>&& children)
        : startingItemNumber(startingItemNumber)
        , children(children)
    {
    }

    explicit _DecomposedAttributedTextOrderedList(Vector<DecomposedAttributedText::Element>&& children)
        : children(children)
    {
    }
};

struct _DecomposedAttributedTextUnorderedList {
    DecomposedAttributedText::ListID identifier { MarkableTraits<DecomposedAttributedText::ListID>::emptyValue() };
    DecomposedAttributedText::ListMarker marker { DecomposedAttributedText::ListMarker::Disc };
    Vector<DecomposedAttributedText::Element> children;

    explicit _DecomposedAttributedTextUnorderedList(const DecomposedAttributedText::ListID& identifier)
        : identifier(identifier)
    {
    }

    _DecomposedAttributedTextUnorderedList(DecomposedAttributedText::ListMarker marker, Vector<DecomposedAttributedText::Element>&& children)
        : marker(marker)
        , children(children)
    {
    }

    explicit _DecomposedAttributedTextUnorderedList(Vector<DecomposedAttributedText::Element>&& children)
        : children(children)
    {
    }
};

DecomposedAttributedText decompose(NSAttributedString *);

TextStream& operator<<(TextStream&, const DecomposedAttributedText::ListMarker&);

TextStream& operator<<(TextStream&, const DecomposedAttributedText::Bold&);

TextStream& operator<<(TextStream&, const DecomposedAttributedText::Italic&);

TextStream& operator<<(TextStream&, const DecomposedAttributedText::OrderedList&);

TextStream& operator<<(TextStream&, const DecomposedAttributedText::UnorderedList&);

TextStream& operator<<(TextStream&, const DecomposedAttributedText&);

bool operator==(const DecomposedAttributedText::ListMarker&, const DecomposedAttributedText::ListMarker&);

bool operator==(const DecomposedAttributedText::Bold&, const DecomposedAttributedText::Bold&);

bool operator==(const DecomposedAttributedText::Italic&, const DecomposedAttributedText::Italic&);

bool operator==(const DecomposedAttributedText::OrderedList&, const DecomposedAttributedText::OrderedList&);

bool operator==(const DecomposedAttributedText::UnorderedList&, const DecomposedAttributedText::UnorderedList&);

bool operator==(const DecomposedAttributedText&, const DecomposedAttributedText&);


