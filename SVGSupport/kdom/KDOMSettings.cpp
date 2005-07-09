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

#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>

#include <qcolor.h>

#include "KDOMSettings.h"

using namespace KDOM;

class KDOMSettings::Private
{
public:
	QStringList fonts;
	QStringList defaultFonts;
	
	int fontSize;
	int minFontSize;

	QColor textColor;
	QColor baseColor;
	QColor linkColor;
	QColor vLinkColor;
};

KDOMSettings::KDOMSettings() : d(new Private())
{
	init();
}

KDOMSettings::KDOMSettings(const KDOMSettings &other) : d(new Private())
{
	*d = *other.d;
}

KDOMSettings::~KDOMSettings()
{
	delete d;
}

void KDOMSettings::init()
{
	KConfig global(QString::fromLatin1("kdomrc"), true, false);
	init(&global, true);

	KConfig *local = KGlobal::config();
	if(!local)
		return;

	init(local, false);
}

void KDOMSettings::init(KConfig *config, bool reset)
{
#ifndef APPLE_COMPILE_HACK
	QString group_save = config->group();

	if(reset || config->hasGroup("DOM Settings"))
	{
		config->setGroup("DOM Settings");
	
		// Fonts and colors
		if(reset)
		{
			d->defaultFonts = QStringList();
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("StandardFont"), KGlobalSettings::generalFont().family()));
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("FixedFont"), KGlobalSettings::fixedFont().family()));
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("SerifFont"), DOM_DEFAULT_VIEW_SERIF_FONT));
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("SansSerifFont"), DOM_DEFAULT_VIEW_SANSSERIF_FONT));
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("CursiveFont"), DOM_DEFAULT_VIEW_CURSIVE_FONT));
			d->defaultFonts.append(config->readEntry(QString::fromLatin1("FantasyFont"), DOM_DEFAULT_VIEW_FANTASY_FONT));
			d->defaultFonts.append(QString::fromLatin1("0")); // font size adjustment
		}

		if(reset || config->hasKey("MinimumFontSize"))
			d->minFontSize = config->readNumEntry(QString::fromLatin1("MinimumFontSize"), DOM_DEFAULT_MIN_FONT_SIZE);

		if(reset || config->hasKey("MediumFontSize"))
			d->fontSize = config->readNumEntry(QString::fromLatin1("MediumFontSize"), 12);

		d->fonts = config->readListEntry("Fonts");
	}

	if(reset || config->hasGroup("General"))
	{
		config->setGroup("General"); // group will be restored by cgs anyway
		if(reset || config->hasKey("foreground"))
			d->textColor = config->readColorEntry("foreground", &DOM_DEFAULT_TXT_COLOR);

		if(reset || config->hasKey( "linkColor"))
			d->linkColor = config->readColorEntry("linkColor", &DOM_DEFAULT_LNK_COLOR);

		if(reset || config->hasKey( "visitedLinkColor"))
			d->vLinkColor = config->readColorEntry("visitedLinkColor", &DOM_DEFAULT_VLNK_COLOR);

		if(reset || config->hasKey( "background"))
			d->baseColor = config->readColorEntry( "background", &DOM_DEFAULT_BASE_COLOR);
	}

	config->setGroup(group_save);
#endif
}

QString KDOMSettings::stdFontName() const
{
	return lookupFont(0);
}

QString KDOMSettings::fixedFontName() const
{
	return lookupFont(1);
}

QString KDOMSettings::serifFontName() const
{
	return lookupFont(2);
}

QString KDOMSettings::sansSerifFontName() const
{
	return lookupFont(3);
}

QString KDOMSettings::cursiveFontName() const
{
	return lookupFont(4);
}

QString KDOMSettings::fantasyFontName() const
{
	return lookupFont(5);
}

int KDOMSettings::minFontSize() const
{
	return d->minFontSize;
}

int KDOMSettings::mediumFontSize() const
{
	return d->fontSize;
}

const QColor &KDOMSettings::baseColor() const
{
	return d->baseColor;
}

const QColor &KDOMSettings::linkColor() const
{
	return d->linkColor;
}

const QColor &KDOMSettings::vLinkColor() const
{
	return d->vLinkColor;
}

QString KDOMSettings::lookupFont(unsigned int i) const
{
	QString font;
	if(d->fonts.count() > i)
		font = d->fonts[i];

	if(font.isEmpty())
		font = d->defaultFonts[i];

	return font;
}

// vim:ts=4:noet
