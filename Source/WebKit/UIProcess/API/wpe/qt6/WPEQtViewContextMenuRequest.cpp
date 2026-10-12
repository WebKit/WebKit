/*
 * Copyright (C) 2026 tusooa <tusooa@kazv.moe>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WPEQtViewContextMenuRequest.h"

WPEQtViewContextMenuRequest::WPEQtViewContextMenuRequest(WebKitHitTestResult* result)
    : m_context(webkit_hit_test_result_get_context(result))
    , m_imageUri(webkit_hit_test_result_get_image_uri(result))
    , m_linkLabel(webkit_hit_test_result_get_link_label(result))
    , m_linkTitle(webkit_hit_test_result_get_link_title(result))
    , m_linkUri(webkit_hit_test_result_get_link_uri(result))
    , m_mediaUri(webkit_hit_test_result_get_media_uri(result))
{
}

WPEQtViewContextMenuRequest::~WPEQtViewContextMenuRequest() = default;

bool WPEQtViewContextMenuRequest::contextIsEditable() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
}

bool WPEQtViewContextMenuRequest::contextIsImage() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
}

bool WPEQtViewContextMenuRequest::contextIsLink() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK;
}

bool WPEQtViewContextMenuRequest::contextIsMedia() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
}

bool WPEQtViewContextMenuRequest::contextIsScrollbar() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR;
}

bool WPEQtViewContextMenuRequest::contextIsSelection() const
{
    return m_context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;
}

QString WPEQtViewContextMenuRequest::imageUri() const
{
    return m_imageUri;
}

QString WPEQtViewContextMenuRequest::linkLabel() const
{
    return m_linkLabel;
}

QString WPEQtViewContextMenuRequest::linkTitle() const
{
    return m_linkTitle;
}

QString WPEQtViewContextMenuRequest::linkUri() const
{
    return m_linkUri;
}

QString WPEQtViewContextMenuRequest::mediaUri() const
{
    return m_mediaUri;
}
