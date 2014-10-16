/*# The contents of this file are subject to the Mozilla Public
  # License Version 1.1 (the "License"); you may not use this file
  # except in compliance with the License. You may obtain a copy of
  # the License at http://www.mozilla.org/MPL/
  #
  # Software distributed under the License is distributed on an "AS
  # IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
  # implied. See the License for the specific language governing
  # rights and limitations under the License.
  #
  # The Original Code is the Bugzilla Bug Tracking System.
  #
  # The Initial Developer of the Original Code is Pascal Held.
  #
  # Contributor(s): Pascal Held <paheld@gmail.com>
  #
*/

function initChangeColumns() {
    window.onunload = unload;
    var av_select = document.getElementById("available_columns");
    var sel_select = document.getElementById("selected_columns");
    YAHOO.util.Dom.removeClass(
        ['avail_header', av_select, 'select_button', 
         'deselect_button', 'up_button', 'down_button'], 'bz_default_hidden');
    switch_options(sel_select, av_select, false);
    sel_select.selectedIndex = -1;
    updateView();
}

function switch_options(from_box, to_box, selected) {
    for (var i = 0; i<from_box.options.length; i++) {
        var opt = from_box.options[i];
        if (opt.selected == selected) {
            var newopt = new Option(opt.text, opt.value, opt.defaultselected, opt.selected);
            to_box.options[to_box.options.length] = newopt;
            from_box.options[i] = null;
            i = i - 1;
        }
        
    }
}

function move_select() {
    var av_select = document.getElementById("available_columns");
    var sel_select = document.getElementById("selected_columns");
    switch_options(av_select, sel_select, true);
    updateView();
}

function move_deselect() {
    var av_select = document.getElementById("available_columns");
    var sel_select = document.getElementById("selected_columns");
    switch_options(sel_select, av_select, true);
    updateView();
}

function move_up() {
    var sel_select = document.getElementById("selected_columns");
    var last = sel_select.options[0];
    var dummy = new Option("", "", false, false);
    for (var i = 1; i<sel_select.options.length; i++) {
        var opt = sel_select.options[i];
        if (opt.selected) {
            sel_select.options[i] = dummy;
            sel_select.options[i-1] = opt;
            sel_select.options[i] = last;
        }
        else{
            last = opt;
        }        
    }
    updateView();
}

function move_down() {
    var sel_select = document.getElementById("selected_columns");
    var last = sel_select.options[sel_select.options.length-1];
    var dummy = new Option("", "", false, false);
    for (var i = sel_select.options.length-2; i >= 0; i--) {
        var opt = sel_select.options[i];
        if (opt.selected) {
            sel_select.options[i] = dummy;
            sel_select.options[i + 1] = opt;
            sel_select.options[i] = last;
        }
        else{
            last = opt;
        }        
    }
    updateView();
}

function updateView() {
    var select_button = document.getElementById("select_button");
    var deselect_button = document.getElementById("deselect_button");
    var up_button = document.getElementById("up_button");
    var down_button = document.getElementById("down_button");
    select_button.disabled = true;
    deselect_button.disabled = true;
    up_button.disabled = true;
    down_button.disabled = true;
    var av_select = document.getElementById("available_columns");
    var sel_select = document.getElementById("selected_columns");
    for (var i = 0; i < av_select.options.length;  i++) {
        if (av_select.options[i].selected) {
            select_button.disabled = false;
            break;
        }
    }
    for (var i = 0; i < sel_select.options.length; i++) {
        if (sel_select.options[i].selected) {
            deselect_button.disabled = false;
            up_button.disabled = false;
            down_button.disabled = false;
            break;
        }
    }
    if (sel_select.options.length > 0) {
        if (sel_select.options[0].selected) {
            up_button.disabled = true;
        }
        if (sel_select.options[sel_select.options.length - 1].selected) {
            down_button.disabled = true;
        }
    }
}

function change_submit() {
    var sel_select = document.getElementById("selected_columns");
    for (var i = 0; i < sel_select.options.length; i++) {
        sel_select.options[i].selected = true;
    }
    return false;
}

function unload() {
    var sel_select = document.getElementById("selected_columns");
    for (var i = 0; i < sel_select.options.length; i++) {
        sel_select.options[i].selected = true;
    }
}
