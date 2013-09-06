/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013,  Apple Inc. All rights reserved.
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

#ifndef RenderBlockFlow_h
#define RenderBlockFlow_h

#include "RenderBlock.h"

namespace WebCore {

class RenderBlockFlow : public RenderBlock {
public:
    explicit RenderBlockFlow(ContainerNode*);
    virtual ~RenderBlockFlow();
    
    virtual bool isRenderBlockFlow() const OVERRIDE FINAL { return true; }
};

inline RenderBlockFlow& toRenderBlockFlow(RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<RenderBlockFlow&>(object);
}

inline const RenderBlockFlow& toRenderBlockFlow(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<const RenderBlockFlow&>(object);
}

inline RenderBlockFlow* toRenderBlockFlow(RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<RenderBlockFlow*>(object);
}

inline const RenderBlockFlow* toRenderBlockFlow(const RenderObject* object)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<const RenderBlockFlow*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBlockFlow(const RenderBlockFlow*);
void toRenderBlockFlow(const RenderBlockFlow&);

} // namespace WebCore

#endif // RenderBlockFlow_h
