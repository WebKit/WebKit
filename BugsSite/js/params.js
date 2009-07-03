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
 * The Initial Developer of the Original Code is Marc Schumann.
 * Portions created by Marc Schumann are Copyright (c) 2007 Marc Schumann.
 * All rights reserved.
 *
 * Contributor(s): Marc Schumann <wurblzap@gmail.com>
 */

function sortedList_moveItem(paramName, direction, separator) {
    var select = document.getElementById('select_' + paramName);
    var inputField = document.getElementById('input_' + paramName);
    var currentIndex = select.selectedIndex;
    var newIndex = currentIndex + direction;
    var optionCurrentIndex;
    var optionNewIndex;

    /* Return if no selection */
    if (currentIndex < 0) return;
    /* Return if trying to move upward out of list */
    if (newIndex < 0) return;
    /* Return if trying to move downward out of list */
    if (newIndex >= select.length) return;

    /* Move selection */
    optionNewIndex = select.options[newIndex];
    optionCurrentIndex = select.options[currentIndex];
    /* Because some browsers don't accept the same option object twice in a
     * selection list, we need to put a blank option here first */
    select.options[newIndex] = new Option();
    select.options[currentIndex] = optionNewIndex;
    select.options[newIndex] = optionCurrentIndex;
    select.selectedIndex = newIndex;
    populateInputField(select, inputField, separator);
}

function populateInputField(select, inputField, separator) {
    var i;
    var stringRepresentation = '';

    for (i = 0; i < select.length; i++) {
        if (select.options[i].value == separator) {
            break;
        }
        if (stringRepresentation != '') {
            stringRepresentation += ',';
        }
        stringRepresentation += select.options[i].value;
    }
    inputField.value = stringRepresentation;
}
