/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 */

function validateAttachmentForm(theform) {
    var desc_value = YAHOO.lang.trim(theform.description.value);
    if (desc_value == '') {
        alert(BUGZILLA.string.attach_desc_required);
        return false;
    }
    return true;
}

function updateCommentPrivacy(checkbox) {
    var text_elem = document.getElementById('comment');
    if (checkbox.checked) {
        text_elem.className='bz_private';
    } else {
        text_elem.className='';
    }
}

function setContentTypeDisabledState(form) {
    var isdisabled = false;
    if (form.ispatch.checked)
        isdisabled = true;

    for (var i = 0; i < form.contenttypemethod.length; i++)
        form.contenttypemethod[i].disabled = isdisabled;

    form.contenttypeselection.disabled = isdisabled;
    form.contenttypeentry.disabled = isdisabled;

// if WEBKIT_CHANGES
    if (isdisabled) {
        document.getElementById('legal').style.display = "table-row";
        var createButton = document.getElementById('create');
        if (createButton && createButton.value == "Submit")
            createButton.value = "Agree and Submit";
        else {
            var commitButton = document.getElementById('commit');
            if (commitButton && commitButton.value == "Submit Bug")
                commitButton.value = "Agree and Submit Bug";
        }
    } else {
        var createButton = document.getElementById('create');
        if (createButton && createButton.value == "Agree and Submit")
            createButton.value = "Submit";
        else {
            var commitButton = document.getElementById('commit');
            if (commitButton && commitButton.value == "Agree and Submit Bug")
                commitButton.value = "Submit Bug";
        }
        document.getElementById('legal').style.display = "none";
    }
// endif WEBKIT_CHANGES
}

function TextFieldHandler() {
    var field_text = document.getElementById("attach_text");
    var greyfields = new Array("data", "autodetect", "list", "manual",
                               "contenttypeselection", "contenttypeentry");
    var i, thisfield;
    if (field_text.value.match(/^\s*$/)) {
        for (i = 0; i < greyfields.length; i++) {
            thisfield = document.getElementById(greyfields[i]);
            if (thisfield) {
                thisfield.removeAttribute("disabled");
            }
        }
    } else {
        for (i = 0; i < greyfields.length; i++) {
            thisfield = document.getElementById(greyfields[i]);
            if (thisfield) {
                thisfield.setAttribute("disabled", "disabled");
            }
        }
    }
}

function DataFieldHandler() {
    var field_data = document.getElementById("data");
    var greyfields = new Array("attach_text");
    var i, thisfield;
    if (field_data.value.match(/^\s*$/)) {
        for (i = 0; i < greyfields.length; i++) {
            thisfield = document.getElementById(greyfields[i]);
            if (thisfield) {
                thisfield.removeAttribute("disabled");
            }
        }
    } else {
        for (i = 0; i < greyfields.length; i++) {
            thisfield = document.getElementById(greyfields[i]);
            if (thisfield) {
                thisfield.setAttribute("disabled", "disabled");
            }
        }
    }
}

function clearAttachmentFields() {
    var element;

    document.getElementById('data').value = '';
    DataFieldHandler();
    if ((element = document.getElementById('attach_text'))) {
        element.value = '';
        TextFieldHandler();
    }
    document.getElementById('description').value = '';
    /* Fire onchange so that the disabled state of the content-type
     * radio buttons are also reset 
     */
    element = document.getElementById('ispatch');
    element.checked = '';
    bz_fireEvent(element, 'change');
    if ((element = document.getElementById('isprivate')))
        element.checked = '';
}

/* Functions used when viewing patches in Diff mode. */

function collapse_all() {
  var elem = document.checkboxform.firstChild;
  while (elem != null) {
    if (elem.firstChild != null) {
      var tbody = elem.firstChild.nextSibling;
      if (tbody.className == 'file') {
        tbody.className = 'file_collapse';
        twisty = get_twisty_from_tbody(tbody);
        twisty.firstChild.nodeValue = '(+)';
        twisty.nextSibling.checked = false;
      }
    }
    elem = elem.nextSibling;
  }
  return false;
}

function expand_all() {
  var elem = document.checkboxform.firstChild;
  while (elem != null) {
    if (elem.firstChild != null) {
      var tbody = elem.firstChild.nextSibling;
      if (tbody.className == 'file_collapse') {
        tbody.className = 'file';
        twisty = get_twisty_from_tbody(tbody);
        twisty.firstChild.nodeValue = '(-)';
        twisty.nextSibling.checked = true;
      }
    }
    elem = elem.nextSibling;
  }
  return false;
}

var current_restore_elem;

function restore_all() {
  current_restore_elem = null;
  incremental_restore();
}

function incremental_restore() {
  if (!document.checkboxform.restore_indicator.checked) {
    return;
  }
  var next_restore_elem;
  if (current_restore_elem) {
    next_restore_elem = current_restore_elem.nextSibling;
  } else {
    next_restore_elem = document.checkboxform.firstChild;
  }
  while (next_restore_elem != null) {
    current_restore_elem = next_restore_elem;
    if (current_restore_elem.firstChild != null) {
      restore_elem(current_restore_elem.firstChild.nextSibling);
    }
    next_restore_elem = current_restore_elem.nextSibling;
  }
}

function restore_elem(elem, alertme) {
  if (elem.className == 'file_collapse') {
    twisty = get_twisty_from_tbody(elem);
    if (twisty.nextSibling.checked) {
      elem.className = 'file';
      twisty.firstChild.nodeValue = '(-)';
    }
  } else if (elem.className == 'file') {
    twisty = get_twisty_from_tbody(elem);
    if (!twisty.nextSibling.checked) {
      elem.className = 'file_collapse';
      twisty.firstChild.nodeValue = '(+)';
    }
  }
}

