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

#include "config.h"
#include "QtByteBlock.h"

#include <RefPtr.h>

namespace WebCore {

PassRefPtr<QtByteBlock> QtByteBlock::create(QSharedPointer<char> data)
{
    QtByteBlock* byteBlock = new QtByteBlock;
    byteBlock->m_data = data;
    return adoptRef(byteBlock);
}

qint64 QtByteBlock::size()
{
    return m_size;
}

qint64 QtByteBlock::capacity()
{
    return m_capacity;
}

const char* QtByteBlock::data()
{
    return (const char*)m_data.data();
}

void QtByteBlock::setSize(qint64 size)
{
    m_size = size;
}

void QtByteBlock::setCapacity(qint64 capacity)
{
    m_capacity = capacity;
}

QtByteBlock::QtByteBlock() : m_size(0), m_capacity(0)
{
}

}

