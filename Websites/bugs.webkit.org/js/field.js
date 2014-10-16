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
 * The Initial Developer of the Original Code is Everything Solved, Inc.
 * Portions created by Everything Solved are Copyright (C) 2007 Everything
 * Solved, Inc. All Rights Reserved.
 *
 * Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
 *                 Guy Pyrzak <guy.pyrzak@gmail.com>
 *                 Reed Loden <reed@reedloden.com>
 */

/* This library assumes that the needed YUI libraries have been loaded 
   already. */

var bz_no_validate_enter_bug = false;
function validateEnterBug(theform) {
    // This is for the "bookmarkable templates" button.
    if (bz_no_validate_enter_bug) {
        // Set it back to false for people who hit the "back" button
        bz_no_validate_enter_bug = false;
        return true;
    }

    var component = theform.component;
    var short_desc = theform.short_desc;
    var version = theform.version;
    var bug_status = theform.bug_status;
    var description = theform.comment;
    var attach_data = theform.data;
    var attach_desc = theform.description;

    var current_errors = YAHOO.util.Dom.getElementsByClassName(
        'validation_error_text', null, theform);
    for (var i = 0; i < current_errors.length; i++) {
        current_errors[i].parentNode.removeChild(current_errors[i]);
    }
    var current_error_fields = YAHOO.util.Dom.getElementsByClassName(
        'validation_error_field', null, theform);
    for (var i = 0; i < current_error_fields.length; i++) {
        var field = current_error_fields[i];
        YAHOO.util.Dom.removeClass(field, 'validation_error_field');
    }

    var focus_me;

    // These are checked in the reverse order that they appear on the page,
    // so that the one closest to the top of the form will be focused.
    if (attach_data.value && YAHOO.lang.trim(attach_desc.value) == '') {
        _errorFor(attach_desc, 'attach_desc');
        focus_me = attach_desc;
    }
    var check_description = status_comment_required[bug_status.value];
    if (check_description && YAHOO.lang.trim(description.value) == '') {
        _errorFor(description, 'description');
        focus_me = description;
    }
    if (YAHOO.lang.trim(short_desc.value) == '') {
        _errorFor(short_desc);
        focus_me = short_desc;
    }
    if (version.selectedIndex < 0) {
        _errorFor(version);
        focus_me = version;
    }
    if (component.selectedIndex < 0) {
        _errorFor(component);
        focus_me = component;
    }

    if (focus_me) {
        focus_me.focus();
        return false;
    }

    return true;
}

function _errorFor(field, name) {
    if (!name) name = field.id;
    var string_name = name + '_required';
    var error_text = BUGZILLA.string[string_name];
    var new_node = document.createElement('div');
    YAHOO.util.Dom.addClass(new_node, 'validation_error_text');
    new_node.innerHTML = error_text;
    YAHOO.util.Dom.insertAfter(new_node, field);
    YAHOO.util.Dom.addClass(field, 'validation_error_field');
}

function createCalendar(name) {
    var cal = new YAHOO.widget.Calendar('calendar_' + name, 
                                        'con_calendar_' + name);
    YAHOO.bugzilla['calendar_' + name] = cal;
    var field = document.getElementById(name);
    cal.selectEvent.subscribe(setFieldFromCalendar, field, false);
    updateCalendarFromField(field);
    cal.render();
}

/* The onclick handlers for the button that shows the calendar. */
function showCalendar(field_name) {
    var calendar  = YAHOO.bugzilla["calendar_" + field_name];
    var field     = document.getElementById(field_name);
    var button    = document.getElementById('button_calendar_' + field_name);

    bz_overlayBelow(calendar.oDomContainer, field);
    calendar.show();
    button.onclick = function() { hideCalendar(field_name); };

    // Because of the way removeListener works, this has to be a function
    // attached directly to this calendar.
    calendar.bz_myBodyCloser = function(event) {
        var container = this.oDomContainer;
        var target    = YAHOO.util.Event.getTarget(event);
        if (target != container && target != button
            && !YAHOO.util.Dom.isAncestor(container, target))
        {
            hideCalendar(field_name);
        }
    };

    // If somebody clicks outside the calendar, hide it.
    YAHOO.util.Event.addListener(document.body, 'click', 
                                 calendar.bz_myBodyCloser, calendar, true);

    // Make Esc close the calendar.
    calendar.bz_escCal = function (event) {
        var key = YAHOO.util.Event.getCharCode(event);
        if (key == 27) {
            hideCalendar(field_name);
        }
    };
    YAHOO.util.Event.addListener(document.body, 'keydown', calendar.bz_escCal);
}

