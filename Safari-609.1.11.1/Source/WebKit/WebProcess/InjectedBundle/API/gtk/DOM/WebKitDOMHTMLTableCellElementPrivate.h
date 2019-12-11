/*
 *  This file is part of the WebKit open source project.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef WebKitDOMHTMLTableCellElementPrivate_h
#define WebKitDOMHTMLTableCellElementPrivate_h

#include <WebCore/HTMLTableCellElement.h>
#include <webkitdom/WebKitDOMHTMLTableCellElement.h>

namespace WebKit {
WebKitDOMHTMLTableCellElement* wrapHTMLTableCellElement(WebCore::HTMLTableCellElement*);
WebKitDOMHTMLTableCellElement* kit(WebCore::HTMLTableCellElement*);
WebCore::HTMLTableCellElement* core(WebKitDOMHTMLTableCellElement*);
} // namespace WebKit

#endif /* WebKitDOMHTMLTableCellElementPrivate_h */
