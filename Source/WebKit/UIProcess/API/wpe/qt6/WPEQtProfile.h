/*
 * Copyright (C) 2024 Igalia S.L.
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

class WPEQtProfilePrivate;

class Q_DECL_EXPORT WPEQtProfile : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString httpUserAgent READ httpUserAgent WRITE setHttpUserAgent NOTIFY httpUserAgentChanged FINAL)
    Q_PROPERTY(QString httpAcceptLanguage READ httpAcceptLanguage WRITE setHttpAcceptLanguage NOTIFY httpAcceptLanguageChanged FINAL)

public:
    WPEQtProfile(const QString& httpUserAgent = "", const QString& httpAcceptLanguage = "*");
    ~WPEQtProfile();

    QString httpUserAgent() const;
    void setHttpUserAgent(const QString&);

    QString httpAcceptLanguage() const;
    void setHttpAcceptLanguage(const QString&);

Q_SIGNALS:
    void httpUserAgentChanged();
    void httpAcceptLanguageChanged();

private:
    Q_DECLARE_PRIVATE(WPEQtProfile)
    QScopedPointer<WPEQtProfilePrivate> d_ptr;
};

