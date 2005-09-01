/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <qstring.h>

#include <kjs/ustring.h>
#include <kjs/identifier.h>

KJS::UString::UString(const QString &d)
{
    unsigned int len = d.length();
    KJS::UChar *dat = new UChar[len];
    memcpy(dat, d.unicode(), len * sizeof(UChar));
    rep = KJS::UString::Rep::create(dat, len);
}

QString KJS::UString::qstring() const
{
    return QString(reinterpret_cast<QChar *>(const_cast<KJS::UChar *>(data())), size());
}

QString KJS::Identifier::qstring() const
{
    return QString((QChar*) data(), size());
}

// vim:ts=4:noet
