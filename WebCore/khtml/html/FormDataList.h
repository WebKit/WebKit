/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTML_FormDataList_h
#define HTML_FormDataList_h

#include <QString.h>
#include <qvaluelist.h>
#include "PlatformString.h"

class QTextCodec;

namespace DOM {

struct FormDataListItem {
    FormDataListItem(const QCString &data) : m_data(data) { }
    FormDataListItem(const QString &path) : m_path(path) { }

    QString m_path;
    QCString m_data;
};

class FormDataList {
public:
    FormDataList(QTextCodec *);

    void appendData(const DOMString &key, const DOMString &value)
        { appendString(key.qstring()); appendString(value.qstring()); }
    void appendData(const DOMString &key, const QString &value)
        { appendString(key.qstring()); appendString(value); }
    void appendData(const DOMString &key, const QCString &value)
        { appendString(key.qstring()); appendString(value); }
    void appendData(const DOMString &key, int value)
        { appendString(key.qstring()); appendString(QString::number(value)); }
    void appendFile(const DOMString &key, const DOMString &filename);

    QValueListConstIterator<FormDataListItem> begin() const
        { return m_list.begin(); }
    QValueListConstIterator<FormDataListItem> end() const
        { return m_list.end(); }

private:
    void appendString(const QCString &s);
    void appendString(const QString &s);

    QTextCodec *m_codec;
    QValueList<FormDataListItem> m_list;
};

};

#endif