function hideCalendar(field_name) {
    var cal = YAHOO.bugzilla["calendar_" + field_name];
    cal.hide();
    var button = document.getElementById('button_calendar_' + field_name);
    button.onclick = function() { showCalendar(field_name); };
    YAHOO.util.Event.removeListener(document.body, 'click',
                                    cal.bz_myBodyCloser);
    YAHOO.util.Event.removeListener(document.body, 'keydown', cal.bz_escCal);
}

/* This is the selectEvent for our Calendar objects on our custom 
 * DateTime fields.
 */
function setFieldFromCalendar(type, args, date_field) {
    var dates = args[0];
    var setDate = dates[0];

    // We can't just write the date straight into the field, because there 
    // might already be a time there.
    var timeRe = /\b(\d{1,2}):(\d\d)(?::(\d\d))?/;
    var currentTime = timeRe.exec(date_field.value);
    var d = new Date(setDate[0], setDate[1] - 1, setDate[2]);
    if (currentTime) {
        d.setHours(currentTime[1], currentTime[2]);
        if (currentTime[3]) {
            d.setSeconds(currentTime[3]);
        }
    }

    var year = d.getFullYear();
    // JavaScript's "Date" represents January as 0 and December as 11.
    var month = d.getMonth() + 1;
    if (month < 10) month = '0' + String(month);
    var day = d.getDate();
    if (day < 10) day = '0' + String(day);
    var dateStr = year + '-' + month  + '-' + day;

    if (currentTime) {
        var minutes = d.getMinutes();
        if (minutes < 10) minutes = '0' + String(minutes);
        var seconds = d.getSeconds();
        if (seconds > 0 && seconds < 10) {
            seconds = '0' + String(seconds);
        }

        dateStr = dateStr + ' ' + d.getHours() + ':' + minutes;
        if (seconds) dateStr = dateStr + ':' + seconds;
    }

    date_field.value = dateStr;
    hideCalendar(date_field.id);
}

/* Sets the calendar based on the current field value. 
 */ 
function updateCalendarFromField(date_field) {
    var dateRe = /(\d\d\d\d)-(\d\d?)-(\d\d?)/;
    var pieces = dateRe.exec(date_field.value);
    if (pieces) {
        var cal = YAHOO.bugzilla["calendar_" + date_field.id];
        cal.select(new Date(pieces[1], pieces[2] - 1, pieces[3]));
        var selectedArray = cal.getSelectedDates();
        var selected = selectedArray[0];
        cal.cfg.setProperty("pagedate", (selected.getMonth() + 1) + '/' 
                                        + selected.getFullYear());
        cal.render();
    }
}

function setupEditLink(id) {
    var link_container = 'container_showhide_' + id;
    var input_container = 'container_' + id;
    var link = 'showhide_' + id;
    hideEditableField(link_container, input_container, link);
}

/* Hide input/select fields and show the text with (edit) next to it */
function hideEditableField( container, input, action, field_id, original_value, new_value ) {
    YAHOO.util.Dom.removeClass(container, 'bz_default_hidden');
    YAHOO.util.Dom.addClass(input, 'bz_default_hidden');
    YAHOO.util.Event.addListener(action, 'click', showEditableField,
                                 new Array(container, input, field_id, new_value));
    if(field_id != ""){
        YAHOO.util.Event.addListener(window, 'load', checkForChangedFieldValues,
                        new Array(container, input, field_id, original_value));
    }
}

/* showEditableField (e, ContainerInputArray)
 * Function hides the (edit) link and the text and displays the input/select field
 *
 * var e: the event
 * var ContainerInputArray: An array containing the (edit) and text area and the input being displayed
 * var ContainerInputArray[0]: the container that will be hidden usually shows the (edit) or (take) text
 * var ContainerInputArray[1]: the input area and label that will be displayed
 * var ContainerInputArray[2]: the input/select field id for which the new value must be set
 * var ContainerInputArray[3]: the new value to set the input/select field to when (take) is clicked
 */
