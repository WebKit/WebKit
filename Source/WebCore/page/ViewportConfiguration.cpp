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
#include "WebCoreSystemInterface.h"
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
    , m_minimumLayoutSizeForMinimalUI(m_minimumLayoutSize)
    , m_usesMinimalUI(false)
    , m_pageDidFinishDocumentLoad(false)
{
    // Setup a reasonable default configuration to avoid computing infinite scale/sizes.
    // Those are the original iPhone configuration.
    m_defaultConfiguration = ViewportConfiguration::webpageParameters();
    updateConfiguration();
}

void ViewportConfiguration::setDefaultConfiguration(const ViewportConfiguration::Parameters& defaultConfiguration)
{
    ASSERT(!constraintsAreAllRelative(m_configuration));
    ASSERT(!m_defaultConfiguration.initialScaleIsSet || defaultConfiguration.initialScale > 0);
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
}

void ViewportConfiguration::setMinimumLayoutSizeForMinimalUI(const FloatSize& minimumLayoutSizeForMinimalUI)
{
    if (m_minimumLayoutSizeForMinimalUI == minimumLayoutSizeForMinimalUI)
        return;

    m_minimumLayoutSizeForMinimalUI = minimumLayoutSizeForMinimalUI;
}

const FloatSize& ViewportConfiguration::activeMinimumLayoutSizeInScrollViewCoordinates() const
{
    if (m_usesMinimalUI)
        return m_minimumLayoutSizeForMinimalUI;
    return m_minimumLayoutSize;
}

void ViewportConfiguration::setViewportArguments(const ViewportArguments& viewportArguments)
{
    if (m_viewportArguments == viewportArguments)
        return;

    m_viewportArguments = viewportArguments;
    updateConfiguration();
}

void ViewportConfiguration::resetMinimalUI()
{
    m_usesMinimalUI = false;
    m_pageDidFinishDocumentLoad = false;
}

void ViewportConfiguration::didFinishDocumentLoad()
{
    m_pageDidFinishDocumentLoad = true;
}

IntSize ViewportConfiguration::layoutSize() const
{
    return IntSize(layoutWidth(), layoutHeight());
}

double ViewportConfiguration::initialScale() const
{
    ASSERT(!constraintsAreAllRelative(m_configuration));

    // If the document has specified its own initial scale, use it regardless.
    // This is guaranteed to be sanity checked already, so no need for MIN/MAX.
    if (m_configuration.initialScaleIsSet)
        return m_configuration.initialScale;

    // If not, it is up to us to determine the initial scale.
    // We want a scale small enough to fit the document width-wise.
    const FloatSize& minimumLayoutSize = activeMinimumLayoutSizeInScrollViewCoordinates();
    double width = m_contentSize.width() > 0 ? m_contentSize.width() : layoutWidth();
    double initialScale = 0;
    if (width > 0)
        initialScale = minimumLayoutSize.width() / width;

    // Prevent the intial scale from shrinking to a height smaller than our view's minimum height.
    double height = m_contentSize.height() > 0 ? m_contentSize.height() : layoutHeight();
    if (height > 0 && height * initialScale < minimumLayoutSize.height())
        initialScale = minimumLayoutSize.height() / height;
    return std::min(std::max(initialScale, m_configuration.minimumScale), m_configuration.maximumScale);
}

double ViewportConfiguration::minimumScale() const
{
    // If we scale to fit, then this is our minimum scale as well.
    if (!m_configuration.initialScaleIsSet)
        return initialScale();

    // If not, we still need to sanity check our value.
    double minimumScale = m_configuration.minimumScale;

    const FloatSize& minimumLayoutSize = activeMinimumLayoutSizeInScrollViewCoordinates();
    double contentWidth = m_contentSize.width();
    if (contentWidth > 0 && contentWidth * minimumScale < minimumLayoutSize.width())
        minimumScale = minimumLayoutSize.width() / contentWidth;

    double contentHeight = m_contentSize.height();
    if (contentHeight > 0 && contentHeight * minimumScale < minimumLayoutSize.height())
        minimumScale = minimumLayoutSize.height() / contentHeight;

    minimumScale = std::min(std::max(minimumScale, m_configuration.minimumScale), m_configuration.maximumScale);

    return minimumScale;
}

ViewportConfiguration::Parameters ViewportConfiguration::webpageParameters()
{
    Parameters parameters;
    parameters.width = 980;
    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
    parameters.minimumScale = 0.25;
    parameters.maximumScale = 5;
    return parameters;
}

ViewportConfiguration::Parameters ViewportConfiguration::textDocumentParameters()
{
    Parameters parameters;

#if PLATFORM(IOS)
    parameters.width = static_cast<int>(wkGetScreenSize().width);
#else
    // FIXME: this needs to be unified with ViewportArguments on all ports.
    parameters.width = 320;
#endif

    parameters.widthIsSet = true;
    parameters.allowsUserScaling = true;
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

static inline bool viewportArgumentUserZoomIsSet(float value)
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

    if (viewportArgumentUserZoomIsSet(m_viewportArguments.userZoom))
        m_configuration.allowsUserScaling = m_viewportArguments.userZoom != 0.;

#if PLATFORM(IOS)
    if (!m_pageDidFinishDocumentLoad)
        m_usesMinimalUI = m_usesMinimalUI || m_viewportArguments.minimalUI;
#endif
}

