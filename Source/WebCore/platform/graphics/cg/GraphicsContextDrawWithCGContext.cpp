/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextDrawWithCGContext.h"

#include "LocalCurrentGraphicsContext.h"
#include <optional>
#include <pal/spi/cg/CoreGraphicsSPI.h>
namespace WebCore {

GraphicsContextDrawWithCGContext::GraphicsContextDrawWithCGContext() = default;

GraphicsContextDrawWithCGContext::~GraphicsContextDrawWithCGContext() = default;

void GraphicsContextDrawWithCGContext::drawWithCGContext(CGContextRef context, const Function<void(CGContextRef)>& draw, StateOptions stateOptions)
{
    std::optional<LocalCurrentGraphicsContext> scopedToolkitContext;
    if (stateOptions == StateOptions::Save)
        CGContextSaveGState(context);
    else if (stateOptions == StateOptions::SaveAndSetGlobalToolkitContext)
        scopedToolkitContext.emplace(context);
    draw(context);
    if (stateOptions == StateOptions::Save)
        CGContextRestoreGState(context);
    CGContextBeginPath(context); // Path is not part of the GState. Clear it.
}

}
