/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#ifndef KRenderingDeviceFactory_H
#define KRenderingDeviceFactory_H

#include <kinstance.h>

#include <qvaluelist.h>

class KCanvas;
class KCanvasTarget;
class KRenderingDevice;

struct KRenderingDeviceInfo
{
	QString name, internal;
};

class KRenderingDeviceFactory
{
public:
	KRenderingDeviceFactory();
	virtual ~KRenderingDeviceFactory();

	static KRenderingDeviceFactory *self();

	// Request a rendering device named 'device'
	KRenderingDevice *request(const QString &device) const;
	
	// Request a list of all available rendering devices
	const QValueList<KRenderingDeviceInfo> deviceList() const;

private:
	static KInstance *s_instance;
	static KRenderingDeviceFactory *s_factory;
};

#endif

// vim:ts=4:noet
