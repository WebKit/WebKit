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

#ifndef KSVG_KSVGSettings_H
#define KSVG_KSVGSettings_H

#include <kdom/KDOMSettings.h>

namespace KSVG
{
    /**
     * Settings for the SVG view.
     */
    class KSVGSettings : public KDOM::KDOMSettings
    {
    public:
        KSVGSettings();
        KSVGSettings(const KSVGSettings &other);
        virtual ~KSVGSettings();

        /**
         * Called by constructor and reparseConfiguration
         */
        virtual void init();

        /** Read settings from @p config.
         * @param reset if true, settings are always set; if false,
         *  settings are only set if the config file has a corresponding key.
         */
        virtual void init(KConfig *config, bool reset = true);

        // Settings
        QString parsingBackend() const;
        void setParsingBackend(const QString &backend) const;

        QString renderingBackend() const;
        void setRenderingBackend(const QString &backend) const;

        bool fontKerning() const;
        void setFontKerning(bool value);
        
        bool progressiveRendering() const;
        void setProgressiveRendering(bool value);

    private:
        class Private;
        Private *d;
    };
}

#endif

// vim:ts=4:noet
