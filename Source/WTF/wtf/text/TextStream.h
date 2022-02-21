/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WTF {

class TextStream {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct FormatNumberRespectingIntegers {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        FormatNumberRespectingIntegers(double number)
            : value(number) { }

        double value;
    };
    
    enum class Formatting : uint8_t {
        SVGStyleRect                = 1 << 0, // "at (0,0) size 10x10"
        NumberRespectingIntegers    = 1 << 1,
        LayoutUnitsAsIntegers       = 1 << 2,
    };

    enum class LineMode { SingleLine, MultipleLine };
    TextStream(LineMode lineMode = LineMode::MultipleLine, OptionSet<Formatting> formattingFlags = { }, unsigned containerSizeLimit = 0)
        : m_formattingFlags(formattingFlags)
        , m_multiLineMode(lineMode == LineMode::MultipleLine)
        , m_containerSizeLimit(containerSizeLimit)
    {
    }

    WTF_EXPORT_PRIVATE TextStream& operator<<(bool);
    WTF_EXPORT_PRIVATE TextStream& operator<<(char);
    WTF_EXPORT_PRIVATE TextStream& operator<<(int);
    WTF_EXPORT_PRIVATE TextStream& operator<<(unsigned);
    WTF_EXPORT_PRIVATE TextStream& operator<<(long);
    WTF_EXPORT_PRIVATE TextStream& operator<<(unsigned long);
    WTF_EXPORT_PRIVATE TextStream& operator<<(long long);

    WTF_EXPORT_PRIVATE TextStream& operator<<(unsigned long long);
    WTF_EXPORT_PRIVATE TextStream& operator<<(float);
    WTF_EXPORT_PRIVATE TextStream& operator<<(double);
    WTF_EXPORT_PRIVATE TextStream& operator<<(const char*);
    WTF_EXPORT_PRIVATE TextStream& operator<<(const void*);
    WTF_EXPORT_PRIVATE TextStream& operator<<(const AtomString&);
    WTF_EXPORT_PRIVATE TextStream& operator<<(const String&);
    WTF_EXPORT_PRIVATE TextStream& operator<<(StringView);
    // Deprecated. Use the NumberRespectingIntegers FormattingFlag instead.
    WTF_EXPORT_PRIVATE TextStream& operator<<(const FormatNumberRespectingIntegers&);

#if PLATFORM(COCOA)
    WTF_EXPORT_PRIVATE TextStream& operator<<(id);
#ifdef __OBJC__
    WTF_EXPORT_PRIVATE TextStream& operator<<(NSArray *);
#endif
#endif

    OptionSet<Formatting> formattingFlags() const { return m_formattingFlags; }
    void setFormattingFlags(OptionSet<Formatting> flags) { m_formattingFlags = flags; }

    bool hasFormattingFlag(Formatting flag) const { return m_formattingFlags.contains(flag); }

    template<typename T>
    void dumpProperty(const String& name, const T& value)
    {
        TextStream& ts = *this;
        ts.startGroup();
        ts << name << " " << value;
        ts.endGroup();
    }

    WTF_EXPORT_PRIVATE String release();
    
    WTF_EXPORT_PRIVATE void startGroup();
    WTF_EXPORT_PRIVATE void endGroup();
    WTF_EXPORT_PRIVATE void nextLine(); // Output newline and indent.

    int indent() const { return m_indent; }
    void setIndent(int indent) { m_indent = indent; }
    void increaseIndent(int amount = 1) { m_indent += amount; }
    void decreaseIndent(int amount = 1) { m_indent -= amount; ASSERT(m_indent >= 0); }

    WTF_EXPORT_PRIVATE void writeIndent();

    unsigned containerSizeLimit() const { return m_containerSizeLimit; }

    // Stream manipulators.
    TextStream& operator<<(TextStream& (*func)(TextStream&))
    {
        return (*func)(*this);
    }

    struct Repeat {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Repeat(unsigned inWidth, char inCharacter)
            : width(inWidth), character(inCharacter)
        { }
        unsigned width { 0 };
        char character { ' ' };
    };

    TextStream& operator<<(const Repeat& repeated)
    {
        for (unsigned i = 0; i < repeated.width; ++i)
            m_text.append(repeated.character);

        return *this;
    }

    class IndentScope {
    public:
        IndentScope(TextStream& ts, int amount = 1)
            : m_stream(ts)
            , m_amount(amount)
        {
            m_stream.increaseIndent(m_amount);
        }
        ~IndentScope()
        {
            m_stream.decreaseIndent(m_amount);
        }

    private:
        TextStream& m_stream;
        int m_amount;
    };

    class GroupScope {
    public:
        GroupScope(TextStream& ts)
            : m_stream(ts)
        {
            m_stream.startGroup();
        }
        ~GroupScope()
        {
            m_stream.endGroup();
        }

    private:
        TextStream& m_stream;
    };

private:
    StringBuilder m_text;
    int m_indent { 0 };
    OptionSet<Formatting> m_formattingFlags;
    bool m_multiLineMode { true };
    unsigned m_containerSizeLimit { 0 };
};

inline TextStream& indent(TextStream& ts)
{
    ts.writeIndent();
    return ts;
}

template<typename T>
struct ValueOrNull {
    explicit ValueOrNull(T* inValue)
        : value(inValue)
    { }
    T* value;
};

template<typename T>
TextStream& operator<<(TextStream& ts, ValueOrNull<T> item)
{
    if (item.value)
        ts << *item.value;
    else
        ts << "null";
    return ts;
}

template<typename Item>
TextStream& operator<<(TextStream& ts, const std::optional<Item>& item)
{
    if (item)
        return ts << item.value();
    
    return ts << "nullopt";
}

template<typename T, typename Traits>
TextStream& operator<<(TextStream& ts, const Markable<T, Traits>& item)
{
    if (item)
        return ts << item.value();
    
    return ts << "unset";
}

template<typename ItemType, size_t inlineCapacity>
TextStream& operator<<(TextStream& ts, const Vector<ItemType, inlineCapacity>& vector)
{
    ts << "[";

    unsigned count = 0;
    for (const auto& value : vector) {
        if (count)
            ts << ", ";
        ts << value;
        if (++count == ts.containerSizeLimit())
            break;
    }

    if (count != vector.size())
        ts << ", ...";

    return ts << "]";
}

template<typename T>
TextStream& operator<<(TextStream& ts, const WeakPtr<T>& item)
{
    if (item)
        return ts << *item;
    
    return ts << "null";
}

template<typename T>
TextStream& operator<<(TextStream& ts, const RefPtr<T>& item)
{
    if (item)
        return ts << *item;
    
    return ts << "null";
}

template<typename T>
TextStream& operator<<(TextStream& ts, const Ref<T>& item)
{
    return ts << item.get();
}

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
TextStream& operator<<(TextStream& ts, const HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>& map)
{
    ts << "{";

    unsigned count = 0;
    for (const auto& keyValuePair : map) {
        if (count)
            ts << ", ";
        ts << keyValuePair.key << ": " << keyValuePair.value;
        if (++count == ts.containerSizeLimit())
            break;
    }

    if (count != map.size())
        ts << ", ...";

    return ts << "}";
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
TextStream& operator<<(TextStream& ts, const HashSet<ValueArg, HashArg, TraitsArg>& set)
{
    ts << "[";

    unsigned count = 0;
    for (const auto& item : set) {
        if (count)
            ts << ", ";
        ts << item;
        if (++count == ts.containerSizeLimit())
            break;
    }

    if (count != set.size())
        ts << ", ...";

    return ts << "]";
}

template<typename Option>
TextStream& operator<<(TextStream& ts, const OptionSet<Option>& options)
{
    ts << "[";
    bool needComma = false;
    for (auto option : options) {
        if (needComma)
            ts << ", ";
        needComma = true;
        ts << option;
    }
    return ts << "]";
}

template<typename, typename = void, typename = void, typename = void, typename = void, size_t = 0>
struct supports_text_stream_insertion : std::false_type { };

template<typename T>
struct supports_text_stream_insertion<T, std::void_t<decltype(std::declval<TextStream&>() << std::declval<T>())>> : std::true_type { };

template<typename ItemType, size_t inlineCapacity>
struct supports_text_stream_insertion<Vector<ItemType, inlineCapacity>> : supports_text_stream_insertion<ItemType> { };

template<typename ValueArg, typename HashArg, typename TraitsArg>
struct supports_text_stream_insertion<HashSet<ValueArg, HashArg, TraitsArg>> : supports_text_stream_insertion<ValueArg> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
struct supports_text_stream_insertion<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>> : std::conjunction<supports_text_stream_insertion<KeyArg>, supports_text_stream_insertion<MappedArg>> { };

template<typename T, typename Traits>
struct supports_text_stream_insertion<Markable<T, Traits>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<OptionSet<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<std::optional<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<WeakPtr<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<RefPtr<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct supports_text_stream_insertion<Ref<T>> : supports_text_stream_insertion<T> { };

template<typename T>
struct ValueOrEllipsis {
    explicit ValueOrEllipsis(const T& value)
        : value(value)
    { }
    const T& value;
};

template<typename T>
TextStream& operator<<(TextStream& ts, ValueOrEllipsis<T> item)
{
    if constexpr (supports_text_stream_insertion<T>::value)
        ts << item.value;
    else
        ts << "...";
    return ts;
}

// Deprecated. Use TextStream::writeIndent() instead.
WTF_EXPORT_PRIVATE void writeIndent(TextStream&, int indent);

} // namespace WTF

using WTF::TextStream;
using WTF::ValueOrEllipsis;
using WTF::ValueOrNull;
using WTF::indent;
