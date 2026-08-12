/*
 * Copyright (C) 2026 Jochen Kühner (jochen.kuehner@gmx.de)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
