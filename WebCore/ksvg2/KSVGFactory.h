/*
    Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_KSVGFactory_H
#define KSVG_KSVGFactory_H

#include <kurl.h>
#include <qptrlist.h>

#include <kparts/factory.h>
#include <kparts/historyprovider.h>

class KInstance;
class KAboutData;

namespace KSVG
{
	class KSVGPart;
	class KSVGSettings;
	class KSVGFactory : public KParts::Factory
	{
	public:
		KSVGFactory(bool clone = false);
		virtual ~KSVGFactory();

		virtual KParts::Part *createPartObject(QWidget *parentWidget, const char *widgetName,
											   QObject *parent, const char *name,
											   const char *className, const QStringList &args);

		static void registerPart(KSVGPart *part);
		static void deregisterPart(KSVGPart *part);

		static QPtrList<KSVGPart> *partList() { return s_parts; }

		static KInstance *instance();
		static KSVGSettings *defaultSVGSettings();
		
		// list of visited URLs
		static KParts::HistoryProvider *vLinks()
		{
			return KParts::HistoryProvider::self();
		}
		
	protected:
		static void ref();
		static void deref();

	private:
		static unsigned long s_refcnt;
		static KSVGFactory *s_self;
		
		static KInstance *s_instance;
		static KAboutData *s_about;
		static KSVGSettings *s_settings;
		
		static QPtrList<KSVGPart> *s_parts;
	};
}

#endif

// vim:ts=4:noet
