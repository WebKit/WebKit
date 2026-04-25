/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 */

function show_mini_login_form( suffix ) {
    var login_link = document.getElementById('login_link' + suffix);
    var login_form = document.getElementById('mini_login' + suffix);
    var account_container = document.getElementById('new_account_container'
                                                    + suffix);

    YAHOO.util.Dom.addClass(login_link, 'bz_default_hidden');
    YAHOO.util.Dom.removeClass(login_form, 'bz_default_hidden');
    YAHOO.util.Dom.addClass(account_container, 'bz_default_hidden');
    return false;
}

function hide_mini_login_form( suffix ) {
    var login_link = document.getElementById('login_link' + suffix);
    var login_form = document.getElementById('mini_login' + suffix);
    var account_container = document.getElementById('new_account_container'
                                                    + suffix);

    YAHOO.util.Dom.removeClass(login_link, 'bz_default_hidden');
    YAHOO.util.Dom.addClass(login_form, 'bz_default_hidden');
    YAHOO.util.Dom.removeClass(account_container, 'bz_default_hidden');
    return false;
}

function show_forgot_form( suffix ) {
    var forgot_link = document.getElementById('forgot_link' + suffix);
    var forgot_form = document.getElementById('forgot_form' + suffix);
    var login_container = document.getElementById('mini_login_container' 
                                                  + suffix);
    YAHOO.util.Dom.addClass(forgot_link, 'bz_default_hidden');
    YAHOO.util.Dom.removeClass(forgot_form, 'bz_default_hidden');
    YAHOO.util.Dom.addClass(login_container, 'bz_default_hidden');
    return false;
}

function hide_forgot_form( suffix ) {
    var forgot_link = document.getElementById('forgot_link' + suffix);
    var forgot_form = document.getElementById('forgot_form' + suffix);
    var login_container = document.getElementById('mini_login_container'
                                                  + suffix);
    YAHOO.util.Dom.removeClass(forgot_link, 'bz_default_hidden');
    YAHOO.util.Dom.addClass(forgot_form, 'bz_default_hidden');
    YAHOO.util.Dom.removeClass(login_container, 'bz_default_hidden');
    return false;
}

function set_language( value ) {
    YAHOO.util.Cookie.set('LANG', value,
    {
        expires: new Date('January 1, 2038'),
        path: BUGZILLA.param.cookie_path
    });
    window.location.reload()
}

// This basically duplicates Bugzilla::Util::display_value for code that
// can't go through the template and has to be in JS.
function display_value(field, value) {
    var field_trans = BUGZILLA.value_descs[field];
    if (!field_trans) return value;
    var translated = field_trans[value];
    if (translated) return translated;
    return value;
}
