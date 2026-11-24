/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GradientCacheCG.h"

#include "ColorConversion.h"
#include "ColorHash.h"
#include "ColorSpace.h"
#include "ColorSpaceCG.h"
#include "Gradient.h"
#include "GradientColorStops.h"
#include "PredefinedColorSpace.h"
#include "SampledGradientBuilder.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/HashMap.h>
#include <wtf/Hasher.h>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using GradientColorStopVector = GradientColorStops::StopVector;

static RetainPtr<CGGradientRef> createGradientBySampling(ColorInterpolationMethod, const GradientColorStopVector&, const std::optional<ColorSpace>&);

// MARK: - Renderer color space.

std::optional<ColorSpace> GradientCacheCG::colorSpaceForDestination(const ColorSpace& colorSpace)
{
    auto predefined = toPredefinedColorSpace(colorSpace);
    if (!predefined)
        return colorSpace;
    switch (*predefined) {
    case PredefinedColorSpace::SRGBLinear:
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    case PredefinedColorSpace::DisplayP3Linear:
#endif
        return std::nullopt;
    default:
        return colorSpace;
    }
}

// MARK: - Gradient options.

static CFDictionaryRef gradientInterpolatesPremultipliedOptionsDictionarySingleton()
{
    static CFTypeRef keys[] = { kCGGradientInterpolatesPremultiplied };
    static CFTypeRef values[] = { kCFBooleanTrue };
    static NeverDestroyed<RetainPtr<CFDictionaryRef>> options = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    return options.get().get();
}

static CFDictionaryRef gradientOptionsDictionarySingleton(ColorInterpolationMethod colorInterpolationMethod)
{
    switch (colorInterpolationMethod.alphaPremultiplication) {
    case AlphaPremultiplication::Unpremultiplied:
        return nullptr;
    case AlphaPremultiplication::Premultiplied:
        return gradientInterpolatesPremultipliedOptionsDictionarySingleton();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

// MARK: - Direct CGGradient strategy (sRGB only).

static bool anyComponentIsNone(const GradientColorStops& stops)
{
    for (auto& stop : stops) {
        if (stop.color.anyComponentIsNone())
            return true;
    }

    return false;
}

// For non-sRGB color spaces, or sRGB with 'none' components, the CGGradient is created by sampling.
static bool needsSampling(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops)
{
    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&](const ColorInterpolationMethod::SRGB&) {
            // FIXME: As an optimization we can precompute 'none' replacements and create a transformed stop list rather than falling back on gradient sampling.
            return anyComponentIsNone(stops);
        },
        [&](const auto&) {
            return true;
        }
    );
}

static RetainPtr<CGGradientRef> makeGradient(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops, const std::optional<ColorSpace>& colorSpace)
{
    if (needsSampling(colorInterpolationMethod, stops))
        return createGradientBySampling(colorInterpolationMethod, stops.stops(), colorSpace);

    auto hasOnlyBoundedSRGBColorStops = [](const auto& stops) {
        for (const auto& stop : stops) {
            if (stop.color.colorSpace() != ColorSpaceName::SRGB)
                return false;
        }
        return true;
    };

    auto numberOfStops = stops.size();

    static constexpr auto reservedStops = 3;
    Vector<CGFloat, reservedStops> locations(ReserveCapacityTag { }, numberOfStops);

    Vector<CGFloat, 4 * reservedStops> colorComponents(ReserveCapacityTag { }, numberOfStops * 4);

    RetainPtr cgColorSpace = [&] -> CGColorSpaceRef {
        if (colorSpace) {
            for (const auto& stop : stops) {
                auto components = stop.color.toResolvedColorComponentsInColorSpace(*colorSpace);
                colorComponents.appendList({ components[0], components[1], components[2], components[3] });
                locations.append(stop.offset);
            }
            return colorSpace->platformColorSpace();
        }

        // FIXME: Now that we only ever use CGGradientCreateWithColorComponents, we should investigate
        // if there is any real benefit to using sRGB when all the stops are bounded vs just using
        // extended sRGB for all gradients.
        if (hasOnlyBoundedSRGBColorStops(stops)) {
            for (const auto& stop : stops) {
                auto [r, g, b, a] = stop.color.toColorTypeLossy<SRGBA<float>>().resolved();
                colorComponents.appendList({ r, g, b, a });

                locations.append(stop.offset);
            }

            return cachedCGColorSpaceSingleton<ColorSpaceFor<SRGBA<float>>>();
        }

        using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpaceName::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;
        for (const auto& stop : stops) {
            auto [r, g, b, a] = stop.color.toColorTypeLossy<OutputSpaceColorType>().resolved();
            colorComponents.appendList({ r, g, b, a });

            locations.append(stop.offset);
        }
        return cachedCGColorSpaceSingleton<ColorSpaceFor<OutputSpaceColorType>>();
    }();

    // CoreGraphics has a bug where if the last two stops are at 1, it fails to extend the last stop's color. This can be visible in radial gradients.
    auto apply139572277Workaround = [&] {
        if (numberOfStops < 2)
            return;

        if (locations[numberOfStops - 2] == 1.0 && locations[numberOfStops - 1] == 1.0) {
            // Replicate the last color stop.
            locations.append(1.0);

            auto lastColorComponentIndex = 4 * (numberOfStops - 1);

            colorComponents.reserveCapacity((numberOfStops + 1) * 4);
            colorComponents.append(colorComponents[lastColorComponentIndex]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 1]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 2]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 3]);

            ++numberOfStops;
        }
    };

    apply139572277Workaround();

    return adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace.get(), colorComponents.span().data(), locations.span().data(), numberOfStops, gradientOptionsDictionarySingleton(colorInterpolationMethod)));
}

