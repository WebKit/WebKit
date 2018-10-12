/*
 * Copyright (C) 2005-2014 Apple Inc. All rights reserved.
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

#include "DisabledAdaptations.h"
#include "FloatSize.h"
#include "IntSize.h"
#include "ViewportArguments.h"
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

static const double forceAlwaysUserScalableMaximumScale = 5.0;
static const double forceAlwaysUserScalableMinimumScale = 1.0;

class ViewportConfiguration {
    WTF_MAKE_NONCOPYABLE(ViewportConfiguration); WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: unify with ViewportArguments.
    struct Parameters {
        double width { 0 };
        double height { 0 };
        double initialScale { 0 };
        double initialScaleIgnoringLayoutScaleFactor { 0 };
        double minimumScale { 0 };
        double maximumScale { 0 };
        bool allowsUserScaling { false };
        bool allowsShrinkToFit { false };
        bool avoidsUnsafeArea { true };

        bool widthIsSet { false };
        bool heightIsSet { false };
        bool initialScaleIsSet { false };

        bool operator==(const Parameters& other) const
        {
            return width == other.width && height == other.height
                && initialScale == other.initialScale && initialScaleIgnoringLayoutScaleFactor == other.initialScaleIgnoringLayoutScaleFactor && minimumScale == other.minimumScale && maximumScale == other.maximumScale
                && allowsUserScaling == other.allowsUserScaling && allowsShrinkToFit == other.allowsShrinkToFit && avoidsUnsafeArea == other.avoidsUnsafeArea
                && widthIsSet == other.widthIsSet && heightIsSet == other.heightIsSet && initialScaleIsSet == other.initialScaleIsSet;
        }
    };

    WEBCORE_EXPORT ViewportConfiguration();

    const Parameters& defaultConfiguration() const { return m_defaultConfiguration; }
    WEBCORE_EXPORT void setDefaultConfiguration(const Parameters&);

    const IntSize& contentsSize() const { return m_contentSize; }
    WEBCORE_EXPORT bool setContentsSize(const IntSize&);

    const FloatSize& viewLayoutSize() const { return m_viewLayoutSize; }

    const FloatSize& minimumLayoutSize() const { return m_minimumLayoutSize; }
    WEBCORE_EXPORT bool setViewLayoutSize(const FloatSize&, std::optional<double>&& scaleFactor = std::nullopt);

    const OptionSet<DisabledAdaptations>& disabledAdaptations() const { return m_disabledAdaptations; }
    WEBCORE_EXPORT bool setDisabledAdaptations(const OptionSet<DisabledAdaptations>&);

    const ViewportArguments& viewportArguments() const { return m_viewportArguments; }
    WEBCORE_EXPORT bool setViewportArguments(const ViewportArguments&);

    WEBCORE_EXPORT bool setCanIgnoreScalingConstraints(bool);
    void setForceAlwaysUserScalable(bool forceAlwaysUserScalable) { m_forceAlwaysUserScalable = forceAlwaysUserScalable; }

    WEBCORE_EXPORT IntSize layoutSize() const;
    WEBCORE_EXPORT double initialScale() const;
    WEBCORE_EXPORT double initialScaleIgnoringContentSize() const;
    WEBCORE_EXPORT double minimumScale() const;
    double maximumScale() const { return m_forceAlwaysUserScalable ? forceAlwaysUserScalableMaximumScale : m_configuration.maximumScale; }
    double maximumScaleIgnoringAlwaysScalable() const { return m_configuration.maximumScale; }
    WEBCORE_EXPORT bool allowsUserScaling() const;
    WEBCORE_EXPORT bool allowsUserScalingIgnoringAlwaysScalable() const;
    bool allowsShrinkToFit() const;
    bool avoidsUnsafeArea() const { return m_configuration.avoidsUnsafeArea; }

    // Matches a width=device-width, initial-scale=1 viewport.
    WEBCORE_EXPORT static Parameters nativeWebpageParameters();
    WEBCORE_EXPORT static Parameters webpageParameters();
    WEBCORE_EXPORT static Parameters textDocumentParameters();
    WEBCORE_EXPORT static Parameters imageDocumentParameters();
    WEBCORE_EXPORT static Parameters xhtmlMobileParameters();
    WEBCORE_EXPORT static Parameters testingParameters();
    
#ifndef NDEBUG
    String description() const;
    WEBCORE_EXPORT void dump() const;
#endif

private:
    void updateConfiguration();
    double viewportArgumentsLength(double length) const;
    double initialScaleFromSize(double width, double height, bool shouldIgnoreScalingConstraints) const;
    int layoutWidth() const;
    int layoutHeight() const;

    bool shouldOverrideDeviceWidthAndShrinkToFit() const;
    bool shouldIgnoreScalingConstraintsRegardlessOfContentSize() const;
    bool shouldIgnoreScalingConstraints() const;
    bool shouldIgnoreVerticalScalingConstraints() const;
    bool shouldIgnoreHorizontalScalingConstraints() const;

    void updateMinimumLayoutSize();

    Parameters m_configuration;
    Parameters m_defaultConfiguration;
    IntSize m_contentSize;
    FloatSize m_minimumLayoutSize;
    FloatSize m_viewLayoutSize;
    ViewportArguments m_viewportArguments;
    OptionSet<DisabledAdaptations> m_disabledAdaptations;

    double m_layoutSizeScaleFactor { 1 };
    bool m_canIgnoreScalingConstraints;
    bool m_forceAlwaysUserScalable;
};

WTF::TextStream& operator<<(WTF::TextStream&, const ViewportConfiguration::Parameters&);
WTF::TextStream& operator<<(WTF::TextStream&, const ViewportConfiguration&);

} // namespace WebCore