function twisty_click(twisty) {
  tbody = get_tbody_from_twisty(twisty);
  if (tbody.className == 'file') {
    tbody.className = 'file_collapse';
    twisty.firstChild.nodeValue = '(+)';
    twisty.nextSibling.checked = false;
  } else {
    tbody.className = 'file';
    twisty.firstChild.nodeValue = '(-)';
    twisty.nextSibling.checked = true;
  }
  return false;
}

function get_tbody_from_twisty(twisty) {
  return twisty.parentNode.parentNode.parentNode.nextSibling;
}
function get_twisty_from_tbody(tbody) {
  return tbody.previousSibling.firstChild.nextSibling.firstChild.firstChild;
}

var prev_mode = 'raw';
var current_mode = 'raw';
var has_edited = 0;
var has_viewed_as_diff = 0;
// if WEBKIT_CHANGES
var viewing_formatted_diff = false;
// endif WEBKIT_CHANGES
function editAsComment(patchviewerinstalled)
{
    switchToMode('edit', patchviewerinstalled);
    has_edited = 1;
}
function undoEditAsComment(patchviewerinstalled)
{
    switchToMode(prev_mode, patchviewerinstalled);
}
function redoEditAsComment(patchviewerinstalled)
{
    switchToMode('edit', patchviewerinstalled);
}

// if WEBKIT_CHANGES
function viewPrettyPatch(attachment_id)
{
    viewing_formatted_diff = !viewing_formatted_diff;
    var src = "attachment.cgi?id=" + attachment_id;
    var buttonText = "View Formatted Diff";
    if (viewing_formatted_diff) {
      src += "&action=prettypatch"
      buttonText = "View Plain Diff";
    }

    document.getElementById('viewFrame').src = src;
    document.getElementById('viewPrettyPatchButton').innerHTML = buttonText;
}
// endif WEBKIT_CHANGES

function viewDiff(attachment_id, patchviewerinstalled)
{
    switchToMode('diff', patchviewerinstalled);

    // If we have not viewed as diff before, set the view diff frame URL
    if (!has_viewed_as_diff) {
      var viewDiffFrame = document.getElementById('viewDiffFrame');
      viewDiffFrame.src =
          'attachment.cgi?id=' + attachment_id + '&action=diff&headers=0';
      has_viewed_as_diff = 1;
    }
}

function viewRaw(patchviewerinstalled)
{
    switchToMode('raw', patchviewerinstalled);
}

function switchToMode(mode, patchviewerinstalled)
{
    // Switch out of current mode
    if (current_mode == 'edit') {
      hideElementById('editFrame');
      hideElementById('undoEditButton');
    } else if (current_mode == 'raw') {
      hideElementById('viewFrame');
// if WEBKIT_CHANGES
      hideElementById('viewPrettyPatchButton');
// endif WEBKIT_CHANGES
      if (patchviewerinstalled)
          hideElementById('viewDiffButton');
      hideElementById(has_edited ? 'redoEditButton' : 'editButton');
      hideElementById('smallCommentFrame');
    } else if (current_mode == 'diff') {
      if (patchviewerinstalled)
          hideElementById('viewDiffFrame');
      hideElementById('viewRawButton');
      hideElementById(has_edited ? 'redoEditButton' : 'editButton');
      hideElementById('smallCommentFrame');
    }

    // Switch into new mode
    if (mode == 'edit') {
      showElementById('editFrame');
      showElementById('undoEditButton');
    } else if (mode == 'raw') {
      showElementById('viewFrame');
// if WEBKIT_CHANGES
      showElementById('viewPrettyPatchButton');
// endif WEBKIT_CHANGES
      if (patchviewerinstalled) 
          showElementById('viewDiffButton');

      showElementById(has_edited ? 'redoEditButton' : 'editButton');
      showElementById('smallCommentFrame');
    } else if (mode == 'diff') {
      if (patchviewerinstalled) 
        showElementById('viewDiffFrame');

      showElementById('viewRawButton');
      showElementById(has_edited ? 'redoEditButton' : 'editButton');
      showElementById('smallCommentFrame');
    }

    prev_mode = current_mode;
    current_mode = mode;
}

function hideElementById(id)
{
  var elm = document.getElementById(id);
  if (elm) {
    YAHOO.util.Dom.addClass(elm, 'bz_default_hidden');
  }
}

function showElementById(id)
{
  var elm = document.getElementById(id);
  if (elm) {
    YAHOO.util.Dom.removeClass(elm, 'bz_default_hidden');
  }
}

function normalizeComments()
{
  // Remove the unused comment field from the document so its contents
  // do not get transmitted back to the server.

  var small = document.getElementById('smallCommentFrame');
  var big = document.getElementById('editFrame');
  if ( (small) && YAHOO.util.Dom.hasClass(small, 'bz_default_hidden') )
  {
    small.parentNode.removeChild(small);
  }
  if ( (big) && YAHOO.util.Dom.hasClass(big, 'bz_default_hidden') )
  {
    big.parentNode.removeChild(big);
  }
}

function toggle_attachment_details_visibility ( ) 
{
    // show hide classes
    var container = document.getElementById('attachment_info');
    if( YAHOO.util.Dom.hasClass(container, 'read') ){
        YAHOO.util.Dom.replaceClass(container, 'read', 'edit');
    }else{
        YAHOO.util.Dom.replaceClass(container, 'edit', 'read');
    }
}

/* Used in bug/create.html.tmpl to show/hide the attachment field. */

function handleWantsAttachment(wants_attachment) {
    if (wants_attachment) {
        hideElementById('attachment_false');
        showElementById('attachment_true');
    }
    else {
        showElementById('attachment_false');
        hideElementById('attachment_true');
        clearAttachmentFields();
    }
}
