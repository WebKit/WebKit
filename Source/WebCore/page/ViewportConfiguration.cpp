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

#include "config.h"
#include "ViewportConfiguration.h"

#include <WebCore/TextStream.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS)
#include "PlatformScreen.h"
#endif

namespace WebCore {

#if !ASSERT_DISABLED
static bool constraintsAreAllRelative(const ViewportConfiguration::Parameters& configuration)
{
    return !configuration.widthIsSet && !configuration.heightIsSet && !configuration.initialScaleIsSet;
}
#endif

ViewportConfiguration::ViewportConfiguration()
    : m_minimumLayoutSize(1024, 768)
    , m_canIgnoreScalingConstraints(false)
    , m_forceAlwaysUserScalable(false)
{
    // Setup a reasonable default configuration to avoid computing infinite scale/sizes.
    // Those are the original iPhone configuration.
    m_defaultConfiguration = ViewportConfiguration::webpageParameters();
    updateConfiguration();
}

void ViewportConfiguration::setDefaultConfiguration(const ViewportConfiguration::Parameters& defaultConfiguration)
{
    ASSERT(!constraintsAreAllRelative(m_configuration));
    ASSERT(!defaultConfiguration.initialScaleIsSet || defaultConfiguration.initialScale > 0);
    ASSERT(defaultConfiguration.minimumScale > 0);
    ASSERT(defaultConfiguration.maximumScale >= defaultConfiguration.minimumScale);

    m_defaultConfiguration = defaultConfiguration;
    updateConfiguration();
}

void ViewportConfiguration::setContentsSize(const IntSize& contentSize)
{
    if (m_contentSize == contentSize)
        return;

    m_contentSize = contentSize;
    updateConfiguration();
}

void ViewportConfiguration::setMinimumLayoutSize(const FloatSize& minimumLayoutSize)
{
    if (m_minimumLayoutSize == minimumLayoutSize)
        return;

    m_minimumLayoutSize = minimumLayoutSize;
    updateConfiguration();
}

void ViewportConfiguration::setViewportArguments(const ViewportArguments& viewportArguments)
{
    if (m_viewportArguments == viewportArguments)
        return;

    m_viewportArguments = viewportArguments;
    updateConfiguration();
}

IntSize ViewportConfiguration::layoutSize() const
{
    return IntSize(layoutWidth(), layoutHeight());
}

bool ViewportConfiguration::shouldIgnoreHorizontalScalingConstraints() const
{
    if (!m_canIgnoreScalingConstraints)
        return false;

    if (!m_configuration.allowsShrinkToFit)
        return false;

    bool laidOutWiderThanViewport = m_contentSize.width() > layoutWidth();
    if (m_viewportArguments.width == ViewportArguments::ValueDeviceWidth)
        return laidOutWiderThanViewport;

    if (m_configuration.initialScaleIsSet && m_configuration.initialScale == 1)
        return laidOutWiderThanViewport;

    return false;
}

bool ViewportConfiguration::shouldIgnoreVerticalScalingConstraints() const
{
    if (!m_canIgnoreScalingConstraints)
        return false;

    if (!m_configuration.allowsShrinkToFit)
        return false;

    bool laidOutTallerThanViewport = m_contentSize.height() > layoutHeight();
    if (m_viewportArguments.height == ViewportArguments::ValueDeviceHeight && m_viewportArguments.width == ViewportArguments::ValueAuto)
        return laidOutTallerThanViewport;

    return false;
}

bool ViewportConfiguration::shouldIgnoreScalingConstraints() const
{
    return shouldIgnoreHorizontalScalingConstraints() || shouldIgnoreVerticalScalingConstraints();
}

double ViewportConfiguration::initialScaleFromSize(double width, double height, bool shouldIgnoreScalingConstraints) const
{
    ASSERT(!constraintsAreAllRelative(m_configuration));

    // If the document has specified its own initial scale, use it regardless.
    // This is guaranteed to be sanity checked already, so no need for MIN/MAX.
    if (m_configuration.initialScaleIsSet && !shouldIgnoreScalingConstraints)
        return m_configuration.initialScale;

    // If not, it is up to us to determine the initial scale.
    // We want a scale small enough to fit the document width-wise.
    const FloatSize& minimumLayoutSize = m_minimumLayoutSize;
    double initialScale = 0;
    if (width > 0 && !shouldIgnoreVerticalScalingConstraints())
        initialScale = minimumLayoutSize.width() / width;

    // Prevent the initial scale from shrinking to a height smaller than our view's minimum height.
    if (height > 0 && height * initialScale < minimumLayoutSize.height() && !shouldIgnoreHorizontalScalingConstraints())
        initialScale = minimumLayoutSize.height() / height;
    return std::min(std::max(initialScale, shouldIgnoreScalingConstraints ? m_defaultConfiguration.minimumScale : m_configuration.minimumScale), m_configuration.maximumScale);
}

double ViewportConfiguration::initialScale() const
{
    return initialScaleFromSize(m_contentSize.width() > 0 ? m_contentSize.width() : layoutWidth(), m_contentSize.height() > 0 ? m_contentSize.height() : layoutHeight(), shouldIgnoreScalingConstraints());
}

double ViewportConfiguration::initialScaleIgnoringContentSize() const
{
    return initialScaleFromSize(layoutWidth(), layoutHeight(), false);
}

double ViewportConfiguration::minimumScale() const
{
    // If we scale to fit, then this is our minimum scale as well.
    if (!m_configuration.initialScaleIsSet || shouldIgnoreScalingConstraints())
        return initialScale();

    // If not, we still need to sanity check our value.
    double minimumScale = m_configuration.minimumScale;

    const FloatSize& minimumLayoutSize = m_minimumLayoutSize;
    double contentWidth = m_contentSize.width();
    if (contentWidth > 0 && contentWidth * minimumScale < minimumLayoutSize.width() && !shouldIgnoreVerticalScalingConstraints())
        minimumScale = minimumLayoutSize.width() / contentWidth;

    double contentHeight = m_contentSize.height();
    if (contentHeight > 0 && contentHeight * minimumScale < minimumLayoutSize.height() && !shouldIgnoreHorizontalScalingConstraints())
        minimumScale = minimumLayoutSize.height() / contentHeight;

    minimumScale = std::min(std::max(minimumScale, m_configuration.minimumScale), m_configuration.maximumScale);

    return minimumScale;
}

bool ViewportConfiguration::allowsUserScaling() const
{
    return m_forceAlwaysUserScalable || shouldIgnoreScalingConstraints() || m_configuration.allowsUserScaling;
}

ViewportConfiguration::Parameters ViewportConfiguration::webpageParameters()
{
    Parameters parameters;
    parameters.width = 980;
    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
    parameters.allowsShrinkToFit = true;
    parameters.minimumScale = 0.25;
    parameters.maximumScale = 5;
    return parameters;
}

ViewportConfiguration::Parameters ViewportConfiguration::textDocumentParameters()
{
    Parameters parameters;

#if PLATFORM(IOS)
    parameters.width = static_cast<int>(screenSize().width());
#else
    // FIXME: this needs to be unified with ViewportArguments on all ports.
    parameters.width = 320;
#endif

    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
    parameters.allowsShrinkToFit = false;
    parameters.minimumScale = 0.25;
    parameters.maximumScale = 5;
    return parameters;
}

ViewportConfiguration::Parameters ViewportConfiguration::imageDocumentParameters()
{
    Parameters parameters;
    parameters.width = 980;
    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
    parameters.allowsShrinkToFit = false;
    parameters.minimumScale = 0.01;
    parameters.maximumScale = 5;
    return parameters;
}

ViewportConfiguration::Parameters ViewportConfiguration::xhtmlMobileParameters()
{
    Parameters parameters = webpageParameters();
    parameters.width = 320;
    return parameters;
}

ViewportConfiguration::Parameters ViewportConfiguration::testingParameters()
{
    Parameters parameters;
    parameters.initialScale = 1;
    parameters.initialScaleIsSet = true;
    parameters.allowsShrinkToFit = true;
    parameters.minimumScale = 1;
    parameters.maximumScale = 5;
    return parameters;
}

static inline bool viewportArgumentValueIsValid(float value)
{
    return value > 0;
}

template<typename ValueType, typename ViewportArgumentsType>
static inline void applyViewportArgument(ValueType& value, ViewportArgumentsType viewportArgumentValue, ValueType minimum, ValueType maximum)
{
    if (viewportArgumentValueIsValid(viewportArgumentValue))
        value = std::min(maximum, std::max(minimum, static_cast<ValueType>(viewportArgumentValue)));
}

template<typename ValueType, typename ViewportArgumentsType>
static inline void applyViewportArgument(ValueType& value, bool& valueIsSet, ViewportArgumentsType viewportArgumentValue, ValueType minimum, ValueType maximum)
{
    if (viewportArgumentValueIsValid(viewportArgumentValue)) {
        value = std::min(maximum, std::max(minimum, static_cast<ValueType>(viewportArgumentValue)));
        valueIsSet = true;
    } else
        valueIsSet = false;
}

static inline bool booleanViewportArgumentIsSet(float value)
{
    return !value || value == 1;
}

void ViewportConfiguration::updateConfiguration()
{
    m_configuration = m_defaultConfiguration;

    const double minimumViewportArgumentsScaleFactor = 0.1;
    const double maximumViewportArgumentsScaleFactor = 10.0;

    bool viewportArgumentsOverridesInitialScale;
    bool viewportArgumentsOverridesWidth;
    bool viewportArgumentsOverridesHeight;

    applyViewportArgument(m_configuration.minimumScale, m_viewportArguments.minZoom, minimumViewportArgumentsScaleFactor, maximumViewportArgumentsScaleFactor);
    applyViewportArgument(m_configuration.maximumScale, m_viewportArguments.maxZoom, m_configuration.minimumScale, maximumViewportArgumentsScaleFactor);
    applyViewportArgument(m_configuration.initialScale, viewportArgumentsOverridesInitialScale, m_viewportArguments.zoom, m_configuration.minimumScale, m_configuration.maximumScale);

    double minimumViewportArgumentsDimension = 10;
    double maximumViewportArgumentsDimension = 10000;
    applyViewportArgument(m_configuration.width, viewportArgumentsOverridesWidth, viewportArgumentsLength(m_viewportArguments.width), minimumViewportArgumentsDimension, maximumViewportArgumentsDimension);
    applyViewportArgument(m_configuration.height, viewportArgumentsOverridesHeight, viewportArgumentsLength(m_viewportArguments.height), minimumViewportArgumentsDimension, maximumViewportArgumentsDimension);

    if (viewportArgumentsOverridesInitialScale || viewportArgumentsOverridesWidth || viewportArgumentsOverridesHeight) {
        m_configuration.initialScaleIsSet = viewportArgumentsOverridesInitialScale;
        m_configuration.widthIsSet = viewportArgumentsOverridesWidth;
        m_configuration.heightIsSet = viewportArgumentsOverridesHeight;
    }

    if (booleanViewportArgumentIsSet(m_viewportArguments.userZoom))
        m_configuration.allowsUserScaling = m_viewportArguments.userZoom != 0.;

    if (booleanViewportArgumentIsSet(m_viewportArguments.shrinkToFit))
        m_configuration.allowsShrinkToFit = m_viewportArguments.shrinkToFit != 0.;
}

double ViewportConfiguration::viewportArgumentsLength(double length) const
{
    if (length == ViewportArguments::ValueDeviceWidth)
        return m_minimumLayoutSize.width();
    if (length == ViewportArguments::ValueDeviceHeight)
        return m_minimumLayoutSize.height();
    return length;
}

int ViewportConfiguration::layoutWidth() const
{
    ASSERT(!constraintsAreAllRelative(m_configuration));

    const FloatSize& minimumLayoutSize = m_minimumLayoutSize;
    if (m_configuration.widthIsSet) {
        // If we scale to fit, then accept the viewport width with sanity checking.
        if (!m_configuration.initialScaleIsSet) {
            double maximumScale = this->maximumScale();
            double maximumContentWidthInViewportCoordinate = maximumScale * m_configuration.width;
            if (maximumContentWidthInViewportCoordinate < minimumLayoutSize.width()) {
                // The content zoomed to maxScale does not fit the the view. Return the minimum width
                // satisfying the constraint maximumScale.
                return std::round(minimumLayoutSize.width() / maximumScale);
            }
            return std::round(m_configuration.width);
        }

        // If not, make sure the viewport width and initial scale can co-exist.
        double initialContentWidthInViewportCoordinate = m_configuration.width * m_configuration.initialScale;
        if (initialContentWidthInViewportCoordinate < minimumLayoutSize.width()) {
            // The specified width does not fit in viewport. Return the minimum width that satisfy the initialScale constraint.
            return std::round(minimumLayoutSize.width() / m_configuration.initialScale);
        }
        return std::round(m_configuration.width);
    }

    // If the page has a real scale, then just return the minimum size over the initial scale.
    if (m_configuration.initialScaleIsSet && !m_configuration.heightIsSet)
        return std::round(minimumLayoutSize.width() / m_configuration.initialScale);

    if (minimumLayoutSize.height() > 0)
        return std::round(minimumLayoutSize.width() * layoutHeight() / minimumLayoutSize.height());
    return minimumLayoutSize.width();
}

int ViewportConfiguration::layoutHeight() const
{
    ASSERT(!constraintsAreAllRelative(m_configuration));

    const FloatSize& minimumLayoutSize = m_minimumLayoutSize;
    if (m_configuration.heightIsSet) {
        // If we scale to fit, then accept the viewport height with sanity checking.
        if (!m_configuration.initialScaleIsSet) {
            double maximumScale = this->maximumScale();
            double maximumContentHeightInViewportCoordinate = maximumScale * m_configuration.height;
            if (maximumContentHeightInViewportCoordinate < minimumLayoutSize.height()) {
                // The content zoomed to maxScale does not fit the the view. Return the minimum height that
                // satisfy the constraint maximumScale.
                return std::round(minimumLayoutSize.height() / maximumScale);
            }
            return std::round(m_configuration.height);
        }

        // If not, make sure the viewport width and initial scale can co-exist.
        double initialContentHeightInViewportCoordinate = m_configuration.height * m_configuration.initialScale;
        if (initialContentHeightInViewportCoordinate < minimumLayoutSize.height()) {
            // The specified width does not fit in viewport. Return the minimum height that satisfy the initialScale constraint.
            return std::round(minimumLayoutSize.height() / m_configuration.initialScale);
        }
        return std::round(m_configuration.height);
    }

    // If the page has a real scale, then just return the minimum size over the initial scale.
    if (m_configuration.initialScaleIsSet && !m_configuration.widthIsSet)
        return std::round(minimumLayoutSize.height() / m_configuration.initialScale);

    if (minimumLayoutSize.width() > 0)
        return std::round(minimumLayoutSize.height() * layoutWidth() / minimumLayoutSize.width());
    return minimumLayoutSize.height();
}

#ifndef NDEBUG

TextStream& operator<<(TextStream& ts, const ViewportConfiguration::Parameters& parameters)
{
    ts.startGroup();
    ts << "width " << parameters.width << ", set: " << (parameters.widthIsSet ? "true" : "false");
    ts.endGroup();

    ts.startGroup();
    ts << "height " << parameters.height << ", set: " << (parameters.heightIsSet ? "true" : "false");
    ts.endGroup();

    ts.startGroup();
    ts << "initialScale " << parameters.initialScale << ", set: " << (parameters.initialScaleIsSet ? "true" : "false");
    ts.endGroup();

    ts.dumpProperty("minimumScale", parameters.minimumScale);
    ts.dumpProperty("maximumScale", parameters.maximumScale);
    ts.dumpProperty("allowsUserScaling", parameters.allowsUserScaling);
    ts.dumpProperty("allowsShrinkToFit", parameters.allowsShrinkToFit);

    return ts;
}

CString ViewportConfiguration::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "viewport-configuration " << (void*)this;
    {
        TextStream::GroupScope scope(ts);
        ts << "viewport arguments";
        ts << m_viewportArguments;
    }
    {
        TextStream::GroupScope scope(ts);
        ts << "configuration";
        ts << m_configuration;
    }
    {
        TextStream::GroupScope scope(ts);
        ts << "default configuration";
        ts << m_defaultConfiguration;
    }

    ts.dumpProperty("contentSize", m_contentSize);
    ts.dumpProperty("minimumLayoutSize", m_minimumLayoutSize);
    ts.dumpProperty("computed initial scale", initialScale());
    ts.dumpProperty("computed minimum scale", minimumScale());
    ts.dumpProperty("computed layout size", layoutSize());
    ts.dumpProperty("ignoring horizontal scaling constraints", shouldIgnoreHorizontalScalingConstraints() ? "true" : "false");
    ts.dumpProperty("ignoring vertical scaling constraints", shouldIgnoreVerticalScalingConstraints() ? "true" : "false");
    
    ts.endGroup();

    return ts.release().utf8();
}

void ViewportConfiguration::dump() const
{
    WTFLogAlways("%s", description().data());
}

#endif

} // namespace WebCore
