/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSCanvasRenderingContext2D.h"

#include "JSNodeCustom.h"
#include "WebCoreOpaqueRootInlines.h"

namespace WebCore {

bool JSCanvasRenderingContext2DOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, JSC::AbstractSlotVisitor& visitor, const char** reason)
{
    if (UNLIKELY(reason))
        *reason = "Canvas is opaque root";

    JSCanvasRenderingContext2D* jsCanvasRenderingContext = JSC::jsCast<JSCanvasRenderingContext2D*>(handle.slot()->asCell());
    return containsWebCoreOpaqueRoot(visitor, jsCanvasRenderingContext->wrapped().canvas());
}

template<typename Visitor>
void JSCanvasRenderingContext2D::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, wrapped().canvas());
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSCanvasRenderingContext2D);

} // namespace WebCore
