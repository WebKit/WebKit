/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
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
    
    _cs_fix_row_ids(clone);
   
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
    cs_reconfigure(row);
    fix_query_string(row);
    return clone;
}

var _cs_source_any_all;
function custom_search_open_paren() {
    var row = document.getElementById('custom_search_last_row');

    // create a copy of j_top and use that as the source, so we can modify
    // j_top if required
    if (!_cs_source_any_all) {
        var j_top = document.getElementById('j_top');
        _cs_source_any_all = j_top.cloneNode(true);
    }

    // find the parent any/all select, and remove the grouped option
    var structure = _cs_build_structure(row);
    var old_id = _cs_get_row_id(row);
    var parent_j = document.getElementById(_cs_get_join(structure, 'f' + old_id));
    _cs_remove_and_g(parent_j);

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
    var id = _cs_fix_row_ids(row, true);
    var prev_id = id - 1;

    var paren_row = row.cloneNode(false);
    paren_row.id = null;
    paren_row.innerHTML = '(<input type="hidden" name="f' + prev_id
                        + '" id="f' + prev_id + '" value="OP">';
    paren_row.insertBefore(not_for_paren, paren_row.firstChild);
    row.parentNode.insertBefore(paren_row, row);

    // New paren set needs a new "Any/All" select.
    var any_all_container = document.createElement('div');
    YAHOO.util.Dom.addClass(any_all_container, ANY_ALL_SELECT_CLASS);
    var any_all = _cs_source_any_all.cloneNode(true);
    any_all.name = 'j' + prev_id;
    any_all.id = any_all.name;
    any_all_container.appendChild(any_all);
    row.insertBefore(any_all_container, row.firstChild);

    var margin = YAHOO.util.Dom.getStyle(row, 'margin-left');
    var int_match = margin.match(/\d+/);
    var new_margin = parseInt(int_match[0]) + PAREN_INDENT_EM;
    YAHOO.util.Dom.setStyle(row, 'margin-left', new_margin + 'em');
    YAHOO.util.Dom.removeClass('cp_container', 'bz_default_hidden');

    cs_reconfigure(any_all_container);
    fix_query_string(any_all_container);
}

function custom_search_close_paren() {
    var new_row = custom_search_new_row();
    
    // We need to up the new row's id by one more, because we're going
    // to insert a "CP" before it.
    var id = _cs_fix_row_ids(new_row);

    var margin = YAHOO.util.Dom.getStyle(new_row, 'margin-left');
    var int_match = margin.match(/\d+/);
    var new_margin = parseInt(int_match[0]) - PAREN_INDENT_EM;
    YAHOO.util.Dom.setStyle(new_row, 'margin-left', new_margin + 'em');

    var paren_row = new_row.cloneNode(false);
    paren_row.id = null;
    paren_row.innerHTML = ')<input type="hidden" name="f' + (id - 1)
                        + '" id="f' + (id - 1) + '" value="CP">';
  
    new_row.parentNode.insertBefore(paren_row, new_row);

    if (new_margin == 0) {
        YAHOO.util.Dom.addClass('cp_container', 'bz_default_hidden');
    }

    cs_reconfigure(new_row);
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
    // Disable the token field so setForm doesn't include it
    var reenable_token = false;
    if (form['token'] && !form['token'].disabled) {
      form['token'].disabled = true;
      reenable_token = true;
    }
    var query = YAHOO.util.Connect.setForm(form);
    if (reenable_token)
      form['token'].disabled = false;
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

function _cs_fix_row_ids(row, preserve_values) {
    // Update the label of the checkbox.
    var label = YAHOO.util.Dom.getElementBy(function() { return true }, 'label', row);
    var id_match = label.htmlFor.match(/\d+$/);
    var id = parseInt(id_match[0]) + 1;
    label.htmlFor = label.htmlFor.replace(/\d+$/, id);

    // Sets all the inputs in the row back to their default
    // and fixes their id.
    var fields =
        YAHOO.util.Dom.getElementsByClassName('custom_search_form_field', null, row);
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

        // Update the numeric id for the row.
        field.name = field.name.replace(/\d+$/, id);
        field.id = field.name;
    }

    return id;
}

