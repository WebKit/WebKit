/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef QtByteBlock_h
#define QtByteBlock_h

#include <PassRefPtr.h>
#include <QSharedPointer>
#include <RefCounted.h>

namespace WebCore {

class QtByteBlock : public RefCounted<QtByteBlock> {
public:
    static PassRefPtr<QtByteBlock> create(QSharedPointer<char>);

    qint64 size();
    qint64 capacity();

    const char* data();

    void setSize(qint64);
    void setCapacity(qint64);
protected:
    QtByteBlock();
    qint64 m_size;
    qint64 m_capacity;
    QSharedPointer<char> m_data;
};

}

#endif // QtByteBlock_h

