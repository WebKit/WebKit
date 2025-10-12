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

struct ClipPath;
struct OffsetPath;

// <'clip-path'> = none | <url> | [ <basic-shape> || <geometry-box> ]
struct ClipPath {
    ClipPath(CSS::Keyword::None) : operation { nullptr } { }
    ClipPath(ReferencePath&& reference) : operation { WTFMove(reference.operation) } { }
    ClipPath(const ReferencePath& reference) : operation { reference.operation } { }
    ClipPath(BasicShapePath&& basicShape) : operation { WTFMove(basicShape.operation) } { }
    ClipPath(const BasicShapePath& basicShape) : operation { basicShape.operation } { }
    ClipPath(BoxPath&& box) : operation { WTFMove(box.operation) } { }
    ClipPath(const BoxPath& box) : operation { box.operation } { }

    explicit ClipPath(RefPtr<PathOperation>&& operation) : operation { WTFMove(operation) } { RELEASE_ASSERT(isValid(operation)); }
    explicit ClipPath(const RefPtr<PathOperation>& operation) : operation { operation } { RELEASE_ASSERT(isValid(operation)); }

    bool isNone() const { return !operation; }
    bool isReference() const { return is<ReferencePathOperation>(operation); }
    bool isBasicShape() const { return is<ShapePathOperation>(operation); }
    bool isBox() const { return is<BoxPathOperation>(operation); }

    std::optional<ReferencePath> tryReference() const;
    std::optional<BasicShapePath> tryBasicShape() const;
    std::optional<BoxPath> tryBox() const;

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const ClipPath& other) const
    {
        return arePointingToEqualData(operation, other.operation);
    }

private:
    friend struct Blending<ClipPath>;
    friend CSSBoxType referenceBox(const ClipPath&);
    friend std::optional<WebCore::Path> tryPath(const ClipPath&, const TransformOperationData&);

    static bool isValid(RefPtr<PathOperation> operation)
    {
        return !operation || !is<RayPathOperation>(*operation);
    }

    RefPtr<PathOperation> operation;
};

inline std::optional<WebCore::Path> tryPath(const ClipPath& clipPath, const TransformOperationData& data)
{
    RefPtr operation = clipPath.operation;
    if (!operation)
        return { };
    return operation->getPath(data);
}

template<typename T> bool ClipPath::holdsAlternative() const
{
         if constexpr (std::same_as<T, CSS::Keyword::None>)                 return isNone();
    else if constexpr (std::same_as<T, ReferencePath>)                      return isReference();
    else if constexpr (std::same_as<T, BasicShapePath>)                     return isBasicShape();
    else if constexpr (std::same_as<T, BoxPath>)                            return isBox();
}

template<typename... F> decltype(auto) ClipPath::switchOn(F&&... f) const
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
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline std::optional<ReferencePath> ClipPath::tryReference() const
{
    if (RefPtr reference = dynamicDowncast<ReferencePathOperation>(operation))
        return ReferencePath { reference.releaseNonNull() };
    return { };
}

inline std::optional<BasicShapePath> ClipPath::tryBasicShape() const
{
    if (RefPtr basicShape = dynamicDowncast<ShapePathOperation>(operation))
        return BasicShapePath { basicShape.releaseNonNull() };
    return { };
}

inline std::optional<BoxPath> ClipPath::tryBox() const
{
    if (RefPtr box = dynamicDowncast<BoxPathOperation>(operation))
        return BoxPath { box.releaseNonNull() };
    return { };
}

// MARK: - Conversion

template<> struct CSSValueConversion<ClipPath> { auto operator()(BuilderState&, const CSSValue&) -> ClipPath; };

// MARK: - Blending

template<> struct Blending<ClipPath> {
    auto canBlend(const ClipPath&, const ClipPath&) -> bool;
    auto blend(const ClipPath&, const ClipPath&, const BlendingContext&) -> ClipPath;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ClipPath)