function showEditableField (e, ContainerInputArray) {
    var inputs = new Array();
    var inputArea = YAHOO.util.Dom.get(ContainerInputArray[1]);    
    if ( ! inputArea ){
        YAHOO.util.Event.preventDefault(e);
        return;
    }
    YAHOO.util.Dom.addClass(ContainerInputArray[0], 'bz_default_hidden');
    YAHOO.util.Dom.removeClass(inputArea, 'bz_default_hidden');
    if ( inputArea.tagName.toLowerCase() == "input" ) {
        inputs.push(inputArea);
    } else if (ContainerInputArray[2]) {
        inputs.push(document.getElementById(ContainerInputArray[2]));
    } else {
        inputs = inputArea.getElementsByTagName('input');
    }
    if ( inputs.length > 0 ) {
        // Change the first field's value to ContainerInputArray[2]
        // if present before focusing.
        var type = inputs[0].tagName.toLowerCase();
        if (ContainerInputArray[3]) {
            if ( type == "input" ) {
                inputs[0].value = ContainerInputArray[3];
            } else {
                for (var i = 0; inputs[0].length; i++) {
                    if ( inputs[0].options[i].value == ContainerInputArray[3] ) {
                        inputs[0].options[i].selected = true;
                        break;
                    }
                }
            }
        }
        // focus on the first field, this makes it easier to edit
        inputs[0].focus();
        if ( type == "input" ) {
            inputs[0].select();
        }
    }
    YAHOO.util.Event.preventDefault(e);
}


/* checkForChangedFieldValues(e, array )
 * Function checks if after the autocomplete by the browser if the values match the originals.
 *   If they don't match then hide the text and show the input so users don't get confused.
 *
 * var e: the event
 * var ContainerInputArray: An array containing the (edit) and text area and the input being displayed
 * var ContainerInputArray[0]: the conainer that will be hidden usually shows the (edit) text
 * var ContainerInputArray[1]: the input area and label that will be displayed
 * var ContainerInputArray[2]: the field that is on the page, might get changed by browser autocomplete 
 * var ContainerInputArray[3]: the original value from the page loading.
 *
 */  
function checkForChangedFieldValues(e, ContainerInputArray ) {
    var el = document.getElementById(ContainerInputArray[2]);
    var unhide = false;
    if ( el ) {
        if ( el.value != ContainerInputArray[3] ||
            ( el.value == "" && el.id != "alias") ) {
            unhide = true;
        }
        else {
            var set_default = document.getElementById("set_default_" +
                                                      ContainerInputArray[2]);
            if ( set_default ) {
                if(set_default.checked){
                    unhide = true;
                }              
            }
        }
    }
    if(unhide){
        YAHOO.util.Dom.addClass(ContainerInputArray[0], 'bz_default_hidden');
        YAHOO.util.Dom.removeClass(ContainerInputArray[1], 'bz_default_hidden');
    }

}

function hideAliasAndSummary(short_desc_value, alias_value) {
    // check the short desc field
    hideEditableField( 'summary_alias_container','summary_alias_input',
                       'editme_action','short_desc', short_desc_value);  
    // check that the alias hasn't changed
    var bz_alias_check_array = new Array('summary_alias_container',
                                     'summary_alias_input', 'alias', alias_value);
    YAHOO.util.Event.addListener( window, 'load', checkForChangedFieldValues,
                                 bz_alias_check_array);
}

function showPeopleOnChange( field_id_list ) {
    for(var i = 0; i < field_id_list.length; i++) {
        YAHOO.util.Event.addListener( field_id_list[i],'change', showEditableField,
                                      new Array('bz_qa_contact_edit_container',
                                                'bz_qa_contact_input'));
        YAHOO.util.Event.addListener( field_id_list[i],'change',showEditableField,
                                      new Array('bz_assignee_edit_container',
                                                'bz_assignee_input'));
    }
}

