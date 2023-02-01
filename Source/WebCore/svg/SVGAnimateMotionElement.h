/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "Path.h"
#include "SVGAnimationElement.h"

namespace WebCore {

class AffineTransform;
            
class SVGAnimateMotionElement final : public SVGAnimationElement {
    WTF_MAKE_ISO_ALLOCATED(SVGAnimateMotionElement);
public:
    static Ref<SVGAnimateMotionElement> create(const QualifiedName&, Document&);
    void updateAnimationPath();

private:
    SVGAnimateMotionElement(const QualifiedName&, Document&);

    bool hasValidAttributeType() const override;
    bool hasValidAttributeName() const override;

    void parseAttribute(const QualifiedName&, const AtomString&) override;

    void startAnimation() override;
    void stopAnimation(SVGElement* targetElement) override;
    bool setFromAndToValues(const String& fromString, const String& toString) override;
    bool setFromAndByValues(const String& fromString, const String& byString) override;
    bool setToAtEndOfDurationValue(const String& toAtEndOfDurationString) override;
    void calculateAnimatedValue(float percentage, unsigned repeatCount) override;
    void applyResultsToTarget() override;
    std::optional<float> calculateDistance(const String& fromString, const String& toString) override;

    enum RotateMode {
        RotateAngle,
        RotateAuto,
        RotateAutoReverse
    };
    RotateMode rotateMode() const;
    void buildTransformForProgress(AffineTransform*, float percentage);

    void updateAnimationMode() override;

    // Note: we do not support percentage values for to/from coords as the spec implies we should (opera doesn't either)
    FloatPoint m_fromPoint;
    FloatPoint m_toPoint;
    std::optional<FloatPoint> m_toPointAtEndOfDuration;

    Path m_path;
    Path m_animationPath;
};
    
} // namespace WebCore