double ViewportConfiguration::viewportArgumentsLength(double length) const
{
    if (length == ViewportArguments::ValueDeviceWidth)
        return activeMinimumLayoutSizeInScrollViewCoordinates().width();
    if (length == ViewportArguments::ValueDeviceHeight)
        return activeMinimumLayoutSizeInScrollViewCoordinates().height();
    return length;
}

int ViewportConfiguration::layoutWidth() const
{
    ASSERT(!constraintsAreAllRelative(m_configuration));

    const FloatSize& minimumLayoutSize = activeMinimumLayoutSizeInScrollViewCoordinates();
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

    const FloatSize& minimumLayoutSize = activeMinimumLayoutSizeInScrollViewCoordinates();
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
class ViewportConfigurationTextStream : public TextStream {
public:
    ViewportConfigurationTextStream()
        : m_indent(0)
    {
    }

    using TextStream::operator<<;

    ViewportConfigurationTextStream& operator<<(const ViewportConfiguration::Parameters&);
    ViewportConfigurationTextStream& operator<<(const ViewportArguments&);

    void increaseIndent() { ++m_indent; }
    void decreaseIndent() { --m_indent; ASSERT(m_indent >= 0); }

    void writeIndent();

private:
    int m_indent;
};

template <typename T>
static void dumpProperty(ViewportConfigurationTextStream& ts, String name, T value)
{
    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(" << name << " ";
    ts << value << ")";
    ts.decreaseIndent();
}

void ViewportConfigurationTextStream::writeIndent()
{
    for (int i = 0; i < m_indent; ++i)
        *this << "  ";
}

ViewportConfigurationTextStream& ViewportConfigurationTextStream::operator<<(const ViewportConfiguration::Parameters& parameters)
{
    ViewportConfigurationTextStream& ts = *this;

    ts.increaseIndent();
    ts << "\n";
    ts.writeIndent();
    ts << "(width " << parameters.width << ", set: " << (parameters.widthIsSet ? "true" : "false") << ")";

    ts << "\n";
    ts.writeIndent();
    ts << "(height " << parameters.height << ", set: " << (parameters.heightIsSet ? "true" : "false") << ")";

    ts << "\n";
    ts.writeIndent();
    ts << "(initialScale " << parameters.width << ", set: " << (parameters.initialScaleIsSet ? "true" : "false") << ")";
    ts.decreaseIndent();

    dumpProperty(ts, "minimumScale", parameters.minimumScale);
    dumpProperty(ts, "maximumScale", parameters.maximumScale);
    dumpProperty(ts, "allowsUserScaling", parameters.allowsUserScaling);

    return ts;
}

ViewportConfigurationTextStream& ViewportConfigurationTextStream::operator<<(const ViewportArguments& viewportArguments)
{
    ViewportConfigurationTextStream& ts = *this;

    ts.increaseIndent();

    ts << "\n";
    ts.writeIndent();
    ts << "(width " << viewportArguments.width << ", minWidth " << viewportArguments.minWidth << ", maxWidth " << viewportArguments.maxWidth << ")";

    ts << "\n";
    ts.writeIndent();
    ts << "(height " << viewportArguments.height << ", minHeight " << viewportArguments.minHeight << ", maxHeight " << viewportArguments.maxHeight << ")";

    ts << "\n";
    ts.writeIndent();
    ts << "(zoom " << viewportArguments.zoom << ", minZoom " << viewportArguments.minZoom << ", maxZoom " << viewportArguments.maxZoom << ")";
    ts.decreaseIndent();

#if PLATFORM(IOS)
    dumpProperty(ts, "minimalUI", viewportArguments.minimalUI);
#endif
    return ts;
}

CString ViewportConfiguration::description() const
{
    ViewportConfigurationTextStream ts;

    ts << "(viewport-configuration " << (void*)this;
    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(viewport arguments";
    ts << m_viewportArguments;
    ts << ")";
    ts.decreaseIndent();

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(configuration";
    ts << m_configuration;
    ts << ")";
    ts.decreaseIndent();

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(default configuration";
    ts << m_defaultConfiguration;
    ts << ")";
    ts.decreaseIndent();

    dumpProperty(ts, "contentSize", m_contentSize);
    dumpProperty(ts, "minimumLayoutSize", m_minimumLayoutSize);
    dumpProperty(ts, "minimumLayoutSizeForMinimalUI", m_minimumLayoutSizeForMinimalUI);
    ts << "(uses minimal UI " << m_usesMinimalUI << ")";

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(computed initial scale " << initialScale() << ")\n";
    ts.writeIndent();
    ts << "(computed minimum scale " << minimumScale() << ")\n";
    ts.writeIndent();
    ts << "(computed layout size " << layoutSize() << ")";
    ts.decreaseIndent();

    ts << ")\n";

    return ts.release().utf8();
}

void ViewportConfiguration::dump() const
{
    fprintf(stderr, "%s", description().data());
}

#endif

} // namespace WebCore
