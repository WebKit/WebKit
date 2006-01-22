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
#if SVG_SUPPORT

#include <qcolor.h>
#include <qstring.h>

class KConfig;
struct KPerDomainSettings;

// browser window color defaults -- Bernd
#define DOM_DEFAULT_LNK_COLOR Qt::blue
#define DOM_DEFAULT_TXT_COLOR Qt::black
#define DOM_DEFAULT_VLNK_COLOR Qt::magenta
#define DOM_DEFAULT_BASE_COLOR Qt::white

// KEEP IN SYNC WITH konqdefaults.h in kdebase/libkonq!
// lets be modern .. -- Bernd
#define DOM_DEFAULT_VIEW_FONT QString::fromLatin1("Sans Serif")
#define DOM_DEFAULT_VIEW_FIXED_FONT QString::fromLatin1("Monospace")
#define DOM_DEFAULT_VIEW_SERIF_FONT QString::fromLatin1("Serif")
#define DOM_DEFAULT_VIEW_SANSSERIF_FONT QString::fromLatin1("Sans Serif")
#define DOM_DEFAULT_VIEW_CURSIVE_FONT QString::fromLatin1("Sans Serif")
#define DOM_DEFAULT_VIEW_FANTASY_FONT QString::fromLatin1("Sans Serif")
#define DOM_DEFAULT_MIN_FONT_SIZE 7 // everything smaller is usually unreadable.

namespace KDOM
{
    /**
     * General KDOM settings....
     */
    class KDOMSettings
    {
    public:
        /**
         * This enum specifies whether Java/JavaScript execution is allowed.
         */
        enum KJavaScriptAdvice
        {
            KJavaScriptDunno = 0,
            KJavaScriptAccept,
            KJavaScriptReject
        };

        enum KAnimationAdvice
        {
            KAnimationDisabled = 0,
            KAnimationLoopOnce,
            KAnimationEnabled
        };

        /**
         * This enum specifies the policy for window.open
         */
        enum KJSWindowOpenPolicy
        {
            KJSWindowOpenAllow = 0,
            KJSWindowOpenAsk,
            KJSWindowOpenDeny,
            KJSWindowOpenSmart
        };
        
        /**
         * This enum specifies the policy for window.status and .defaultStatus
         */
        enum KJSWindowStatusPolicy
        {
            KJSWindowStatusAllow = 0,
            KJSWindowStatusIgnore
        };

        /**
         * This enum specifies the policy for window.moveBy and .moveTo
         */
        enum KJSWindowMovePolicy
        {
            KJSWindowMoveAllow = 0,
            KJSWindowMoveIgnore
        };

        /**
         * This enum specifies the policy for window.resizeBy and .resizeTo
         */
        enum KJSWindowResizePolicy
        {
            KJSWindowResizeAllow = 0,
            KJSWindowResizeIgnore
        };

        /**
         * This enum specifies the policy for window.focus
         */
        enum KJSWindowFocusPolicy
        {
            KJSWindowFocusAllow = 0,
            KJSWindowFocusIgnore
        };

        KDOMSettings();
        KDOMSettings(const KDOMSettings &other);
        virtual ~KDOMSettings();

        /**
         * Called by constructor and reparseConfiguration
         */
        virtual void init();

        /**
         * Read settings from @p config.
         * @param config is a pointer to KConfig object.
         * @param reset if true, settings are always set; if false,
         *        settings are only set if the config file has a corresponding key.
         */
        virtual void init(KConfig *config, bool reset = true);

        // Behaviour settings
        KAnimationAdvice showAnimations() const;

        KJSWindowOpenPolicy windowOpenPolicy(const QString &hostname = QString::null) const;
        KJSWindowMovePolicy windowMovePolicy(const QString &hostname = QString::null) const;
        KJSWindowResizePolicy windowResizePolicy(const QString &hostname = QString::null) const;
        KJSWindowStatusPolicy windowStatusPolicy(const QString &hostname = QString::null) const;
        KJSWindowFocusPolicy windowFocusPolicy(const QString &hostname = QString::null) const;

        // Font settings
        QString stdFontName() const;
        QString fixedFontName() const;
        QString serifFontName() const;
        QString sansSerifFontName() const;
        QString cursiveFontName() const;
        QString fantasyFontName() const;

        int minFontSize() const;
        int mediumFontSize() const;

        const QString &encoding() const;

        // Color settings
        const QColor &textColor() const;
        const QColor &baseColor() const;
        const QColor &linkColor() const;
        const QColor &vLinkColor() const;

        // Java and JavaScript
        bool isJavaEnabled(const QString &hostname = QString::null) const;
        bool isJavaScriptEnabled(const QString &hostname = QString::null) const;
        bool isJavaScriptDebugEnabled(const QString &hostname = QString::null) const;
        bool isJavaScriptErrorReportingEnabled(const QString &hostname = QString::null) const;
        bool isPluginsEnabled(const QString &hostname = QString::null) const;
        
        // AdBlocK Filtering
        bool isAdFiltered(const QString &url) const;
        bool isAdFilterEnabled() const;
        bool isHideAdsEnabled() const;
        void addAdFilter(const QString &url);

        // Whether to show passive popup when windows are blocked (@since 3.5)
        bool jsPopupBlockerPassivePopup() const;
        void setJSPopupBlockerPassivePopup(bool enabled);

        bool jsErrorsEnabled() const;
        void setJSErrorsEnabled(bool enabled);

        // helpers for parsing domain-specific configuration, used in KControl module as well
        static KJavaScriptAdvice strToAdvice(const QString &str);
        static void splitDomainAdvice(const QString &configStr, QString &domain,
                                      KJavaScriptAdvice &javaAdvice, KJavaScriptAdvice &javaScriptAdvice);

        static const char *adviceToStr(KJavaScriptAdvice _advice);

        /** reads from @p config's current group, forcing initialization if @p reset is true.
         * @param config is a pointer to KConfig object.
         * @param reset true if initialization is to be forced.
         * @param global true if the global domain is to be read.
         * @param pd_settings will be initialised with the computed (inherited) settings.
         */
        virtual void readDomainSettings(KConfig *config, bool reset, bool global, KPerDomainSettings &pd_settings);

        // CSS helpers
        virtual QString settingsToCSS() const;
        QString userStyleSheet() const;

    private:
        QString lookupFont(unsigned int i) const;

        class Private;
        Private *d;
    };
}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
