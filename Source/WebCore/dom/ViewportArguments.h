/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef ViewportArguments_h
#define ViewportArguments_h

#include "FloatSize.h"
#include <wtf/Forward.h>

namespace WebCore {

class Document;

enum ViewportErrorCode {
    UnrecognizedViewportArgumentKeyError,
    UnrecognizedViewportArgumentValueError,
    TruncatedViewportArgumentValueError,
    MaximumScaleTooLargeError
};

struct ViewportAttributes {
    FloatSize layoutSize;

    float initialScale;
    float minimumScale;
    float maximumScale;

    float userScalable;
    float orientation;
};

struct ViewportArguments {

    enum Type {
        // These are ordered in increasing importance.
        Implicit,
#if PLATFORM(IOS)
        PluginDocument,
        ImageDocument,
#endif
        ViewportMeta,
        CSSDeviceAdaptation
    } type;

    enum {
        ValueAuto = -1,
        ValueDeviceWidth = -2,
        ValueDeviceHeight = -3,
        ValuePortrait = -4,
        ValueLandscape = -5
    };

    explicit ViewportArguments(Type type = Implicit)
        : type(type)
        , width(ValueAuto)
        , minWidth(ValueAuto)
        , maxWidth(ValueAuto)
        , height(ValueAuto)
        , minHeight(ValueAuto)
        , maxHeight(ValueAuto)
        , zoom(ValueAuto)
        , minZoom(ValueAuto)
        , maxZoom(ValueAuto)
        , userZoom(ValueAuto)
        , orientation(ValueAuto)
#if PLATFORM(IOS)
        , minimalUI(false)
#endif
    {
    }

    // All arguments are in CSS units.
    ViewportAttributes resolve(const FloatSize& initialViewportSize, const FloatSize& deviceSize, int defaultWidth) const;

    float width;
    float minWidth;
    float maxWidth;
    float height;
    float minHeight;
    float maxHeight;
    float zoom;
    float minZoom;
    float maxZoom;
    float userZoom;
    float orientation;
#if PLATFORM(IOS)
    bool minimalUI;
#endif

    bool operator==(const ViewportArguments& other) const
    {
        // Used for figuring out whether to reset the viewport or not,
        // thus we are not taking type into account.
        return width == other.width
            && minWidth == other.minWidth
            && maxWidth == other.maxWidth
            && height == other.height
            && minHeight == other.minHeight
            && maxHeight == other.maxHeight
            && zoom == other.zoom
            && minZoom == other.minZoom
            && maxZoom == other.maxZoom
            && userZoom == other.userZoom
#if PLATFORM(IOS)
            && minimalUI == other.minimalUI
#endif
            && orientation == other.orientation;
    }

    bool operator!=(const ViewportArguments& other) const
    {
        return !(*this == other);
    }

#if PLATFORM(GTK)
    // FIXME: We're going to keep this constant around until all embedders
    // refactor their code to no longer need it.
    static const float deprecatedTargetDPI;
#endif
};

WEBCORE_EXPORT ViewportAttributes computeViewportAttributes(ViewportArguments args, int desktopWidth, int deviceWidth, int deviceHeight, float devicePixelRatio, IntSize visibleViewport);

WEBCORE_EXPORT void restrictMinimumScaleFactorToViewportSize(ViewportAttributes& result, IntSize visibleViewport, float devicePixelRatio);
WEBCORE_EXPORT void restrictScaleFactorToInitialScaleIfNotUserScalable(ViewportAttributes& result);
float computeMinimumScaleFactorForContentContained(const ViewportAttributes& result, const IntSize& viewportSize, const IntSize& contentSize);

void setViewportFeature(const String& keyString, const String& valueString, Document*, void* data);
void reportViewportWarning(Document*, ViewportErrorCode, const String& replacement1, const String& replacement2);

} // namespace WebCore

#endif // ViewportArguments_h
