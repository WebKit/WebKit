/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMCustom.h"

#include "JSMainThreadExecState.h"
#include "WebKitDOMHTMLInputElement.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMHTMLMediaElementPrivate.h"
#include "WebKitDOMHTMLTextAreaElement.h"
#include "WebKitDOMHTMLTextAreaElementPrivate.h"
#include "WebKitDOMPrivate.h"
#include "gobject/ConvertToUTF8String.h"

#if ENABLE(VIDEO)
#include "TextTrack.h"
#include "WebKitDOMTextTrackPrivate.h"
#endif

using namespace WebKit;

gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* area)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(area), FALSE);

    return core(area)->lastChangeWasUserEdit();
}

gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(input), FALSE);

    return core(input)->lastChangeWasUserEdit();
}


