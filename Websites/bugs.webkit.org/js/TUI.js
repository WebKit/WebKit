/* The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Bugzilla Bug Tracking System.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): Dennis Melentyev <dennis.melentyev@infopulse.com.ua>
 *                 Max Kanat-Alexander <mkanat@bugzilla.org>
 */

/* This file provides JavaScript functions to be included when one wishes
 * to show/hide certain UI elements, and have the state of them being
 * shown/hidden stored in a cookie.
 * 
 * TUI stands for Tweak UI.
 *
 * Requires js/util.js and the YUI Dom and Cookie libraries.
 *
 * See template/en/default/bug/create/create.html.tmpl for a usage example.
 */

var TUI_HIDDEN_CLASS = 'bz_tui_hidden';
var TUI_COOKIE_NAME  = 'TUI';

var TUI_alternates = new Array();

/** 
 * Hides a particular class of elements if they are shown, 
 * or shows them if they are hidden. Then it stores whether that
 * class is now hidden or shown.
 *
 * @param className   The name of the CSS class to hide.
 */
function TUI_toggle_class(className) {
    var elements = YAHOO.util.Dom.getElementsByClassName(className);
    for (var i = 0; i < elements.length; i++) {
        bz_toggleClass(elements[i], TUI_HIDDEN_CLASS);
    }
    _TUI_save_class_state(elements, className);
    _TUI_toggle_control_link(className);
}


/**
 * Specifies that a certain class of items should be hidden by default,
 * if the user doesn't have a TUI cookie.
 * 
 * @param className   The class to hide by default.
 */
function TUI_hide_default(className) {
    YAHOO.util.Event.onDOMReady(function () {
        if (!YAHOO.util.Cookie.getSub('TUI', className)) {
            TUI_toggle_class(className);
        }
    });
}

function _TUI_toggle_control_link(className) {
    var link = document.getElementById(className + "_controller");
    if (!link) return;
    var original_text = link.innerHTML;
    link.innerHTML = TUI_alternates[className];
    TUI_alternates[className] = original_text;
}

function _TUI_save_class_state(elements, aClass) {
    // We just check the first element to see if it's hidden or not, and
    // consider that all elements are the same.
    if (YAHOO.util.Dom.hasClass(elements[0], TUI_HIDDEN_CLASS)) {
        _TUI_store(aClass, 0);
    }
    else {
        _TUI_store(aClass, 1);
    }
}

function _TUI_store(aClass, state) {
    YAHOO.util.Cookie.setSub(TUI_COOKIE_NAME, aClass, state,
    {
        expires: new Date('January 1, 2038'),
        path: BUGZILLA.param.cookie_path
    });
}

function _TUI_restore() {
    var yui_classes = YAHOO.util.Cookie.getSubs(TUI_COOKIE_NAME);
    for (yui_item in yui_classes) {
        if (yui_classes[yui_item] == 0) {
            var elements = YAHOO.util.Dom.getElementsByClassName(yui_item);
            for (var i = 0; i < elements.length; i++) {
                YAHOO.util.Dom.addClass(elements[i], 'bz_tui_hidden');
            }
            _TUI_toggle_control_link(yui_item);
        }
    }
}

YAHOO.util.Event.onDOMReady(_TUI_restore);
