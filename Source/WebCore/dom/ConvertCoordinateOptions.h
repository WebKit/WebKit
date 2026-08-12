/*
 * Copyright (C) 2026 Jochen Kühner (jochen.kuehner@gmx.de)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "GeometryBox.h"

namespace WebCore {

struct ConvertCoordinateOptions {
    GeometryBox fromBox { GeometryBox::Border };
    GeometryBox toBox { GeometryBox::Border };
};

} // namespace WebCore
