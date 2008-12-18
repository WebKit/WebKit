/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Animation.h"

#include "RenderStyle.h"

namespace WebCore {

Animation::Animation()
    : m_delay(RenderStyle::initialAnimationDelay())
    , m_direction(RenderStyle::initialAnimationDirection())
    , m_duration(RenderStyle::initialAnimationDuration())
    , m_iterationCount(RenderStyle::initialAnimationIterationCount())
    , m_name(RenderStyle::initialAnimationName())
    , m_property(RenderStyle::initialAnimationProperty())
    , m_timingFunction(RenderStyle::initialAnimationTimingFunction())
    , m_playState(RenderStyle::initialAnimationPlayState())
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timingFunctionSet(false)
    , m_isNone(false)
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_delay(o.m_delay)
    , m_direction(o.m_direction)
    , m_duration(o.m_duration)
    , m_iterationCount(o.m_iterationCount)
    , m_name(o.m_name)
    , m_property(o.m_property)
    , m_timingFunction(o.m_timingFunction)
    , m_playState(o.m_playState)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_isNone(o.m_isNone)
{
}

Animation& Animation::operator=(const Animation& o)
{
    m_delay = o.m_delay;
    m_direction = o.m_direction;
    m_duration = o.m_duration;
    m_iterationCount = o.m_iterationCount;
    m_name = o.m_name;
    m_playState = o.m_playState;
    m_property = o.m_property;
    m_timingFunction = o.m_timingFunction;

    m_delaySet = o.m_delaySet;
    m_directionSet = o.m_directionSet;
    m_durationSet = o.m_durationSet;
    m_iterationCountSet = o.m_iterationCountSet;
    m_nameSet = o.m_nameSet;
    m_playStateSet = o.m_playStateSet;
    m_propertySet = o.m_propertySet;
    m_timingFunctionSet = o.m_timingFunctionSet;

    m_isNone = o.m_isNone;

    return *this;
}

Animation::~Animation()
{
}

bool Animation::animationsMatch(const Animation* o, bool matchPlayStates) const
{
    if (!o)
        return false;
    
    bool result = m_delay == o->m_delay &&
                  m_direction == o->m_direction &&
                  m_duration == o->m_duration &&
                  m_iterationCount == o->m_iterationCount &&
                  m_name == o->m_name &&
                  m_property == o->m_property && 
                  m_timingFunction == o->m_timingFunction &&
                  m_delaySet == o->m_delaySet &&
                  m_directionSet == o->m_directionSet &&
                  m_durationSet == o->m_durationSet &&
                  m_iterationCountSet == o->m_iterationCountSet &&
                  m_nameSet == o->m_nameSet &&
                  m_propertySet == o->m_propertySet &&
                  m_timingFunctionSet == o->m_timingFunctionSet &&
                  m_isNone == o->m_isNone;

    if (!result)
        return false;

    return !matchPlayStates || (m_playState == o->m_playState && m_playStateSet == o->m_playStateSet);
}

} // namespace WebCore
/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Animation.h"

#include "RenderStyle.h"

namespace WebCore {

Animation::Animation()
    : m_delay(RenderStyle::initialAnimationDelay())
    , m_direction(RenderStyle::initialAnimationDirection())
    , m_duration(RenderStyle::initialAnimationDuration())
    , m_iterationCount(RenderStyle::initialAnimationIterationCount())
    , m_name(RenderStyle::initialAnimationName())
    , m_property(RenderStyle::initialAnimationProperty())
    , m_timingFunction(RenderStyle::initialAnimationTimingFunction())
    , m_playState(RenderStyle::initialAnimationPlayState())
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timingFunctionSet(false)
    , m_isNone(false)
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_delay(o.m_delay)
    , m_direction(o.m_direction)
    , m_duration(o.m_duration)
    , m_iterationCount(o.m_iterationCount)
    , m_name(o.m_name)
    , m_property(o.m_property)
    , m_timingFunction(o.m_timingFunction)
    , m_playState(o.m_playState)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_isNone(o.m_isNone)
{
}

Animation& Animation::operator=(const Animation& o)
{
    m_delay = o.m_delay;
    m_direction = o.m_direction;
    m_duration = o.m_duration;
    m_iterationCount = o.m_iterationCount;
    m_name = o.m_name;
    m_playState = o.m_playState;
    m_property = o.m_property;
    m_timingFunction = o.m_timingFunction;

    m_delaySet = o.m_delaySet;
    m_directionSet = o.m_directionSet;
    m_durationSet = o.m_durationSet;
    m_iterationCountSet = o.m_iterationCountSet;
    m_nameSet = o.m_nameSet;
    m_playStateSet = o.m_playStateSet;
    m_propertySet = o.m_propertySet;
    m_timingFunctionSet = o.m_timingFunctionSet;

    m_isNone = o.m_isNone;

    return *this;
}

Animation::~Animation()
{
}

bool Animation::animationsMatch(const Animation* o, bool matchPlayStates) const
{
    if (!o)
        return false;
    
    bool result = m_delay == o->m_delay &&
                  m_direction == o->m_direction &&
                  m_duration == o->m_duration &&
                  m_iterationCount == o->m_iterationCount &&
                  m_name == o->m_name &&
                  m_property == o->m_property && 
                  m_timingFunction == o->m_timingFunction &&
                  m_delaySet == o->m_delaySet &&
                  m_directionSet == o->m_directionSet &&
                  m_durationSet == o->m_durationSet &&
                  m_iterationCountSet == o->m_iterationCountSet &&
                  m_nameSet == o->m_nameSet &&
                  m_propertySet == o->m_propertySet &&
                  m_timingFunctionSet == o->m_timingFunctionSet &&
                  m_isNone == o->m_isNone;

    if (!result)
        return false;

    return !matchPlayStates || (m_playState == o->m_playState && m_playStateSet == o->m_playStateSet);
}

} // namespace WebCore
