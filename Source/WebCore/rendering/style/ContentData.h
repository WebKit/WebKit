/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CounterContent.h"
#include "StyleImage.h"
#include "RenderPtr.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class Document;
class RenderObject;
class RenderStyle;

class ContentData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Type {
        CounterDataType,
        ImageDataType,
        QuoteDataType,
        TextDataType
    };
    virtual ~ContentData() = default;

    Type type() const { return m_type; }

    bool isCounter() const { return type() == CounterDataType; }
    bool isImage() const { return type() == ImageDataType; }
    bool isQuote() const { return type() == QuoteDataType; }
    bool isText() const { return type() == TextDataType; }

    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const = 0;

    std::unique_ptr<ContentData> clone() const;

    ContentData* next() const { return m_next.get(); }
    void setNext(std::unique_ptr<ContentData> next) { m_next = WTFMove(next); }

    void setAltText(const String& alt) { m_altText = alt; }
    const String& altText() const { return m_altText; }

protected:
    explicit ContentData(Type type)
        : m_type(type)
    {
    }

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const = 0;

    std::unique_ptr<ContentData> m_next;
    String m_altText;
    Type m_type;
};

class ImageContentData final : public ContentData {
public:
    explicit ImageContentData(Ref<StyleImage>&& image)
        : ContentData(ImageDataType)
        , m_image(WTFMove(image))
    {
    }

    const StyleImage& image() const { return m_image.get(); }
    void setImage(Ref<StyleImage>&& image)
    {
        m_image = WTFMove(image);
    }

private:
    RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const final;
    std::unique_ptr<ContentData> cloneInternal() const final
    {
        auto image = makeUnique<ImageContentData>(m_image.copyRef());
        image->setAltText(altText());
        return image;
    }

    Ref<StyleImage> m_image;
};

inline bool operator==(const ImageContentData& a, const ImageContentData& b)
{
    return &a.image() == &b.image();
}

inline bool operator!=(const ImageContentData& a, const ImageContentData& b)
{
    return !(a == b);
}

class TextContentData final : public ContentData {
public:
    explicit TextContentData(const String& text)
        : ContentData(TextDataType)
        , m_text(text)
    {
    }

    const String& text() const { return m_text; }
    void setText(const String& text) { m_text = text; }

private:
    RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const final;
    std::unique_ptr<ContentData> cloneInternal() const final { return makeUnique<TextContentData>(m_text); }

    String m_text;
};

inline bool operator==(const TextContentData& a, const TextContentData& b)
{
    return a.text() == b.text();
}

inline bool operator!=(const TextContentData& a, const TextContentData& b)
{
    return !(a == b);
}

class CounterContentData final : public ContentData {
public:
    explicit CounterContentData(std::unique_ptr<CounterContent> counter)
        : ContentData(CounterDataType)
        , m_counter(WTFMove(counter))
    {
        ASSERT(m_counter);
    }

    const CounterContent& counter() const { return *m_counter; }
    void setCounter(std::unique_ptr<CounterContent> counter)
    {
        ASSERT(counter);
        m_counter = WTFMove(counter);
    }

private:
    RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const final;
    std::unique_ptr<ContentData> cloneInternal() const final
    {
        return makeUnique<CounterContentData>(makeUnique<CounterContent>(counter()));
    }

    std::unique_ptr<CounterContent> m_counter;
};

inline bool operator==(const CounterContentData& a, const CounterContentData& b)
{
    return a.counter() == b.counter();
}

inline bool operator!=(const CounterContentData& a, const CounterContentData& b)
{
    return !(a == b);
}

class QuoteContentData final : public ContentData {
public:
    explicit QuoteContentData(QuoteType quote)
        : ContentData(QuoteDataType)
        , m_quote(quote)
    {
    }

    QuoteType quote() const { return m_quote; }
    void setQuote(QuoteType quote) { m_quote = quote; }

private:
    RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const final;
    std::unique_ptr<ContentData> cloneInternal() const final { return makeUnique<QuoteContentData>(quote()); }

    QuoteType m_quote;
};

inline bool operator==(const QuoteContentData& a, const QuoteContentData& b)
{
    return a.quote() == b.quote();
}

inline bool operator!=(const QuoteContentData& a, const QuoteContentData& b)
{
    return !(a == b);
}

inline bool operator==(const ContentData& a, const ContentData& b)
{
    if (a.type() != b.type())
        return false;

    switch (a.type()) {
    case ContentData::CounterDataType:
        return downcast<CounterContentData>(a) == downcast<CounterContentData>(b);
    case ContentData::ImageDataType:
        return downcast<ImageContentData>(a) == downcast<ImageContentData>(b);
    case ContentData::QuoteDataType:
        return downcast<QuoteContentData>(a) == downcast<QuoteContentData>(b);
    case ContentData::TextDataType:
        return downcast<TextContentData>(a) == downcast<TextContentData>(b);
    }

    ASSERT_NOT_REACHED();
    return false;
}

inline bool operator!=(const ContentData& a, const ContentData& b)
{
    return !(a == b);
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CONTENT_DATA(ToClassName, ContentDataName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::ContentData& contentData) { return contentData.is##ContentDataName(); } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CONTENT_DATA(ImageContentData, Image)
SPECIALIZE_TYPE_TRAITS_CONTENT_DATA(TextContentData, Text)
SPECIALIZE_TYPE_TRAITS_CONTENT_DATA(CounterContentData, Counter)
SPECIALIZE_TYPE_TRAITS_CONTENT_DATA(QuoteContentData, Quote)