function assignToDefaultOnChange(field_id_list) {
    showPeopleOnChange( field_id_list );
    for(var i = 0; i < field_id_list.length; i++) {
        YAHOO.util.Event.addListener( field_id_list[i],'change', setDefaultCheckbox,
                                      'set_default_assignee');
        YAHOO.util.Event.addListener( field_id_list[i],'change',setDefaultCheckbox,
                                      'set_default_qa_contact');    
    }
}

function initDefaultCheckbox(field_id){
    YAHOO.util.Event.addListener( 'set_default_' + field_id,'change', boldOnChange,
                                  'set_default_' + field_id);
    YAHOO.util.Event.addListener( window,'load', checkForChangedFieldValues,
                                  new Array( 'bz_' + field_id + '_edit_container',
                                             'bz_' + field_id + '_input',
                                             'set_default_' + field_id ,'1'));
    
    YAHOO.util.Event.addListener( window, 'load', boldOnChange,
                                 'set_default_' + field_id ); 
}

function showHideStatusItems(e, dupArrayInfo) {
    var el = document.getElementById('bug_status');
    // finish doing stuff based on the selection.
    if ( el ) {
        showDuplicateItem(el);

        // Make sure that fields whose visibility or values are controlled
        // by "resolution" behave properly when resolution is hidden.
        var resolution = document.getElementById('resolution');
        if (resolution && resolution.options[0].value != '') {
            resolution.bz_lastSelected = resolution.selectedIndex;
            var emptyOption = new Option('', '');
            resolution.insertBefore(emptyOption, resolution.options[0]);
            emptyOption.selected = true;
        }
        YAHOO.util.Dom.addClass('resolution_settings', 'bz_default_hidden');
        if (document.getElementById('resolution_settings_warning')) {
            YAHOO.util.Dom.addClass('resolution_settings_warning',
                                    'bz_default_hidden');
        }
        YAHOO.util.Dom.addClass('duplicate_display', 'bz_default_hidden');


        if ( (el.value == dupArrayInfo[1] && dupArrayInfo[0] == "is_duplicate")
             || bz_isValueInArray(close_status_array, el.value) ) 
        {
            YAHOO.util.Dom.removeClass('resolution_settings', 
                                       'bz_default_hidden');
            YAHOO.util.Dom.removeClass('resolution_settings_warning', 
                                       'bz_default_hidden');

            // Remove the blank option we inserted.
            if (resolution && resolution.options[0].value == '') {
                resolution.removeChild(resolution.options[0]);
                resolution.selectedIndex = resolution.bz_lastSelected;
            }
        }

        if (resolution) {
            bz_fireEvent(resolution, 'change');
        }
    }
}

function showDuplicateItem(e) {
    var resolution = document.getElementById('resolution');
    var bug_status = document.getElementById('bug_status');
    var dup_id = document.getElementById('dup_id');
    if (resolution) {
        if (resolution.value == 'DUPLICATE' && bz_isValueInArray( close_status_array, bug_status.value) ) {
            // hide resolution show duplicate
            YAHOO.util.Dom.removeClass('duplicate_settings', 
                                       'bz_default_hidden');
            YAHOO.util.Dom.addClass('dup_id_discoverable', 'bz_default_hidden');
            // check to make sure the field is visible or IE throws errors
            if( ! YAHOO.util.Dom.hasClass( dup_id, 'bz_default_hidden' ) ){
                dup_id.focus();
                dup_id.select();
            }
        }
        else {
            YAHOO.util.Dom.addClass('duplicate_settings', 'bz_default_hidden');
            YAHOO.util.Dom.removeClass('dup_id_discoverable', 
                                       'bz_default_hidden');
            dup_id.blur();
        }
    }
    YAHOO.util.Event.preventDefault(e); //prevents the hyperlink from going to the url in the href.
}

function setResolutionToDuplicate(e, duplicate_or_move_bug_status) {
    var status = document.getElementById('bug_status');
    var resolution = document.getElementById('resolution');
    YAHOO.util.Dom.addClass('dup_id_discoverable', 'bz_default_hidden');
    status.value = duplicate_or_move_bug_status;
    bz_fireEvent(status, 'change');
    resolution.value = "DUPLICATE";
    bz_fireEvent(resolution, 'change');
    YAHOO.util.Event.preventDefault(e);
}

