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

#pragma once

#include "WPEQtView.h"

class QT_WPE_EXPORT WPEQtViewContextMenuRequest {
    Q_GADGET
    Q_PROPERTY(bool contextIsEditable READ contextIsEditable)
    Q_PROPERTY(bool contextIsImage READ contextIsImage)
    Q_PROPERTY(bool contextIsLink READ contextIsLink)
    Q_PROPERTY(bool contextIsMedia READ contextIsMedia)
    Q_PROPERTY(bool contextIsScrollbar READ contextIsScrollbar)
    Q_PROPERTY(bool contextIsSelection READ contextIsSelection)
    Q_PROPERTY(QString imageUri READ imageUri)
    Q_PROPERTY(QString linkLabel READ linkLabel)
    Q_PROPERTY(QString linkTitle READ linkTitle)
    Q_PROPERTY(QString linkUri READ linkUri)
    Q_PROPERTY(QString mediaUri READ mediaUri)

public:
    WPEQtViewContextMenuRequest(WebKitHitTestResult*);
    virtual ~WPEQtViewContextMenuRequest();
    QVariantList menuItems() const;
    bool contextIsEditable() const;
    bool contextIsImage() const;
    bool contextIsLink() const;
    bool contextIsMedia() const;
    bool contextIsScrollbar() const;
    bool contextIsSelection() const;
    QString imageUri() const;
    QString linkLabel() const;
    QString linkTitle() const;
    QString linkUri() const;
    QString mediaUri() const;

private:
    QVariantList m_menuItems;
    guint m_context;
    QString m_imageUri;
    QString m_linkLabel;
    QString m_linkTitle;
    QString m_linkUri;
    QString m_mediaUri;
};
