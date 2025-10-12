/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <WebCore/PathOperation.h>

namespace WebCore {
namespace Style {

struct OffsetPath;
struct ClipPath;

enum SupportRayPathOperation : bool { No, Yes };

// <ray-path> = <ray()> || <coord-box>
struct RayPath {
    explicit RayPath(Ref<RayPathOperation>&& operation) : operation { WTFMove(operation) } { }
    explicit RayPath(const Ref<RayPathOperation>& operation) : operation { operation } { }

    ALWAYS_INLINE const RayFunction& ray() const { return operation->ray(); }
    ALWAYS_INLINE CSSBoxType referenceBox() const { return operation->referenceBox(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (referenceBox() == CSSBoxType::BoxMissing)
            return visitor(ray());
        return visitor(SpaceSeparatedTuple { ray(), referenceBox() });
    }

    bool operator==(const RayPath& other) const
    {
        return arePointingToEqualData(operation, other.operation);
    }

private:
    friend ClipPath;
    friend OffsetPath;
    friend std::optional<WebCore::Path> tryPath(const RayPath&, const TransformOperationData&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const RayPath&);

    Ref<RayPathOperation> operation;
};

// <reference-path> = <url> || <coord-box>
struct ReferencePath {
    explicit ReferencePath(Ref<ReferencePathOperation>&& operation) : operation { WTFMove(operation) } { }
    explicit ReferencePath(const Ref<ReferencePathOperation>& operation) : operation { operation } { }

    ALWAYS_INLINE const URL& url() const { return operation->url(); }
    ALWAYS_INLINE const AtomString& fragment() const { return operation->fragment(); }
    ALWAYS_INLINE CSSBoxType referenceBox() const { return operation->referenceBox(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (referenceBox() == CSSBoxType::BoxMissing)
            return visitor(url());
        return visitor(SpaceSeparatedTuple { url(), referenceBox() });
    }

    bool operator==(const ReferencePath& other) const
    {
        return arePointingToEqualData(operation, other.operation);
    }

private:
    friend ClipPath;
    friend OffsetPath;
    friend std::optional<WebCore::Path> tryPath(const ReferencePath&, const TransformOperationData&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ReferencePath&);

    Ref<ReferencePathOperation> operation;
};

// <basic-shape-path> = <basic-shape> || <coord-box>
struct BasicShapePath {
    explicit BasicShapePath(Ref<ShapePathOperation>&& operation) : operation { WTFMove(operation) } { }
    explicit BasicShapePath(const Ref<ShapePathOperation>& operation) : operation { operation } { }

    ALWAYS_INLINE const BasicShape& shape() const { return operation->shape(); }
    ALWAYS_INLINE CSSBoxType referenceBox() const { return operation->referenceBox(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (referenceBox() == CSSBoxType::BoxMissing)
            return visitor(shape());
        return visitor(SpaceSeparatedTuple { shape(), referenceBox() });
    }

    bool operator==(const BasicShapePath& other) const
    {
        return arePointingToEqualData(operation, other.operation);
    }

private:
    friend ClipPath;
    friend OffsetPath;
    friend std::optional<WebCore::Path> tryPath(const BasicShapePath&, const TransformOperationData&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const BasicShapePath&);

    Ref<ShapePathOperation> operation;
};

// <box-path> = <coord-box>
struct BoxPath {
    explicit BoxPath(Ref<BoxPathOperation>&& operation) : operation { WTFMove(operation) } { }
    explicit BoxPath(const Ref<BoxPathOperation>& operation) : operation { operation } { }

    ALWAYS_INLINE CSSBoxType referenceBox() const { return operation->referenceBox(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        return visitor(referenceBox());
    }

    bool operator==(const BoxPath& other) const
    {
        return operation.get() == other.operation.get();
    }

private:
    friend ClipPath;
    friend OffsetPath;
    friend std::optional<WebCore::Path> tryPath(const BoxPath&, const TransformOperationData&);

    Ref<BoxPathOperation> operation;
};

inline std::optional<WebCore::Path> tryPath(const RayPath& rayPath, const TransformOperationData& data)
{
    Ref operation = rayPath.operation;
    return operation->getPath(data);
}

inline std::optional<WebCore::Path> tryPath(const ReferencePath& referencePath, const TransformOperationData& data)
{
    Ref operation = referencePath.operation;
    return operation->getPath(data);
}

inline std::optional<WebCore::Path> tryPath(const BasicShapePath& basicShapePath, const TransformOperationData& data)
{
    Ref operation = basicShapePath.operation;
    return operation->getPath(data);
}

inline std::optional<WebCore::Path> tryPath(const BoxPath& boxPath, const TransformOperationData& data)
{
    Ref operation = boxPath.operation;
    return operation->getPath(data);
}

// MARK: - Conversion

// Shared Converter for both ClipPath and OffsetPath
template<> struct CSSValueConversion<RefPtr<PathOperation>> { RefPtr<PathOperation> operator()(BuilderState&, const CSSValue&, SupportRayPathOperation); };

// `RayPath` is special-cased to return a `CSSRayValue`.
template<> struct CSSValueCreation<RayPath> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const RayPath&); };

// `BasicShapePath` is special-cased to handle non-standard `PathConversion` argument.
template<> struct CSSValueCreation<BasicShapePath> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const BasicShapePath&, PathConversion = PathConversion::None); };

// MARK: - Serialization

// `BasicShapePath` is special-cased to handle non-standard `PathConversion` argument.
template<> struct Serialize<BasicShapePath> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const BasicShapePath&, PathConversion = PathConversion::None); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const RayPath&);
WTF::TextStream& operator<<(WTF::TextStream&, const ReferencePath&);
WTF::TextStream& operator<<(WTF::TextStream&, const BasicShapePath&);
WTF::TextStream& operator<<(WTF::TextStream&, const BoxPath&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::RayPath)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ReferencePath)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BasicShapePath)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BoxPath)
