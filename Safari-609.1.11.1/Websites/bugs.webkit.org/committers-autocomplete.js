// Copyright (C) 2010 Ojan Vafai. All rights reserved.
// Copyright (C) 2010 Adam Barth. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

WebKitCommitters = (function() {
    var COMMITTERS_URL = 'https://svn.webkit.org/repository/webkit/trunk/Tools/Scripts/webkitpy/common/config/contributors.json';
    var m_committers;

    function statusToType(status) {
        if (status === 'reviewer')
            return 'r';
        if (status === 'committer')
            return 'c';
        return undefined;
    }

    function parseCommittersPy(text) {
        var contributors = JSON.parse(text);

        m_committers = [];

        for (var name in contributors) {
            var record = contributors[name];
            m_committers.push({
                name: name,
                emails: record.emails,
                irc: record.nicks,
                type: statusToType(record.status),
            });
        }
    }

    function loadCommitters(callback) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', COMMITTERS_URL);

        xhr.onload = function() {
            parseCommittersPy(xhr.responseText);
            callback();
        };

        xhr.onerror = function() {
            console.log('Unable to load contributors.json');
            callback();
        };

        xhr.send();
    }

    function getCommitters(callback) {
        if (m_committers) {
            callback(m_committers);
            return;
        }

        loadCommitters(function() {
            callback(m_committers);
        });
    }

    return {
        "getCommitters": getCommitters
    };
})();

