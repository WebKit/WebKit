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

#include <WebCore/StylePathOperationWrappers.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'offset-path'> = none | [ [ <ray()> | <url> | <basic-shape> ] || <coord-box> ]
// https://drafts.fxtf.org/motion-1/#propdef-offset-path
struct OffsetPath {
    OffsetPath(CSS::Keyword::None) : operation { nullptr } { }
    OffsetPath(RayPath&& ray) : operation { WTFMove(ray.operation) } { }
    OffsetPath(const RayPath& ray) : operation { ray.operation } { }
    OffsetPath(ReferencePath&& reference) : operation { WTFMove(reference.operation) } { }
    OffsetPath(const ReferencePath& reference) : operation { reference.operation } { }
    OffsetPath(BasicShapePath&& basicShape) : operation { WTFMove(basicShape.operation) } { }
    OffsetPath(const BasicShapePath& basicShape) : operation { basicShape.operation } { }
    OffsetPath(BoxPath&& box) : operation { WTFMove(box.operation) } { }
    OffsetPath(const BoxPath& box) : operation { box.operation } { }

    explicit OffsetPath(RefPtr<PathOperation>&& operation) : operation { WTFMove(operation) } { }
    explicit OffsetPath(const RefPtr<PathOperation>& operation) : operation { operation } { }

    bool isNone() const { return !operation; }
    bool isRay() const { return is<RayPathOperation>(operation); }
    bool isReference() const { return is<ReferencePathOperation>(operation); }
    bool isBasicShape() const { return is<ShapePathOperation>(operation); }
    bool isBox() const { return is<BoxPathOperation>(operation); }

    std::optional<RayPath> tryRay() const;
    std::optional<ReferencePath> tryReference() const;
    std::optional<BasicShapePath> tryBasicShape() const;
    std::optional<BoxPath> tryBox() const;

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const OffsetPath& other) const
    {
        return arePointingToEqualData(operation, other.operation);
    }

private:
    friend struct Blending<OffsetPath>;
    friend struct ToPlatform<OffsetPath>;
    friend std::optional<WebCore::Path> tryPath(const OffsetPath&, const TransformOperationData&);

    RefPtr<PathOperation> operation;
};

inline std::optional<WebCore::Path> tryPath(const OffsetPath& offsetPath, const TransformOperationData& data)
{
    RefPtr operation = offsetPath.operation;
    if (!operation)
        return { };
    return operation->getPath(data);
}

template<typename T> bool OffsetPath::holdsAlternative() const
{
         if constexpr (std::same_as<T, CSS::Keyword::None>)                 return isNone();
    else if constexpr (std::same_as<T, RayPath>)                            return isRay();
    else if constexpr (std::same_as<T, ReferencePath>)                      return isReference();
    else if constexpr (std::same_as<T, BasicShapePath>)                     return isBasicShape();
    else if constexpr (std::same_as<T, BoxPath>)                            return isBox();
}

template<typename... F> decltype(auto) OffsetPath::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    if (!operation)
        return visitor(CSS::Keyword::None { });

    switch (operation->type()) {
    case PathOperation::Type::Reference:
        return visitor(ReferencePath { downcast<ReferencePathOperation>(*operation) });
    case PathOperation::Type::Shape:
        return visitor(BasicShapePath { downcast<ShapePathOperation>(*operation) });
    case PathOperation::Type::Box:
        return visitor(BoxPath { downcast<BoxPathOperation>(*operation) });
    case PathOperation::Type::Ray:
        return visitor(RayPath { downcast<RayPathOperation>(*operation) });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline std::optional<RayPath> OffsetPath::tryRay() const
{
    if (RefPtr ray = dynamicDowncast<RayPathOperation>(operation))
        return RayPath { ray.releaseNonNull() };
    return { };
}

inline std::optional<ReferencePath> OffsetPath::tryReference() const
{
    if (RefPtr reference = dynamicDowncast<ReferencePathOperation>(operation))
        return ReferencePath { reference.releaseNonNull() };
    return { };
}

inline std::optional<BasicShapePath> OffsetPath::tryBasicShape() const
{
    if (RefPtr basicShape = dynamicDowncast<ShapePathOperation>(operation))
        return BasicShapePath { basicShape.releaseNonNull() };
    return { };
}

inline std::optional<BoxPath> OffsetPath::tryBox() const
{
    if (RefPtr box = dynamicDowncast<BoxPathOperation>(operation))
        return BoxPath { box.releaseNonNull() };
    return { };
}

// MARK: - Conversion

template<> struct CSSValueConversion<OffsetPath> { auto operator()(BuilderState&, const CSSValue&) -> OffsetPath; };
template<> struct CSSValueCreation<OffsetPath> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const OffsetPath&); };

// MARK: - Serialization

template<> struct Serialize<OffsetPath> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const OffsetPath&); };

// MARK: - Blending

template<> struct Blending<OffsetPath> {
    auto canBlend(const OffsetPath&, const OffsetPath&) -> bool;
    auto blend(const OffsetPath&, const OffsetPath&, const BlendingContext&) -> OffsetPath;
};

// MARK: - Platform

template<> struct ToPlatform<OffsetPath> { auto operator()(const OffsetPath&) -> RefPtr<PathOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OffsetPath)
