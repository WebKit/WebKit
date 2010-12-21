/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Bugzilla Bug Tracking System.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gervase Markham <gerv@gerv.net>
 *
 * ***** END LICENSE BLOCK ***** */

var g_helpTexts = new Object();
var g_helpIframe;
var g_helpDiv;

/**
 * Generate help controls during page construction.
 *
 * @return Boolean; true if controls were created and false if not.
 */
function generateHelp()
{
    // Only enable help if controls can be hidden
    if (!document.body.style)
        return false;

    // Create help controls (a div to hold help text and an iframe
    // to mask any and all controls under the popup)
    document.write('<div id="helpDiv" style="display: none;"><\/div>');
    document.write('<iframe id="helpIframe" src="about:blank"');
    document.write('        frameborder="0" scrolling="no"><\/iframe>');

    return true;
}

/**
 * Enable help popups for all form elements after the page has finished loading.
 *
 * @return Boolean; true if help was enabled and false if not.
 */
function enableHelp()
{
    g_helpIframe = document.getElementById('helpIframe');
    g_helpDiv = document.getElementById('helpDiv');
    if (!g_helpIframe || !g_helpDiv) // Disabled if no controls found
        return false;

    // MS decided to add fieldsets to the elements array; and
    // Mozilla decided to copy this brokenness. Grr.
    for (var i = 0; i < document.forms.length; i++) {
        for (var j = 0; j < document.forms[i].elements.length; j++) {
            if (document.forms[i].elements[j].tagName != 'FIELDSET') {
                document.forms[i].elements[j].onmouseover = showHelp;
            }
        }
    }

    document.body.onclick = hideHelp;
    return true;
}

/**
 * Show the help popup for a form element.
 */
function showHelp() {
    if (!g_helpIframe || !g_helpDiv || !g_helpTexts[this.name])
        return;

    // Get the position and size of the form element in the document
    var elemY = bz_findPosY(this);
    var elemX = bz_findPosX(this);
    var elemH = this.offsetHeight;

    // Update help text displayed in the div
    g_helpDiv.innerHTML = ''; // Helps IE 5 Mac
    g_helpDiv.innerHTML = g_helpTexts[this.name];

    // Position and display the help popup
    g_helpIframe.style.top = g_helpDiv.style.top = elemY + elemH + 5 + "px";
    g_helpIframe.style.left = g_helpDiv.style.left = elemX + "px";
    g_helpIframe.style.display = g_helpDiv.style.display = '';
    g_helpIframe.style.width = g_helpDiv.offsetWidth + "px";
    g_helpIframe.style.height = g_helpDiv.offsetHeight + "px";
}

/**
 * Hide the help popup.
 */
function hideHelp() {
    if (!g_helpIframe || !g_helpDiv)
        return;

    g_helpIframe.style.display = g_helpDiv.style.display = 'none';
}
