/*
 * Copyright (C) 2018, 2019, 2024 Igalia S.L
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

class WPEQtViewLoadRequestPrivate;

class QT_WPE_EXPORT WPEQtViewLoadRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url)
    Q_PROPERTY(WPEQtView::LoadStatus status READ status)
    Q_PROPERTY(QString errorString READ errorString)

public:
    explicit WPEQtViewLoadRequest(const QUrl&, WPEQtView::LoadStatus, const QString& errorString);
    virtual ~WPEQtViewLoadRequest();

    QUrl url() const;
    WPEQtView::LoadStatus status() const;
    QString errorString() const;

protected:
    bool event(QEvent*) override;

private:
    Q_DECLARE_PRIVATE(WPEQtViewLoadRequest)
    QScopedPointer<WPEQtViewLoadRequestPrivate> d_ptr;
};
