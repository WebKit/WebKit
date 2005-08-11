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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "KRenderingDevice.h"
#include "KRenderingDevice.moc"

KCanvasCommonArgs::KCanvasCommonArgs()
{
	m_path = 0;
	m_style = 0;
	m_canvas = 0;
}

KCanvasCommonArgs::~KCanvasCommonArgs()
{
}

KCanvas *KCanvasCommonArgs::canvas() const
{
	return m_canvas;
}

void KCanvasCommonArgs::setCanvas(KCanvas *canvas)
{
	m_canvas = canvas;
}

KCanvasUserData KCanvasCommonArgs::path() const
{
	return m_path;
}

void KCanvasCommonArgs::setPath(KCanvasUserData path)
{
	m_path = path;
}

KRenderingStyle *KCanvasCommonArgs::style() const
{
	return m_style;
}

void KCanvasCommonArgs::setStyle(KRenderingStyle *style)
{
	m_style = style;
}

KRenderingDevice::KRenderingDevice()
{
	m_currentPath = 0;
}

KRenderingDevice::~KRenderingDevice()
{
}

KCanvasUserData KRenderingDevice::currentPath() const
{
	return m_currentPath;
}

KRenderingDeviceContext *KRenderingDevice::currentContext() const
{
	return m_contextStack.current();
}

KRenderingDeviceContext *KRenderingDevice::popContext()
{
	return m_contextStack.pop();
}

void KRenderingDevice::pushContext(KRenderingDeviceContext *context)
{
	m_contextStack.push(context);
}

void KRenderingDevice::setCurrentPath(KCanvasUserData path)
{
	m_currentPath = path;
}

// vim:ts=4:noet
