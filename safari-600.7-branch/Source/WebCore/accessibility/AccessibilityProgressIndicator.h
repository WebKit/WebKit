/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef AccessibilityProgressIndicator_h
#define AccessibilityProgressIndicator_h

#include "AccessibilityRenderObject.h"

namespace WebCore {

#if ENABLE(METER_ELEMENT)
class HTMLMeterElement;
class RenderMeter;
#endif

class HTMLProgressElement;
class RenderProgress;
    
class AccessibilityProgressIndicator : public AccessibilityRenderObject {
public:
    static PassRefPtr<AccessibilityProgressIndicator> create(RenderProgress*);
#if ENABLE(METER_ELEMENT)
    static PassRefPtr<AccessibilityProgressIndicator> create(RenderMeter*);
#endif

private:
    virtual AccessibilityRole roleValue() const override { return ProgressIndicatorRole; }

    virtual bool isProgressIndicator() const override { return true; }

    virtual float valueForRange() const override;
    virtual float maxValueForRange() const override;
    virtual float minValueForRange() const override;

    explicit AccessibilityProgressIndicator(RenderProgress*);
    HTMLProgressElement* progressElement() const;

#if ENABLE(METER_ELEMENT)
    explicit AccessibilityProgressIndicator(RenderMeter*);
    HTMLMeterElement* meterElement() const;
#endif
    
    virtual bool computeAccessibilityIsIgnored() const override;
};


} // namespace WebCore

#endif // AccessibilityProgressIndicator_h
