/*
 * Copyright (C) 2026 Jochen Kühner (jochen.kuehner@gmx.de)
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

#pragma once

#include "BoxQuadOptions.h"
#include "ConvertCoordinateOptions.h"
#include "DOMPointInit.h"
#include "DOMQuadInit.h"
#include "ExceptionOr.h"
#include <wtf/Vector.h>

namespace WebCore {

class DOMPoint;
class DOMQuad;
class DOMRectReadOnly;
class Node;

namespace GeometryUtils {

ExceptionOr<Vector<Ref<DOMQuad>>> getBoxQuads(Node&, BoxQuadOptions&&);
ExceptionOr<Ref<DOMQuad>> convertQuadFromNode(Node&, DOMQuadInit&&, GeometryNode&&, ConvertCoordinateOptions&&);
ExceptionOr<Ref<DOMQuad>> convertRectFromNode(Node&, DOMRectReadOnly&, GeometryNode&&, ConvertCoordinateOptions&&);
ExceptionOr<Ref<DOMPoint>> convertPointFromNode(Node&, DOMPointInit&&, GeometryNode&&, ConvertCoordinateOptions&&);

} // namespace GeometryUtils
} // namespace WebCore
