/*
 * Copyright (C) 2018, 2019 Igalia S.L
 * Copyright (C) 2018, 2019 Zodiac Inflight Innovations
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

#include <QObject>

class WPEQtViewLoadRequestPrivate;

class WPEQtViewLoadRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url)
    Q_PROPERTY(WPEQtView::LoadStatus status READ status)
    Q_PROPERTY(QString errorString READ errorString)

public:
    ~WPEQtViewLoadRequest();

    QUrl url() const;
    WPEQtView::LoadStatus status() const;
    QString errorString() const;

    explicit WPEQtViewLoadRequest(const WPEQtViewLoadRequestPrivate&);

private:
    friend class WPEQtView;

    Q_DECLARE_PRIVATE(WPEQtViewLoadRequest)
    QScopedPointer<WPEQtViewLoadRequestPrivate> d_ptr;
};

