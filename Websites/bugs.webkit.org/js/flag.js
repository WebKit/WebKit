/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 */

// Shows or hides a requestee field depending on whether or not
// the user is requesting the corresponding flag.
function toggleRequesteeField(flagField, no_focus)
{
  // Convert the ID of the flag field into the ID of its corresponding
  // requestee field and then use the ID to get the field.
  var id = flagField.name.replace(/flag(_type)?-(\d+)/, "requestee$1-$2");
  var requesteeField = document.getElementById(id);
  if (!requesteeField) return;

  // Show or hide the requestee field based on the value
  // of the flag field.
  if (flagField.value == "?") {
      YAHOO.util.Dom.removeClass(requesteeField.parentNode, 'bz_default_hidden');
      if (!no_focus) requesteeField.focus();
  } else
      YAHOO.util.Dom.addClass(requesteeField.parentNode, 'bz_default_hidden');
}

// Hides requestee fields when the window is loaded since they shouldn't
// be enabled until the user requests that flag type.
function hideRequesteeFields()
{
  var inputElements = document.getElementsByTagName("input");
  var selectElements = document.getElementsByTagName("select");
  //You cannot update Node lists, so you must create an array to combine the NodeLists
  var allElements = [];
  for( var i=0; i < inputElements.length; i++ ) {
      allElements[allElements.length] = inputElements.item(i);
  }
  for( var i=0; i < selectElements.length; i++ ) { //Combine inputs with selects
      allElements[allElements.length] = selectElements.item(i);
  }
  var inputElement, id, flagField;
  for ( var i=0 ; i<allElements.length ; i++ )
  {
    inputElement = allElements[i];
    if (inputElement.name.search(/^requestee(_type)?-(\d+)$/) != -1)
    {
      // Convert the ID of the requestee field into the ID of its corresponding
      // flag field and then use the ID to get the field.
      id = inputElement.name.replace(/requestee(_type)?-(\d+)/, "flag$1-$2");
      flagField = document.getElementById(id);
      if (flagField && flagField.value != "?")
          YAHOO.util.Dom.addClass(inputElement.parentNode, 'bz_default_hidden');
    }
  }
}
YAHOO.util.Event.onDOMReady(hideRequesteeFields);
