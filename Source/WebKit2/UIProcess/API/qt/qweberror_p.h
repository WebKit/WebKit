/*
 * Copyright (C) 2011 Andreas Kling <kling@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef qweberror_p_h
#define qweberror_p_h

#include "qweberror.h"
#include <QtCore/QSharedData>
#include <WKError.h>
#include <WKRetainPtr.h>

class QWebErrorPrivate : public QSharedData {
public:
    static QWebError createQWebError(WKErrorRef);
    QWebErrorPrivate(WKErrorRef);
    ~QWebErrorPrivate();

    WKRetainPtr<WKErrorRef> error;
};

#endif /* qweberror_p_h */
