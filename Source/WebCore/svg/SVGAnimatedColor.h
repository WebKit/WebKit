/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGAnimatedColor_h
#define SVGAnimatedColor_h

#include "SVGAnimatedTypeAnimator.h"

namespace WebCore {

class SVGAnimatedColorAnimator final : public SVGAnimatedTypeAnimator {
public:
    SVGAnimatedColorAnimator(SVGAnimationElement&, SVGElement&);

private:
    std::unique_ptr<SVGAnimatedType> constructFromString(const String&) final;
    std::unique_ptr<SVGAnimatedType> startAnimValAnimation(const SVGElementAnimatedPropertyList&) final { return nullptr; }
    void stopAnimValAnimation(const SVGElementAnimatedPropertyList&) final { }
    void resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType&) final { }
    void animValWillChange(const SVGElementAnimatedPropertyList&) final { }
    void animValDidChange(const SVGElementAnimatedPropertyList&) final { }
    void addAnimatedTypes(SVGAnimatedType*, SVGAnimatedType*) final;
    void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*, SVGAnimatedType*) final;
    float calculateDistance(const String& fromString, const String& toString) final;
};

} // namespace WebCore

#endif