// MARK: - Gradient-by-sampling strategy.

static RetainPtr<CGGradientRef> createGradientBySampling(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStopVector& stops, const std::optional<ColorSpace>& destinationColorSpace)
{
    using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpaceName::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    auto sampled = sampleGradientStops<OutputSpaceColorType>(colorInterpolationMethod, stops);

    Vector<CGFloat> locations(sampled.locations.size());
    Vector<CGFloat> components(sampled.colorComponents.size());
    for (size_t i = 0; i < sampled.locations.size(); ++i)
        locations[i] = sampled.locations[i];

    RetainPtr<CGColorSpaceRef> cgColorSpace;
    if (destinationColorSpace) {
        constexpr auto inputColorSpace = ColorSpaceFor<OutputSpaceColorType>;
        auto numberOfStops = sampled.locations.size();
        for (size_t i = 0; i < numberOfStops; ++i) {
            ColorComponents<float, 4> input {
                sampled.colorComponents[i * 4],
                sampled.colorComponents[i * 4 + 1],
                sampled.colorComponents[i * 4 + 2],
                sampled.colorComponents[i * 4 + 3]
            };
            auto converted = convertAndResolveColorComponents(inputColorSpace, input, *destinationColorSpace);
            components[i * 4] = converted[0];
            components[i * 4 + 1] = converted[1];
            components[i * 4 + 2] = converted[2];
            components[i * 4 + 3] = converted[3];
        }
        cgColorSpace = destinationColorSpace->platformColorSpace();
    } else {
        for (size_t i = 0; i < sampled.colorComponents.size(); ++i)
            components[i] = sampled.colorComponents[i];
        cgColorSpace = cachedCGColorSpaceSingleton<ColorSpaceFor<OutputSpaceColorType>>();
    }

    return adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace,
        components.span().data(), locations.span().data(), locations.size(), gradientOptionsDictionarySingleton(colorInterpolationMethod)));
}

// MARK: - Cache.

namespace {

// What a CGGradient is created from. Gradients with equal keys share a CGGradient.
struct CGGradientKey {
    ColorInterpolationMethod colorInterpolationMethod;
    GradientColorStopVector stops;
    std::optional<ColorSpace> colorSpace;

    bool matches(ColorInterpolationMethod otherColorInterpolationMethod, const GradientColorStops& otherStops, const std::optional<ColorSpace>& otherColorSpace) const
    {
        return colorInterpolationMethod == otherColorInterpolationMethod && colorSpace == otherColorSpace && stops == otherStops.stops();
    }
};

// Hashes what a CGGradient is created from, except the renderer color space. Never 0 or the maximum value.
unsigned computeCGGradientHash(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops)
{
    Hasher hasher;
    add(hasher, colorInterpolationMethod);
    for (auto& stop : stops)
        add(hasher, stop.offset, stop.color);
    // Hasher never returns 0. Also avoid the maximum value, so that the hash can be a key with the default hash traits.
    return std::min(hasher.hash(), std::numeric_limits<unsigned>::max() - 1);
}

// Process-wide cache of the CGGradients of non-transient gradients, and of all gradients created by sampling.
// Non-transient gradients are painted across frames and contexts, in the GPU process also concurrently from
// the rendering backend work queues and the main thread. Sampling is expensive also for transient gradients,
// e.g. CSS gradients that are created for each paint. The CGGradients are keyed by what they are created
// from, so gradients with the same color stops share a CGGradient, and the cache holds no reference to the
// gradients. The CGGradients of non-transient gradients are also keyed by the gradient identifier, so that a
// gradient painted again is found without hashing and comparing its color stops. Unused entries are evicted
// as least recently used.
class CGGradientCache {
    WTF_MAKE_NONCOPYABLE(CGGradientCache);
public:
    CGGradientCache() = default;

    static CGGradientCache& singleton()
    {
        static NeverDestroyed<CGGradientCache> cache;
        return cache;
    }

