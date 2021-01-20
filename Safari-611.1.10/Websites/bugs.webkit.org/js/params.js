/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
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