function _cs_build_structure(form_member) {
    // build a map of the structure of the custom fields
    var form = YAHOO.util.Dom.getAncestorByTagName(form_member, 'form');
    var last_id = _get_last_cs_row_id(form);
    var structure = [ 'j_top' ];
    var nested = [ structure ];
    for (var id = 1; id <= last_id; id++) {
        var f = form['f' + id];
        if (!f || !f.parentNode.parentNode) continue;

        if (f.value == 'OP') {
            var j = [ 'j' + id ];
            nested[nested.length - 1].push(j);
            nested.push(j);
            continue;
        } else if (f.value == 'CP') {
            nested.pop();
            continue;
        } else {
            nested[nested.length - 1].push('f' + id);
        }
    }
    return structure;
}

function cs_reconfigure(form_member) {
    var structure = _cs_build_structure(form_member);
    _cs_add_listeners(structure);
    _cs_trigger_j_listeners(structure);
    fix_query_string(form_member);

    var j = _cs_get_join(structure, 'f' + _get_last_cs_row_id());
    document.getElementById('op_button').disabled = document.getElementById(j).value == 'AND_G';
}

function _cs_add_listeners(parents) {
    for (var i = 0, l = parents.length; i < l; i++) {
        if (typeof(parents[i]) == 'object') {
            // nested
            _cs_add_listeners(parents[i]);
        } else if (i == 0) {
            // joiner
            YAHOO.util.Event.removeListener(parents[i], 'change', _cs_j_change);
            YAHOO.util.Event.addListener(parents[i], 'change', _cs_j_change, parents);
        } else {
            // field
            YAHOO.util.Event.removeListener(parents[i], 'change', _cs_f_change);
            YAHOO.util.Event.addListener(parents[i], 'change', _cs_f_change, parents);
        }
    }
}

function _cs_trigger_j_listeners(fields) {
    var has_children = false;
    for (var i = 0, l = fields.length; i < l; i++) {
        if (typeof(fields[i]) == 'undefined') {
            continue;
        } else if (typeof(fields[i]) == 'object') {
            // nested
            _cs_trigger_j_listeners(fields[i]);
            has_children = true;
        } else if (i == 0) {
            _cs_j_change(undefined, fields);
        }
    }
    if (has_children) {
        _cs_remove_and_g(document.getElementById(fields[0]));
    }
}

function _cs_get_join(parents, field) {
    for (var i = 0, l = parents.length; i < l; i++) {
        if (typeof(parents[i]) == 'object') {
            // nested
            var result = _cs_get_join(parents[i], field);
            if (result) return result;
        } else if (parents[i] == field) {
            return parents[0];
        }
    }
    return false;
}

function _cs_remove_and_g(join_field) {
    var index = bz_optionIndex(join_field, 'AND_G');
    join_field.options[index] = null;
    join_field.options[bz_optionIndex(join_field, 'AND')].innerHTML = cs_and_label;
    join_field.options[bz_optionIndex(join_field, 'OR')].innerHTML = cs_or_label;
}

function _cs_j_change(evt, fields, field) {
    var j = document.getElementById(fields[0]);
    if (j && j.value == 'AND_G') {
        for (var i = 1, l = fields.length; i < l; i++) {
            if (typeof(fields[i]) == 'object') continue;
            if (!field) {
                field = document.getElementById(fields[i]).value;
            } else {
                document.getElementById(fields[i]).value = field;
            }
        }
        if (evt) {
            fix_query_string(j);
        }
        if ('f' + _get_last_cs_row_id() == fields[fields.length - 1]) {
            document.getElementById('op_button').style.display = 'none';
        }
    } else {
        document.getElementById('op_button').style.display = '';
    }
}

function _cs_f_change(evt, args) {
    var field = YAHOO.util.Event.getTarget(evt);
    _cs_j_change(evt, args, field.value);
}

function _get_last_cs_row_id() {
    return _cs_get_row_id('custom_search_last_row');
}

function _cs_get_row_id(row) {
    var label = YAHOO.util.Dom.getElementBy(function() { return true }, 'label', row);
    return parseInt(label.htmlFor.match(/\d+$/)[0]);
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
