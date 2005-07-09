/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc

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

#include "ImageSource.h"

using namespace KDOM;

ImageSource::ImageSource(QByteArray buf) : buffer(buf), pos(0),
										   eof(false), rew(false), rewable(true)
{
}

int ImageSource::readyToSend()
{
	if(eof && pos == buffer.size())
		return -1;

	return buffer.size() - pos;
}

void ImageSource::sendTo(QDataSink *sink, int n)
{
	sink->receive((const unsigned char *) &buffer.at(pos), n);

	pos += n;

	// buffer is no longer needed
	if(eof && pos == buffer.size() && !rewable)
	{
		buffer.resize(0);
		pos = 0;
	}
}

void ImageSource::rewind()
{
	pos = 0;
	
	if(!rew)
		QDataSource::rewind();
	else
		ready();
}

void ImageSource::cleanBuffer()
{
	// if we need to be able to rewind, buffer is needed
	if(rew)
		return;

	rewable = false;

	// buffer is no longer needed
	if(eof && pos == buffer.size())
	{
		buffer.resize(0);
		pos = 0;
	}
}

// vim:ts=4:noet