(function() {
    var SINGLE_EMAIL_INPUTS = ['email1', 'email2', 'email3', 'requester', 'requestee', 'assigned_to'];
    var EMAIL_INPUTS = SINGLE_EMAIL_INPUTS.concat(['cc', 'newcc', 'new_watchedusers']);

    var m_menus = {};
    var m_focusedInput;
    var m_committers;
    var m_prefix;
    var m_selectedIndex;

    function contactsMatching(prefix) {
        var list = [];
        if (!prefix)
            return list;

        for (var i = 0; i < m_committers.length; i++) {
            if (isMatch(m_committers[i], prefix))
                list.push(m_committers[i]);
        }
        return list;
    }

    function startsWith(str, prefix) {
        return str.toLowerCase().indexOf(prefix.toLowerCase()) == 0;
    }

    function startsWithAny(arry, prefix) {
        for (var i = 0; i < arry.length; i++) {
            if (startsWith(arry[i], prefix))
                return true;
        }
        return false;
    }

    function isMatch(contact, prefix) {
        if (startsWithAny(contact.emails, prefix))
            return true;

        if (contact.irc && startsWithAny(contact.irc, prefix))
            return true;

        var names = contact.name.split(' ');
        for (var i = 0; i < names.length; i++) {
            if (startsWith(names[i], prefix))
                return true;
        }
        
        return false;
    }

    function isMenuVisible() {
        return getMenu().style.display != 'none';
    }

    function showMenu(shouldShow) {
        getMenu().style.display = shouldShow ? '' : 'none';
    }

    function updateMenu() {
        var newPrefix = m_focusedInput.value;
        if (newPrefix) {
            newPrefix = newPrefix.slice(getStart(), getEnd());
            newPrefix = newPrefix.replace(/^\s+/, '');
            newPrefix = newPrefix.replace(/\s+$/, '');
        }

        if (m_prefix == newPrefix)
            return;

        m_prefix = newPrefix;

        var contacts = contactsMatching(m_prefix);
        if (contacts.length == 0 || contacts.length == 1 && contacts[0].emails[0] == m_prefix) {
            showMenu(false);
            return;
        }

        var html = [];
        for (var i = 0; i < contacts.length; i++) {
            var contact = contacts[i];
            html.push('<div style="padding:1px 2px;" ' + 'email=' +
                contact.emails[0] + '>' + contact.name + ' - ' + contact.emails[0]);
            if (contact.irc)
                html.push(' (:' + contact.irc + ')');
            if (contact.type)
                html.push(' (' + contact.type + ')');
            html.push('</div>');
        }
        getMenu().innerHTML = html.join('');
        selectItem(0);
        showMenu(true);
    }

    function getIndex(item) {
        for (var i = 0; i < getMenu().childNodes.length; i++) {
            if (item == getMenu().childNodes[i])
                return i;
        }
        console.error("Couldn't find item.");
    }

    function getMenu() {
        return m_menus[m_focusedInput.name];
    }

    function createMenu(name, input) {
        if (!m_menus[name]) {
            var menu = document.createElement('div');
            menu.style.cssText =
                "position:absolute;border:1px solid black;background-color:white;-webkit-box-shadow:3px 3px 3px #888;";
            menu.style.minWidth = m_focusedInput.offsetWidth + 'px';
            m_focusedInput.parentNode.insertBefore(menu, m_focusedInput.nextSibling);

            menu.addEventListener('mousedown', function(e) {
                selectItem(getIndex(e.target));
                e.preventDefault();
            }, false);

            menu.addEventListener('mouseup', function(e) {
                if (m_selectedIndex == getIndex(e.target))
                    insertSelectedItem();
            }, false);
            
            m_menus[name] = menu;
        }
    }

    function getStart() {
        var index = m_focusedInput.value.lastIndexOf(',', m_focusedInput.selectionStart - 1);
        if (index == -1)
            return 0;
        return index + 1;
    }

    function getEnd() {
        var index = m_focusedInput.value.indexOf(',', m_focusedInput.selectionStart);
        if (index == -1)
            return m_focusedInput.value.length;
        return index;
    }

    function getItem(index) {
        return getMenu().childNodes[index];
    }

    function selectItem(index) {
        if (index < 0 || index >= getMenu().childNodes.length)
            return;

        if (m_selectedIndex != undefined) {
            getItem(m_selectedIndex).style.backgroundColor = '';
            getItem(m_selectedIndex).style.color = '';
        }

        getItem(index).style.backgroundColor = '#039';
        getItem(index).style.color = 'white';

        m_selectedIndex = index;
    }

    function insertSelectedItem() {
        var selectedEmail = getItem(m_selectedIndex).getAttribute('email');
        var oldValue = m_focusedInput.value;

        var newValue = oldValue.slice(0, getStart()) + selectedEmail + oldValue.slice(getEnd());
        if (SINGLE_EMAIL_INPUTS.indexOf(m_focusedInput.name) == -1 &&
            newValue.charAt(newValue.length - 1) != ',')
            newValue = newValue + ',';

        m_focusedInput.value = newValue;
        showMenu(false);    
    }

    function handleKeyDown(e) {
        if (!isMenuVisible())
            return;

        switch (e.keyIdentifier) {
            case 'Up':
                selectItem(m_selectedIndex - 1);
                e.preventDefault();
                break;
            
            case 'Down':
                selectItem(m_selectedIndex + 1);
                e.preventDefault();
                break;
                
            case 'Enter':
                insertSelectedItem();
                e.preventDefault();
                break;
        }
    }

    function handleKeyUp(e) {
        if (e.keyIdentifier == 'Enter')
            return;

        if (m_focusedInput.selectionStart == m_focusedInput.selectionEnd)
            updateMenu();
        else
            showMenu(false);
    }

    function enableAutoComplete(input) {
        m_focusedInput = input;

        if (!getMenu()) {
            createMenu(m_focusedInput.name);
            // Turn off autocomplete to avoid showing the browser's dropdown menu.
            m_focusedInput.setAttribute('autocomplete', 'off');
            m_focusedInput.addEventListener('keyup', handleKeyUp, false);
            m_focusedInput.addEventListener('keydown', handleKeyDown, false);
            m_focusedInput.addEventListener('blur', function() {
                showMenu(false);
                m_prefix = null;
                m_selectedIndex = 0;
            }, false);
            // Turn on autocomplete on submit to avoid breaking autofill on back/forward navigation.
            m_focusedInput.form.addEventListener("submit", function() {
                m_focusedInput.setAttribute("autocomplete", "on");
            }, false);
        }
        
        updateMenu();
    }

    for (var i = 0; i < EMAIL_INPUTS.length; i++) {
        var field = document.getElementsByName(EMAIL_INPUTS[i])[0];
        if (field)
            field.addEventListener("focus", function(e) { enableAutoComplete(e.target); }, false);
    }

    WebKitCommitters.getCommitters(function (committers) {
        m_committers = committers;
    });
})();
