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

#ifndef KDOM_KDOMSettings_H
#define KDOM_KDOMSettings_H

class KConfig;

// browser window color defaults -- Bernd
#define DOM_DEFAULT_LNK_COLOR Qt::blue
#define DOM_DEFAULT_TXT_COLOR Qt::black
#define DOM_DEFAULT_VLNK_COLOR Qt::magenta
#define DOM_DEFAULT_BASE_COLOR Qt::white

// KEEP IN SYNC WITH konqdefaults.h in kdebase/libkonq!
// lets be modern .. -- Bernd
#define DOM_DEFAULT_VIEW_FONT QString::fromLatin1("helvetica")
#define DOM_DEFAULT_VIEW_FIXED_FONT QString::fromLatin1("courier")

// generic CSS fonts. Since usual X distributions don't have
// a good set of fonts, this is quite conservative...
#define DOM_DEFAULT_VIEW_SERIF_FONT QString::fromLatin1("times")
#define DOM_DEFAULT_VIEW_SANSSERIF_FONT QString::fromLatin1("helvetica")
#define DOM_DEFAULT_VIEW_CURSIVE_FONT QString::fromLatin1("helvetica")
#define DOM_DEFAULT_VIEW_FANTASY_FONT QString::fromLatin1("helvetica")
#define DOM_DEFAULT_MIN_FONT_SIZE 7 // everything smaller is usually unreadable.

namespace KDOM
{
	/**
	 * General KDOM settings....
	 */
	class KDOMSettings
	{
	public:
		KDOMSettings();
		KDOMSettings(const KDOMSettings &other);
		virtual ~KDOMSettings();

		/**
		 * Called by constructor and reparseConfiguration
		 */
		virtual void init();

		/**
		 * Read settings from @p config.
		 * @param reset if true, settings are always set; if false,
		 *  settings are only set if the config file has a corresponding key.
		 */
		virtual void init(KConfig *config, bool reset = true);

		// Font settings
		QString stdFontName() const;
		QString fixedFontName() const;
		QString serifFontName() const;
		QString sansSerifFontName() const;
		QString cursiveFontName() const;
		QString fantasyFontName() const;

		int minFontSize() const;
		int mediumFontSize() const;

		// Color settings
		const QColor &textColor() const;
		const QColor &baseColor() const;
		const QColor &linkColor() const;
		const QColor &vLinkColor() const;

	private:
		QString lookupFont(unsigned int i) const;

		class Private;
		Private *d;
	};
}

#endif

// vim:ts=4:noet