    RetainPtr<CGGradientRef> cgGradient(const Gradient& gradient, const std::optional<ColorSpace>& colorSpace)
    {
        // Gradients are immutable and identifiers are not reused, so a CGGradient found by the identifier is
        // the one of the gradient. Transient gradients are not looked up by the identifier, as those are
        // created for each paint and would only evict the identifiers of the non-transient gradients.
        if (!gradient.isTransient()) {
            Locker locker { m_lock };
            if (RetainPtr cgGradient = findByIdentifier(gradient.identifier(), colorSpace))
                return cgGradient;
        }

        auto hash = computeCGGradientHash(gradient.colorInterpolationMethod(), gradient.stops());
        {
            Locker locker { m_lock };
            if (RetainPtr cgGradient = find(hash, gradient, colorSpace))
                return addByIdentifier(gradient, colorSpace, cgGradient);
        }

        // Create the CGGradient without holding the lock. Another thread may create one for the same key concurrently.
        auto cgGradient = makeGradient(gradient.colorInterpolationMethod(), gradient.stops(), colorSpace);

        Locker locker { m_lock };
        if (RetainPtr cachedCGGradient = find(hash, gradient, colorSpace))
            return addByIdentifier(gradient, colorSpace, cachedCGGradient);
        m_entries.ensure(hash, [] {
            return Entries { };
        }).iterator->value.append({ { gradient.colorInterpolationMethod(), gradient.stops().stops(), colorSpace }, cgGradient });
        if (m_hashes.size() > capacity)
            m_entries.remove(m_hashes.takeFirst());
        return addByIdentifier(gradient, colorSpace, cgGradient);
    }

private:
    struct Entry {
        CGGradientKey key;
        RetainPtr<CGGradientRef> cgGradient;
    };
    // The entries whose keys have the same hash.
    using Entries = Vector<Entry, 1>;

    struct IdentifierEntry {
        std::optional<ColorSpace> colorSpace;
        RetainPtr<CGGradientRef> cgGradient;
    };
    // The entries of a gradient, one per renderer color space.
    using IdentifierEntries = Vector<IdentifierEntry, 1>;

    RetainPtr<CGGradientRef> findByIdentifier(GradientIdentifier identifier, const std::optional<ColorSpace>& colorSpace) WTF_REQUIRES_LOCK(m_lock)
    {
        auto iterator = m_identifierEntries.find(identifier);
        if (iterator == m_identifierEntries.end())
            return nullptr;
        auto index = iterator->value.findIf([&](auto& entry) {
            return entry.colorSpace == colorSpace;
        });
        if (index == notFound)
            return nullptr;
        m_identifiers.appendOrMoveToLast(identifier);
        return iterator->value[index].cgGradient;
    }

    RetainPtr<CGGradientRef> addByIdentifier(const Gradient& gradient, const std::optional<ColorSpace>& colorSpace, const RetainPtr<CGGradientRef>& cgGradient) WTF_REQUIRES_LOCK(m_lock)
    {
        if (gradient.isTransient())
            return cgGradient;
        auto identifier = gradient.identifier();
        auto& entries = m_identifierEntries.ensure(identifier, [] {
            return IdentifierEntries { };
        }).iterator->value;
        // Another thread may have added the CGGradient after this thread did not find it by the identifier.
        if (!entries.containsIf([&](auto& entry) { return entry.colorSpace == colorSpace; }))
            entries.append({ colorSpace, cgGradient });
        m_identifiers.appendOrMoveToLast(identifier);
        if (m_identifiers.size() > capacity)
            m_identifierEntries.remove(m_identifiers.takeFirst());
        return cgGradient;
    }

    RetainPtr<CGGradientRef> find(unsigned hash, const Gradient& gradient, const std::optional<ColorSpace>& colorSpace) WTF_REQUIRES_LOCK(m_lock)
    {
        m_hashes.appendOrMoveToLast(hash);
        auto iterator = m_entries.find(hash);
        if (iterator == m_entries.end())
            return nullptr;
        auto index = iterator->value.findIf([&](auto& entry) {
            return entry.key.matches(gradient.colorInterpolationMethod(), gradient.stops(), colorSpace);
        });
        if (index == notFound)
            return nullptr;
        return iterator->value[index].cgGradient;
    }

    // Number of hashes, and of gradient identifiers, whose CGGradients are kept.
    static constexpr size_t capacity = 512;

    Lock m_lock;
    HashMap<unsigned, Entries> m_entries WTF_GUARDED_BY_LOCK(m_lock);
    // Least recently used first.
    ListHashSet<unsigned> m_hashes WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<GradientIdentifier, IdentifierEntries> m_identifierEntries WTF_GUARDED_BY_LOCK(m_lock);
    // Least recently used first.
    ListHashSet<GradientIdentifier> m_identifiers WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace

RetainPtr<CGGradientRef> GradientCacheCG::cgGradient(const Gradient& gradient, const std::optional<ColorSpace>& colorSpace)
{
    if (gradient.isTransient() && !needsSampling(gradient.colorInterpolationMethod(), gradient.stops()))
        return makeGradient(gradient.colorInterpolationMethod(), gradient.stops(), colorSpace);
    return CGGradientCache::singleton().cgGradient(gradient, colorSpace);
}

}
