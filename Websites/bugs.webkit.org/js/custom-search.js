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
 * The Initial Developer of the Original Code is BugzillaSource, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2011 
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *   Max Kanat-Alexander <mkanat@bugzilla.org>
 */

var PAREN_INDENT_EM = 2;
var ANY_ALL_SELECT_CLASS = 'any_all_select';

// When somebody chooses to "Hide Advanced Features" for Custom Search,
// we don't want to hide "Not" checkboxes if they've been checked. We
// accomplish this by removing the custom_search_advanced class from Not
// checkboxes when they are checked.
//
// We never add the custom_search_advanced class back. If we did, TUI
// would get confused in this case: Check Not, Hide Advanced Features,
// Uncheck Not, Show Advanced Features. (It hides "Not" when it shouldn't.)
function custom_search_not_changed(id) {
    var container = document.getElementById('custom_search_not_container_' + id);
    YAHOO.util.Dom.removeClass(container, 'custom_search_advanced');

    fix_query_string(container);
}

function custom_search_new_row() {
    var row = document.getElementById('custom_search_last_row');
    var clone = row.cloneNode(true);
    
    _cs_fix_ids(clone);
   
    // We only want one copy of the buttons, in the new row. So the old
    // ones get deleted.
    var op_button = document.getElementById('op_button');
    row.removeChild(op_button);
    var cp_button = document.getElementById('cp_container');
    row.removeChild(cp_button);
    var add_button = document.getElementById('add_button');
    row.removeChild(add_button);
    _remove_any_all(clone);

    // Always make sure there's only one row with this id.
    row.id = null;
    row.parentNode.appendChild(clone);
    fix_query_string(row);
    return clone;
}

function custom_search_open_paren() {
    var row = document.getElementById('custom_search_last_row');

    // If there's an "Any/All" select in this row, it needs to stay as
    // part of the parent paren set.
    var old_any_all = _remove_any_all(row);
    if (old_any_all) {
        var any_all_row = row.cloneNode(false);
        any_all_row.id = null;
        any_all_row.appendChild(old_any_all);
        row.parentNode.insertBefore(any_all_row, row);
    }

    // We also need a "Not" checkbox to stay in the parent paren set.
    var new_not = YAHOO.util.Dom.getElementsByClassName(
        'custom_search_not_container', null, row);
    var not_for_paren = new_not[0].cloneNode(true);

    // Preserve the values when modifying the row.
    var id = _cs_fix_ids(row, true);
    var prev_id = id - 1;

    var paren_row = row.cloneNode(false);
    paren_row.id = null;
    paren_row.innerHTML = '(<input type="hidden" name="f' + prev_id
                        + '" value="OP">';
    paren_row.insertBefore(not_for_paren, paren_row.firstChild);
    row.parentNode.insertBefore(paren_row, row);
    
    // New paren set needs a new "Any/All" select.
    var any_all_container = document.createElement('div');
    YAHOO.util.Dom.addClass(any_all_container, ANY_ALL_SELECT_CLASS);
    var j_top = document.getElementById('j_top');
    var any_all = j_top.cloneNode(true);
    any_all.name = 'j' + prev_id;
    any_all.id = any_all.name;
    any_all_container.appendChild(any_all);
    row.insertBefore(any_all_container, row.firstChild);

    var margin = YAHOO.util.Dom.getStyle(row, 'margin-left');
    var int_match = margin.match(/\d+/);
    var new_margin = parseInt(int_match[0]) + PAREN_INDENT_EM;
    YAHOO.util.Dom.setStyle(row, 'margin-left', new_margin + 'em');
    YAHOO.util.Dom.removeClass('cp_container', 'bz_default_hidden');

    fix_query_string(any_all_container);
}

function custom_search_close_paren() {
    var new_row = custom_search_new_row();
    
    // We need to up the new row's id by one more, because we're going
    // to insert a "CP" before it.
    var id = _cs_fix_ids(new_row);

    var margin = YAHOO.util.Dom.getStyle(new_row, 'margin-left');
    var int_match = margin.match(/\d+/);
    var new_margin = parseInt(int_match[0]) - PAREN_INDENT_EM;
    YAHOO.util.Dom.setStyle(new_row, 'margin-left', new_margin + 'em');

    var paren_row = new_row.cloneNode(false);
    paren_row.id = null;
    paren_row.innerHTML = ')<input type="hidden" name="f' + (id - 1)
                        + '" value="CP">';
  
    new_row.parentNode.insertBefore(paren_row, new_row);

    if (new_margin == 0) {
        YAHOO.util.Dom.addClass('cp_container', 'bz_default_hidden');
    }

    fix_query_string(new_row);
}

// When a user goes Back in their browser after searching, some browsers
// (Chrome, as of September 2011) do not remember the DOM that was created
// by the Custom Search JS. (In other words, their whole entered Custom
// Search disappears.) The solution is to update the History object,
// using the query string, which query.cgi can read to re-create the page
// exactly as the user had it before.
function fix_query_string(form_member) {
    // window.History comes from history.js.
    // It falls back to the normal window.history object for HTML5 browsers.
    if (!(window.History && window.History.replaceState))
        return;

    var form = YAHOO.util.Dom.getAncestorByTagName(form_member, 'form');
    var query = YAHOO.util.Connect.setForm(form);
    window.History.replaceState(null, document.title, '?' + query);
}

// Browsers that do not support HTML5's history.replaceState gain an emulated
// version via history.js, with the full query_string in the location's hash.
// We rewrite the URL to include the hash and redirect to the new location,
// which causes custom search fields to work.
function redirect_html4_browsers() {
    var hash = document.location.hash;
    if (hash == '') return;
    hash = hash.substring(1);
    if (hash.substring(0, 10) != 'query.cgi?') return;
    var url = document.location + "";
    url = url.substring(0, url.indexOf('query.cgi#')) + hash;
    document.location = url;
}

function _cs_fix_ids(parent, preserve_values) {
    // Update the label of the checkbox.
    var label = YAHOO.util.Dom.getElementBy(function() { return true },
                                            'label', parent);
    var id_match = label.htmlFor.match(/\d+$/);
    var id = parseInt(id_match[0]) + 1;
    label.htmlFor = label.htmlFor.replace(/\d+$/, id);

    // Sets all the inputs in the parent back to their default
    // and fixes their id.
    var fields =
        YAHOO.util.Dom.getElementsByClassName('custom_search_form_field', null,
                                              parent);
    for (var i = 0; i < fields.length; i++) {
        var field = fields[i];

        if (!preserve_values) {
            if (field.type == "checkbox") {
                field.checked = false;
            }
            else {
                field.value = '';
            }
        }
        
        // Update the numeric id for the new row.
        field.name = field.name.replace(/\d+$/, id);
        field.id = field.name;
    }
    
    return id;
}

function _remove_any_all(parent) {
    var any_all = YAHOO.util.Dom.getElementsByClassName(
        ANY_ALL_SELECT_CLASS, null, parent);
    if (any_all[0]) {
        parent.removeChild(any_all[0]);
        return any_all[0];
    }
    return null;
}
