/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
#include "SpatialBackdropSource.h"

namespace WebCore {

SpatialBackdropSource::SpatialBackdropSource(URL&& sourceURL, URL&& modelURL, std::optional<URL>&& environmentMapURL)
    : m_sourceURL(WTFMove(sourceURL))
    , m_modelURL(WTFMove(modelURL))
    , m_environmentMapURL(WTFMove(environmentMapURL))
{
}

SpatialBackdropSource::SpatialBackdropSource(URL&& sourceURL, URL&& modelURL, URL&& environmentMapURL)
    : m_sourceURL(WTFMove(sourceURL))
    , m_modelURL(WTFMove(modelURL))
    , m_environmentMapURL(environmentMapURL.isValid() ? std::make_optional(WTFMove(environmentMapURL)) : std::nullopt)
{
}

} // namespace WebCore
#endif