function setDefaultCheckbox(e, field_id ) { 
    var el = document.getElementById(field_id);
    var elLabel = document.getElementById(field_id + "_label");
    if( el && elLabel ) {
        el.checked = "true";
        YAHOO.util.Dom.setStyle(elLabel, 'font-weight', 'bold');
    }
}

function boldOnChange(e, field_id){
    var el = document.getElementById(field_id);
    var elLabel = document.getElementById(field_id + "_label");
    if( el && elLabel ) {
        if( el.checked ){
            YAHOO.util.Dom.setStyle(elLabel, 'font-weight', 'bold');
        }
        else{
            YAHOO.util.Dom.setStyle(elLabel, 'font-weight', 'normal');
        }
    }
}

function updateCommentTagControl(checkbox, field) {
    if (checkbox.checked) {
        YAHOO.util.Dom.addClass(field, 'bz_private');
    } else {
        YAHOO.util.Dom.removeClass(field, 'bz_private');
    }
}

/**
 * Reset the value of the classification field and fire an event change
 * on it.  Called when the product changes, in case the classification
 * field (which is hidden) controls the visibility of any other fields.
 */
function setClassification() {
    var classification = document.getElementById('classification');
    var product = document.getElementById('product');
    var selected_product = product.value; 
    var select_classification = all_classifications[selected_product];
    classification.value = select_classification;
    bz_fireEvent(classification, 'change');
}

/**
 * Says that a field should only be displayed when another field has
 * a certain value. May only be called after the controller has already
 * been added to the DOM.
 */
function showFieldWhen(controlled_id, controller_id, values) {
    var controller = document.getElementById(controller_id);
    // Note that we don't get an object for "controlled" here, because it
    // might not yet exist in the DOM. We just pass along its id.
    YAHOO.util.Event.addListener(controller, 'change',
        handleVisControllerValueChange, [controlled_id, controller, values]);
}

/**
 * Called by showFieldWhen when a field's visibility controller 
 * changes values. 
 */
function handleVisControllerValueChange(e, args) {
    var controlled_id = args[0];
    var controller = args[1];
    var values = args[2];

    var label_container = 
        document.getElementById('field_label_' + controlled_id);
    var field_container =
        document.getElementById('field_container_' + controlled_id);
    var selected = false;
    for (var i = 0; i < values.length; i++) {
        if (bz_valueSelected(controller, values[i])) {
            selected = true;
            break;
        }
    }

    if (selected) {
        YAHOO.util.Dom.removeClass(label_container, 'bz_hidden_field');
        YAHOO.util.Dom.removeClass(field_container, 'bz_hidden_field');
    }
    else {
        YAHOO.util.Dom.addClass(label_container, 'bz_hidden_field');
        YAHOO.util.Dom.addClass(field_container, 'bz_hidden_field');
    }
}

function showValueWhen(controlled_field_id, controlled_value_ids, 
                       controller_field_id, controller_value_id)
{
    var controller_field = document.getElementById(controller_field_id);
    // Note that we don't get an object for the controlled field here, 
    // because it might not yet exist in the DOM. We just pass along its id.
    YAHOO.util.Event.addListener(controller_field, 'change',
        handleValControllerChange, [controlled_field_id, controlled_value_ids,
                                    controller_field, controller_value_id]);
}

function handleValControllerChange(e, args) {
    var controlled_field = document.getElementById(args[0]);
    var controlled_value_ids = args[1];
    var controller_field = args[2];
    var controller_value_id = args[3];

    var controller_item = document.getElementById(
        _value_id(controller_field.id, controller_value_id));

    for (var i = 0; i < controlled_value_ids.length; i++) {
        var item = getPossiblyHiddenOption(controlled_field,
                                           controlled_value_ids[i]);
        if (item.disabled && controller_item && controller_item.selected) {
            item = showOptionInIE(item, controlled_field);
            YAHOO.util.Dom.removeClass(item, 'bz_hidden_option');
            item.disabled = false;
        }
        else if (!item.disabled && controller_item && !controller_item.selected) {
            YAHOO.util.Dom.addClass(item, 'bz_hidden_option');
            if (item.selected) {
                item.selected = false;
                bz_fireEvent(controlled_field, 'change');
            }
            item.disabled = true;
            hideOptionInIE(item, controlled_field);
        }
    }
}

