/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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

#ifndef ContentData_h
#define ContentData_h

#include "CounterContent.h"
#include "StyleImage.h"
#include "RenderPtr.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class RenderObject;
class RenderStyle;

class ContentData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ContentData() { }

    virtual bool isCounter() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isQuote() const { return false; }
    virtual bool isText() const { return false; }

    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const = 0;

    std::unique_ptr<ContentData> clone() const;

    ContentData* next() const { return m_next.get(); }
    void setNext(std::unique_ptr<ContentData> next) { m_next = std::move(next); }

    void setAltText(const String& alt) { m_altText = alt; }
    const String& altText() const { return m_altText; }
    
    virtual bool equals(const ContentData&) const = 0;

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const = 0;

    std::unique_ptr<ContentData> m_next;
    String m_altText;
};

class ImageContentData final : public ContentData {
public:
    explicit ImageContentData(PassRefPtr<StyleImage> image)
        : m_image(image)
    {
    }

    const StyleImage* image() const { return m_image.get(); }
    StyleImage* image() { return m_image.get(); }
    void setImage(PassRefPtr<StyleImage> image) { m_image = image; }

    virtual bool isImage() const override { return true; }
    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const override;

    virtual bool equals(const ContentData& data) const override
    {
        if (!data.isImage())
            return false;
        return *static_cast<const ImageContentData&>(data).image() == *image();
    }

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const override
    {
        RefPtr<StyleImage> image = const_cast<StyleImage*>(this->image());

        return std::make_unique<ImageContentData>(image.release());
    }

    RefPtr<StyleImage> m_image;
};

class TextContentData final : public ContentData {
public:
    explicit TextContentData(const String& text)
        : m_text(text)
    {
    }

    const String& text() const { return m_text; }
    void setText(const String& text) { m_text = text; }

    virtual bool isText() const override { return true; }
    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const override;

    virtual bool equals(const ContentData& data) const override
    {
        if (!data.isText())
            return false;
        return static_cast<const TextContentData&>(data).text() == text();
    }

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const override { return std::make_unique<TextContentData>(text()); }

    String m_text;
};

class CounterContentData final : public ContentData {
public:
    explicit CounterContentData(std::unique_ptr<CounterContent> counter)
        : m_counter(std::move(counter))
    {
    }

    const CounterContent* counter() const { return m_counter.get(); }
    void setCounter(std::unique_ptr<CounterContent> counter) { m_counter = std::move(counter); }

    virtual bool isCounter() const override { return true; }
    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const override;

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const override
    {
        auto counterData = std::make_unique<CounterContent>(*counter());
        return std::make_unique<CounterContentData>(std::move(counterData));
    }

    virtual bool equals(const ContentData& data) const override
    {
        if (!data.isCounter())
            return false;
        return *static_cast<const CounterContentData&>(data).counter() == *counter();
    }

    std::unique_ptr<CounterContent> m_counter;
};

class QuoteContentData final : public ContentData {
public:
    explicit QuoteContentData(QuoteType quote)
        : m_quote(quote)
    {
    }

    QuoteType quote() const { return m_quote; }
    void setQuote(QuoteType quote) { m_quote = quote; }

    virtual bool isQuote() const override { return true; }
    virtual RenderPtr<RenderObject> createContentRenderer(Document&, const RenderStyle&) const override;

    virtual bool equals(const ContentData& data) const override
    {
        if (!data.isQuote())
            return false;
        return static_cast<const QuoteContentData&>(data).quote() == quote();
    }

private:
    virtual std::unique_ptr<ContentData> cloneInternal() const override { return std::make_unique<QuoteContentData>(quote()); }

    QuoteType m_quote;
};

inline bool operator==(const ContentData& a, const ContentData& b)
{
    return a.equals(b);
}

inline bool operator!=(const ContentData& a, const ContentData& b)
{
    return !(a == b);
}

} // namespace WebCore

#endif // ContentData_h
