/* This file is part of the KDE project

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1998, 1999 Torben Weis (weis@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)

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


/**
 * @libdoc KDE HTML widget
 *
 * This library provides a full-featured HTML parser and widget. It is
 * used for rendering in all KDE applications which allow HTML viewing,
 * including the Konqueror browser/file manager and the KDE Help
 * system.
 *
 * This library provides (will provide)
 * full HTML4 support, support for embedding Java applets, and will at some
 * point provide support for cascading style sheets
 * (CSS) and JavaScript.
 *
 * If you want to add to your application a widget that only needs simple text
 * browsing, you can also use the @ref KTextBrowser widget in kdeui.
 *
 * @ref KHTMLPart :
 *   The main part/widget for using khtml.
 *
 * @ref DOM :
 *   The dom implementation used in khtml.
 *
 */

/**
 *
 * The Document Object Model (DOM) is divided into two parts, the
 * @ref COREDOM core
 * DOM, specifying some core functionality, and the @ref HTMLDOM HTML DOM,
 * which deals with the extensions needed for HTML.
 *
 *
 */
namespace DOM
{
};