// A convenience function to generate the "id" tag of an <option>
// based on the numeric id that Bugzilla uses for that value.
function _value_id(field_name, id) {
    return 'v' + id + '_' + field_name;
}

/*********************************/
/* Code for Hiding Options in IE */
/*********************************/

/* IE 7 and below (and some other browsers) don't respond to "display: none"
 * on <option> tags. However, you *can* insert a Comment Node as a
 * child of a <select> tag. So we just insert a Comment where the <option>
 * used to be. */
var ie_hidden_options = new Array();
function hideOptionInIE(anOption, aSelect) {
    if (browserCanHideOptions(aSelect)) return;

    var commentNode = document.createComment(anOption.value);
    commentNode.id = anOption.id;
    // This keeps the interface of Comments and Options the same for
    // our other functions.
    commentNode.disabled = true;
    // replaceChild is very slow on IE in a <select> that has a lot of
    // options, so we use replaceNode when we can.
    if (anOption.replaceNode) {
        anOption.replaceNode(commentNode);
    }
    else {
        aSelect.replaceChild(commentNode, anOption);
    }

    // Store the comment node for quick access for getPossiblyHiddenOption
    if (!ie_hidden_options[aSelect.id]) {
        ie_hidden_options[aSelect.id] = new Array();
    }
    ie_hidden_options[aSelect.id][anOption.id] = commentNode;
}

function showOptionInIE(aNode, aSelect) {
    if (browserCanHideOptions(aSelect)) return aNode;

    // We do this crazy thing with innerHTML and createElement because
    // this is the ONLY WAY that this works properly in IE.
    var optionNode = document.createElement('option');
    optionNode.innerHTML = aNode.data;
    optionNode.value = aNode.data;
    optionNode.id = aNode.id;
    // replaceChild is very slow on IE in a <select> that has a lot of
    // options, so we use replaceNode when we can.
    if (aNode.replaceNode) {
        aNode.replaceNode(optionNode);
    }
    else {
        aSelect.replaceChild(optionNode, aNode);
    }
    delete ie_hidden_options[aSelect.id][optionNode.id];
    return optionNode;
}

function initHidingOptionsForIE(select_name) {
    var aSelect = document.getElementById(select_name);
    if (browserCanHideOptions(aSelect)) return;

    for (var i = 0; ;i++) {
        var item = aSelect.options[i];
        if (!item) break;
        if (item.disabled) {
          hideOptionInIE(item, aSelect);
          i--; // Hiding an option means that the options array has changed.
        }
    }
}

function getPossiblyHiddenOption(aSelect, optionId) {
    // Works always for <option> tags, and works for commentNodes
    // in IE (but not in Webkit).
    var id = _value_id(aSelect.id, optionId);
    var val = document.getElementById(id);

    // This is for WebKit and other browsers that can't "display: none"
    // an <option> and also can't getElementById for a commentNode.
    if (!val && ie_hidden_options[aSelect.id]) {
        val = ie_hidden_options[aSelect.id][id];
    }

    return val;
}

var browser_can_hide_options;
function browserCanHideOptions(aSelect) {
    /* As far as I can tell, browsers that don't hide <option> tags
     * also never have a X position for <option> tags, even if
     * they're visible. This is the only reliable way I found to
     * differentiate browsers. So we create a visible option, see
     * if it has a position, and then remove it. */
    if (typeof(browser_can_hide_options) == "undefined") {
        var new_opt = bz_createOptionInSelect(aSelect, '', '');
        var opt_pos = YAHOO.util.Dom.getX(new_opt);
        aSelect.removeChild(new_opt);
        if (opt_pos) {
            browser_can_hide_options = true;
        }
        else {
            browser_can_hide_options = false;
        }
    }
    return browser_can_hide_options;
}

/* (end) option hiding code */

/**
 * The Autoselect
 */
YAHOO.bugzilla.userAutocomplete = {
    counter : 0,
    dataSource : null,
    generateRequest : function ( enteredText ){ 
      YAHOO.bugzilla.userAutocomplete.counter = 
                                   YAHOO.bugzilla.userAutocomplete.counter + 1;
      YAHOO.util.Connect.setDefaultPostHeader('application/json', true);
      var json_object = {
          method : "User.get",
          id : YAHOO.bugzilla.userAutocomplete.counter,
          params : [ { 
            match : [ decodeURIComponent(enteredText) ],
            include_fields : [ "name", "real_name" ]
          } ]
      };
      var stringified =  YAHOO.lang.JSON.stringify(json_object);
      var debug = { msg: "json-rpc obj debug info", "json obj": json_object, 
                    "param" : stringified}
      YAHOO.bugzilla.userAutocomplete.debug_helper( debug );
      return stringified;
    },
    resultListFormat : function(oResultData, enteredText, sResultMatch) {
        return ( YAHOO.lang.escapeHTML(oResultData.real_name) + " ("
                 + YAHOO.lang.escapeHTML(oResultData.name) + ")");
    },
    debug_helper : function ( ){
        /* used to help debug any errors that might happen */
        if( typeof(console) !== 'undefined' && console != null && arguments.length > 0 ){
            console.log("debug helper info:", arguments);
        }
        return true;
    },    
    init_ds : function(){
        this.dataSource = new YAHOO.util.XHRDataSource("jsonrpc.cgi");
        this.dataSource.connTimeout = 30000;
        this.dataSource.connMethodPost = true;
        this.dataSource.connXhrMode = "cancelStaleRequests";
        this.dataSource.maxCacheEntries = 5;
        this.dataSource.responseSchema = {
            resultsList : "result.users",
            metaFields : { error: "error", jsonRpcId: "id"},
            fields : [
                { key : "name" },
                { key : "real_name"}
            ]
        };
    },
    init : function( field, container, multiple ) {
        if( this.dataSource == null ){
            this.init_ds();  
        }            
        var userAutoComp = new YAHOO.widget.AutoComplete( field, container, 
                                this.dataSource );
        // other stuff we might want to do with the autocomplete goes here
        userAutoComp.maxResultsDisplayed = BUGZILLA.param.maxusermatches;
        userAutoComp.generateRequest = this.generateRequest;
        userAutoComp.formatResult = this.resultListFormat;
        userAutoComp.doBeforeLoadData = this.debug_helper;
        userAutoComp.minQueryLength = 3;
        userAutoComp.autoHighlight = false;
        // this is a throttle to determine the delay of the query from typing
        // set this higher to cause fewer calls to the server
        userAutoComp.queryDelay = 0.05;
        userAutoComp.useIFrame = true;
        userAutoComp.resultTypeList = false;
        if( multiple == true ){
            userAutoComp.delimChar = [","];
        }
        
    }
};

YAHOO.bugzilla.keywordAutocomplete = {
    dataSource : null,
    init_ds : function(){
        this.dataSource = new YAHOO.util.LocalDataSource( YAHOO.bugzilla.keyword_array );
    },
    init : function( field, container ) {
        if( this.dataSource == null ){
            this.init_ds();
        }
        var keywordAutoComp = new YAHOO.widget.AutoComplete(field, container, this.dataSource);
        keywordAutoComp.maxResultsDisplayed = YAHOO.bugzilla.keyword_array.length;
        keywordAutoComp.formatResult = keywordAutoComp.formatEscapedResult;
        keywordAutoComp.minQueryLength = 0;
        keywordAutoComp.useIFrame = true;
        keywordAutoComp.delimChar = [","," "];
        keywordAutoComp.resultTypeList = false;
        keywordAutoComp.queryDelay = 0;
        /*  Causes all the possibilities in the keyword to appear when a user 
         *  focuses on the textbox 
         */
        keywordAutoComp.textboxFocusEvent.subscribe( function(){
            var sInputValue = YAHOO.util.Dom.get('keywords').value;
            if( sInputValue.length === 0 ){
                this.sendQuery(sInputValue);
                this.collapseContainer();
                this.expandContainer();
            }
        });
        keywordAutoComp.dataRequestEvent.subscribe( function(type, args) {
            args[0].autoHighlight = args[1] != '';
        });
    }
};
