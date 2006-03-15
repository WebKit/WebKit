/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// FOR TESTING XXX

function StartCodeTime ( ) {
	CodeTimeInitial = new Date();
	}

function EndCodeTime ( str ) {
	var CodeTimeFinal = new Date();
	var diff = CodeTimeFinal.getTime() - CodeTimeInitial.getTime();
	dump("Timing " + str + " took " + diff + " milliseconds.\n");
	}

// ************************************************************
// Initialize stuff
// ************************************************************

haveRead = false;
currentTime = new Date(); // a new Date object with the current time
monthShowing = new Date();
nowShowing = new Date();

month_names_internal = new Array("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");


// the namespace of the RDF literals
rdf_ns="http://when.com/1999/07/03-appts-rdf#";

// find the location of the appointments data - relative URLs don't work (now, anyway)

// the filename within the directory of the XUL document
var RDFRelFileName = 'bug12012.rdf';

dump("Getting window.location.href...\n");
// to get the directory, start with the filename of the XUL document
var baseURL = window.location.href;
dump("...done.\n");

// XXX Needs three slashes because of file:// URL parsing bug (see Bug 9236)
//if ((baseURL.indexOf("file:/") != -1) && (baseURL.indexOf("file:///") == -1)) {
	//baseURL = "file:///" + baseURL.substring(6);
//}
if (baseURL.indexOf("file:") == 0) {
	baseURL = baseURL.substring(5);
	while (baseURL.charAt(0) == '/') {
		baseURL = baseURL.substring(1);
	}
	baseURL = "file:///" + baseURL;
}

// and take off whatever is after the last "/"
baseURL = baseURL.substring(0, baseURL.lastIndexOf("/") + 1);

// combine these to get the filename for the RDF
RDFFileName = baseURL + RDFRelFileName;
appts_resourcename = RDFFileName + "#WhenComAppointmentsRoot";

// the rdf service
RDFService = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
RDFService = RDFService.QueryInterface(Components.interfaces.nsIRDFService);

// ************************************************************
// Event handler for Edit code
// ************************************************************

function ascendToTR (node) {
	var rv = node;
	while ( rv.nodeName.toLowerCase() != "tr" ) {
		rv = rv.parentNode;
	}
	return rv;
}

function handleRClick ( event ) {
	// XXX event.type and event.currentNode and event.button are broken
	var currentNode = ascendToTR(event.target);
	EditApptById(currentNode.getAttribute("id"));
}

// ************************************************************
// Functions for event handlers (e.g., onclick)
// ************************************************************

function gotoURL( url ) {
	window.content.location = url;
	}

function getAddDiv() {
	return document.documentElement.firstChild.nextSibling.nextSibling.firstChild.firstChild;
	}

function getScheduleDiv() {
	return document.documentElement.firstChild.nextSibling.nextSibling.firstChild.firstChild.nextSibling;
	}

function getPickDiv() {
	return document.documentElement.firstChild.nextSibling.nextSibling.firstChild.firstChild.nextSibling.nextSibling;
	}

function buttonDisabled ( number ) {
	// 0 for AddEvent
	// 1 for Schedule
	// 2 for Go To Date

	var tabTR = document.getElementById("tabsTR");
	tabTR.childNodes.item(0).className = "tab";
	tabTR.childNodes.item(1).className = "tab";
	tabTR.childNodes.item(2).className = "tab";
	tabTR.childNodes.item(number).className = "tab disable";

	// XXX BUG These two lines cause failure to display
	// setting is on an XULElement
	//tabTR.parentNode.parentNode.style.border="medium solid black";
	//tabTR.parentNode.parentNode.style.border="none";

	// XXX BUG These two lines cover up most of a shifting bug...
	// Setting is on a table
	tabTR.parentNode.style.border="medium solid black";
	tabTR.parentNode.style.border="none";
	
}

function divActive( number ) {
	// 0 for AddEvent
	// 1 for Schedule
	// 2 for Go To Date

	buttonDisabled(number)

	// for speed:
	var AddDiv = getAddDiv();
	var ScheduleDiv = getScheduleDiv();
	var PickDiv = getPickDiv()

	if ( number == 0 ) {
		ScheduleDiv.style.display = "none";
		PickDiv.style.display = "none";
		AddDiv.style.display = "block";
	} else if ( number == 1 ) {
		AddDiv.style.display = "none";
		PickDiv.style.display = "none";
		ScheduleDiv.style.display = "block";
		ShowSchedule();
	} else if ( number == 2 ) {
		AddDiv.style.display = "none";
		ScheduleDiv.style.display = "none";
		PickDiv.style.display = "block";
		ShowPick();
	}
}

function editButtonsActive( number ) {
	// 0 for addbuttons
	// 1 for editbuttons

	var adddiv = getAddDiv();
	var addbdiv = GetElementWithIdIn("addbuttons", adddiv);
	var edbdiv = GetElementWithIdIn("editbuttons", adddiv);

	if (number == 0) {
		edbdiv.style.display = "none";
		addbdiv.style.display = "block";
	} else if (number == 1) {
		addbdiv.style.display = "none";
		edbdiv.style.display = "block";
	}

}

function nextDay() {
	nowShowing.setDate(nowShowing.getDate() + 1);
	ShowSchedule();
}

function nextWeek() {
	nowShowing.setDate(nowShowing.getDate() + 7);
	ShowSchedule();
}

function prevDay() {
	nowShowing.setDate(nowShowing.getDate() - 1);
	ShowSchedule();
}

function prevWeek() {
	nowShowing.setDate(nowShowing.getDate() - 7);
	ShowSchedule();
}

function today() {
	nowShowing = new Date();
	ShowSchedule();
	}

/* Date Picker */

function nextMonth() {
	monthShowing.setMonth(monthShowing.getMonth() + 1);
	ShowPick();
}

function prevMonth() {
	monthShowing.setMonth(monthShowing.getMonth() - 1);
	ShowPick();
}

function nextYear() {
	monthShowing.setFullYear(monthShowing.getFullYear() + 1);
	ShowPick();
}

function prevYear() {
	monthShowing.setFullYear(monthShowing.getFullYear() - 1);
	ShowPick();
}

function thisMonth() {
	monthShowing = new Date();
	ShowPick();
	}

function pickDate( year, month, date ) {
	// the month is 1..12
	nowShowing = new Date(year, month - 1, date);
	divActive(1); // calls ShowSchedule
}


// ************************************************************
// Utility functions
// ************************************************************

// XXX Should this be needed?

function GetElementWithIdIn ( id, ancestor ) {
// recursive search for an element with a given id
	if ( ancestor.getAttribute("id") == id ) {
		return ancestor;
	} else {  // else shows logic, not really needed because of return
		for (var i = 0; i < ancestor.childNodes.length ; i++ ) {
			// This isn't an official way to use named constants, but
			// Mozilla supports it
			if ( ancestor.childNodes.item(i).nodeType == Node.ELEMENT_NODE ) {
				var rv = GetElementWithIdIn(id, ancestor.childNodes.item(i));
				if (rv != null) {
					return rv; // exits the function here, since we found the target
				}
			}
		}
		return null;
	}
}

// ************************************************************
// The Appointment type
// ************************************************************

function ApptToString () {
	return "[" + String(this.date) + "," + this.title + "]";
}

Appointment.prototype.toString = ApptToString;

function Appointment ( date, title, desc, dur, id ) { // used as a constructor
	this.date = date; // date
	this.title = title; // title
	this.desc = desc; // description
	this.dur = dur; // duration (minutes)
	this.id = id; // id (in the RDF and in the table row)

// A useful debugging function:
// XXX If error in string, get silent error:
	//this.toString = new Function('return "[" + String(this.date) + "," + this.title + "]"');
	}

function compareAppt ( x , y ) { // for passing to Array::sort
	// should return negative if x<y, 0 if x=y, and positive if x>y
	// Date::getTime returns milliseconds since Jan 1, 1970, so
	return x.date.getTime() - y.date.getTime();
}

// ************************************************************
// Code used for showing the schedule
// ************************************************************

function getOutputTBody() {
	// get the TBody element for output
	// XXX a hack, since getElementById doesn't work with mixed XUL/HTML, it
	// seems.

	//return document.documentElement.childNodes.item(2).childNodes.item(1);

	// hack within a hack, to get around bug 8044
	// for thead:
	return getScheduleDiv().firstChild.nextSibling.firstChild.nextSibling;
	// for no thead:
	// return getScheduleDiv().firstChild.nextSibling.firstChild;
}

function getOutputForDate() {
 // 2, 1, 3
	return getScheduleDiv().firstChild.firstChild.firstChild.nextSibling.nextSibling;
}

function timeString( date ) {
	var minutes = date.getMinutes();
	if (minutes < 10 ) {
		minutes = "0" + minutes;
	}
	var hours = date.getHours();
	if (hours < 12) {
		var suffix = "A";
	} else {
		var suffix = "P";
	}
	if (hours == 0) {
		hours = 12;
	} else if (hours > 12) {
		hours -= 12;
	}
	return hours + ":" + minutes + suffix;
}

function dateString( date ) {
// month is 0..11
	// ISO-8601 version:
	// return date.getFullYear() + "-" + ( date.getMonth() + 1) + "-" + date.getDate();

	return month_names_internal[date.getMonth()] +  // both 0-based
		" " + date.getDate() + ", " + date.getFullYear();
}

function getAttr(rdf_datasource,service,attr_name) {
	var attr = rdf_datasource.GetTarget(service,
					 RDFService.GetResource(rdf_ns + attr_name),
					 true);
	if (attr)
		attr = attr.QueryInterface(
					Components.interfaces.nsIRDFLiteral);
	if (attr)
			attr = attr.Value;
	return attr;
}

function ReadAndSort () {
	if ( ! haveRead ) {
		// enumerate all of the flash datasources.
		var enumerator = appts_container.GetElements();

		// put all the appointments into an array
		allAppts = new Array();
	
		try {
			while (enumerator.hasMoreElements()) {
				var service = enumerator.getNext().QueryInterface(
												Components.interfaces.nsIRDFResource);
		
				// get the title text
				var title = getAttr(rdf_datasource, service, 'title');
		
				// get the description text
				var descrip = getAttr(rdf_datasource, service, 'notes');
		
				// get the date, and see if it's for us
				var year = getAttr(rdf_datasource, service, 'year');
				var month = getAttr(rdf_datasource, service, 'month');
				var date = getAttr(rdf_datasource, service, 'date');
				var hour = getAttr(rdf_datasource, service, 'hour');
				var minute = getAttr(rdf_datasource, service, 'minute');

				// get the full resource URL:
				var theid = service.Value;
				// and use only the fragment identifier:
				theid = theid.substring(theid.lastIndexOf("#") + 1);

				var duration = getAttr(rdf_datasource, service, 'duration');
				// month is 0..11
				var apptDate = new Date(year, month - 1, date, hour, minute);

				allAppts[allAppts.length] = new Appointment( apptDate, title, descrip, duration, theid );
		}
		} catch (ex) {
			window.alert("Caught exception [[" + ex + "]]\n");
		}
	haveRead = true;
	}

	var todaysAppts = new Array();

	for ( var i = 0 ; i < allAppts.length ; i++ ) {
		var appt = allAppts[i];
		if ( (appt.date.getFullYear() == nowShowing.getFullYear())
			&& (appt.date.getMonth() == nowShowing.getMonth())
			&& (appt.date.getDate() == nowShowing.getDate()) ) {

		todaysAppts[todaysAppts.length] = appt;
		}
	}

	// sort todaysAppts using the JavaScript builtin Array::sort(), by
	// providing a sort function
	todaysAppts.sort( compareAppt );

	return todaysAppts;
	}

function ShowSchedule() {
	// Get the nodes where we will output
	var outTB = getOutputTBody();
	var outDate = getOutputForDate();

	// Remove all its existing children, since we'll rewrite them
	while (outTB.hasChildNodes() ) {
		outTB.removeChild(outTB.firstChild);
	}
	while (outDate.hasChildNodes() ) {
		outDate.removeChild(outDate.firstChild);
	}

	// Write the date at the top
	outDate.appendChild(document.createTextNode(dateString(nowShowing)));

	// and write the appointments to the table...

	// XXX Hack: get around insertRow() bug.
	var newrow = outTB.insertRow(outTB.rows.length);

	// Get the appointments for today, already sorted, from some kind of input
	// this is an Array of Appointment

	var todaysAppts = ReadAndSort();

	for ( var i = 0 ; i < todaysAppts.length ; i++ ) {
		var appt = todaysAppts[i];

		// add the appointment to the list:

		var newrow = outTB.insertRow(outTB.rows.length);

		// set up event handling
		newrow.setAttribute("id", appt.id);
		// XXX extra param shouldn't be needed
		newrow.addEventListener("click", handleRClick, false, false);

		// XXX Hack: get around insertCell() bug by switching order.
		var titlecell = newrow.insertCell(1);
		var timecell = newrow.insertCell(0);
		timecell.appendChild(document.createTextNode(timeString(appt.date)));
		titlecell.appendChild(document.createTextNode(appt.title));
	}
}

// ************************************************************
// Code used for the Goto date function
// ************************************************************

// monthShowing holds Year and Month for current Calendar

function monthString( date ) {
	// ISO-8601 format
	// return date.getFullYear() + "-" + ( date.getMonth() + 1);
	return month_names_internal[date.getMonth()] +  // both 0-based
		" " + date.getFullYear();
}

function ShowPick() {
	var pickDiv = getPickDiv(); // for speed
// StartCodeTime();
	var outCal = GetElementWithIdIn("calendar", pickDiv);
	var outDate = GetElementWithIdIn("MonthOutput", pickDiv);
// EndCodeTime("two runs of GetElementWithIdIn");

// StartCodeTime();
	// Remove all existing children, since we'll rewrite them
	while (outCal.hasChildNodes() ) {
		outCal.removeChild(outCal.firstChild);
	}
	while (outDate.hasChildNodes() ) {
		outDate.removeChild(outDate.firstChild);
	}
// EndCodeTime("removing existing children");

// StartCodeTime();
	// Write the month at the top
	outDate.appendChild(document.createTextNode(monthString(monthShowing)));
// EndCodeTime("initial stuff");

	// and write the calendar to the table...

// StartCodeTime();
	// XXX Hack: get around insertRow() bug.
	var myrow = outCal.insertRow(0);
// EndCodeTime("initial stuff");

	// make room before the first day, which starts on a strange day of the week
	// note that getDay(), 0=Sunday and 6=Saturday

// StartCodeTime();
	var curDay = new Date(monthShowing); // get the month we want to show
	curDay.setDate(1); // start with the first day of the month
// EndCodeTime("setting curDay");

// StartCodeTime();
	myrow = outCal.insertRow(outCal.rows.length);
	var junkcell = myrow.insertCell(0); // XXX Bug in insertCell
	for (var i = 0 ; i < curDay.getDay() ; i++ ) {
		myrow.insertCell(myrow.cells.length);
		}
// EndCodeTime("writing extra cells");

// StartCodeTime();
	var mycell;
	var targMonth = monthShowing.getMonth(); // for speed - saves 30ms
	// for speed:
	var testNowShowing = (
		( monthShowing.getFullYear() == nowShowing.getFullYear()) &&
		( monthShowing.getMonth() == nowShowing.getMonth()) );
	var testCurrentTime = (
		( monthShowing.getFullYear() == currentTime.getFullYear()) &&
		( monthShowing.getMonth() == currentTime.getMonth()) );

	while ( curDay.getMonth() == targMonth ) {
		if ( ( curDay.getDay() == 0) && (curDay.getDate() != 1) ) {
			// if it's Sunday, and not the 1st of the month
			// then add a new row
			myrow = outCal.insertRow(outCal.rows.length);
			junkcell = myrow.insertCell(0); // XXX Bug in insertCell
		}
		mycell = myrow.insertCell( myrow.cells.length );
		// put the number of the current date in the cell
		mycell.appendChild( document.createTextNode( curDay.getDate() ) );
		// and set the onClick handler
		mycell.setAttribute("onclick", "pickDate(" + curDay.getFullYear()
		                               + "," + (curDay.getMonth() + 1)
									   + "," + curDay.getDate()
									   + ")");
		// and set the class
		var classNm = "caldate";
		if ( testNowShowing && ( curDay.getDate() == nowShowing.getDate())) {
			classNm += " nowshowing";
			}
		if ( testCurrentTime && ( curDay.getDate() == currentTime.getDate())) {
			classNm += " today";
			}
		mycell.setAttribute("class", classNm);
		// and loop...
		curDay.setDate(curDay.getDate() + 1);
	}
// EndCodeTime("writing the days");
}


// ************************************************************
// Code used for Adding to the data
// ************************************************************

function makeAssert ( source, data, name ) {
	var target = RDFService.GetLiteral(data);
	var arc = RDFService.GetResource(rdf_ns + name);
	rdf_datasource.Assert(source, arc, target, true);
}

function SendBackToSource( appt ) {

	// make a resource for the new appointment

	// These four all write, but only first appt works
	// var sourceNode = RDFService.GetResource("http://www.w3.org/1999/02/22-rdf-syntax-ns#Description");
	// var sourceNode = RDFService.GetResource("");
	// var sourceNode = RDFService.GetResource("aoeuidhtns");
	// var sourceNode = RDFService.GetResource("http://when.com/aoeuidhtns#");

	// milliseconds since 1970-01-01 00:00 should be a good enough
	// unique identifier (creation time of appt)
	var sourceNode
		= RDFService.GetResource(RDFFileName +"#" + appt.id);

	// create literals and assert a relationship to the new appointment
	// for each piece of data

	makeAssert( sourceNode, appt.date.getFullYear(), 'year');
	makeAssert( sourceNode, appt.date.getMonth() + 1, 'month'); // month is 0..1 in the date structure
	makeAssert( sourceNode, appt.date.getDate(), 'date');
	makeAssert( sourceNode, appt.date.getHours(), 'hour');
	makeAssert( sourceNode, appt.date.getMinutes(), 'minute');
	makeAssert( sourceNode, appt.title, 'title');
	makeAssert( sourceNode, appt.desc, 'notes');
	makeAssert( sourceNode, appt.dur, 'duration');

	// connect the new appointment to the datasource 

	appts_container.AppendElement( sourceNode );

	// write the datasource back to disk

	try {
		rdf_remote_datasource.Flush(); // must be a RemoteDataSource, after 1999-06-23
	} catch (ex) {
		window.alert("Caught exception [[" + ex + "]] in Flush().\n");
	}

}

function Add( appt ) {

	// Add it to our internal data	-- note that we don't show it!
	allAppts[allAppts.length] = appt;

	// and add it to the external data
	SendBackToSource( appt );

}

function ResetAddFormTo( year, month, date, hrs, min, durhrs, durmin, title, notes) {
	var addDiv = getAddDiv(); // for speed

	var yearselect = GetElementWithIdIn("addyear", addDiv );
	var monthselect = GetElementWithIdIn("addmonth", addDiv );
	var dayselect = GetElementWithIdIn("addday", addDiv );
	var hourselect = GetElementWithIdIn("addtimehrs", addDiv );
	var minselect = GetElementWithIdIn("addtimemin", addDiv );
	var durhourselect = GetElementWithIdIn("adddurhrs", addDiv );
	var durminselect = GetElementWithIdIn("adddurmin", addDiv );

	var titleInput = GetElementWithIdIn("addtitle", addDiv );
	var notesInput = GetElementWithIdIn("addnotes", addDiv );

	if (yearselect != null) {  // "if (yearselect)" works too
		// Subtract the value of the first year in the list to set
		// selectedIndex.  JavaScript converts the string value to a number.
		yearselect.selectedIndex =
			year - yearselect.options.item(0).value;
	} else {
		window.alert("Hit a null.");
	}

	if (monthselect != null) {
		// selectedIndex 0 based, so subtract one
    	monthselect.selectedIndex = month - 1;
	} else {
		window.alert("Hit a null.");
	}

	if (dayselect != null) {
		// selectedIndex 0 based, so subtract one
		dayselect.selectedIndex = date - 1;
	} else {
		window.alert("Hit a null.");
	}

	if (hourselect != null) {
		// both 0 based
		hourselect.selectedIndex = hrs ;
	} else {
		window.alert("Hit a null.");
	}

	if (minselect != null) {
		minselect.selectedIndex = Math.round(min / 5);
	} else {
		window.alert("Hit a null.");
	}

	if (durhourselect != null) {
		durhourselect.selectedIndex = durhrs;
	} else {
		window.alert("Hit a null.");
	}

	if (durminselect != null) {
		durminselect.selectedIndex = Math.round(durmin / 5);
	} else {
		window.alert("Hit a null.");
	}

	if (titleInput != null) {
		if ( title == null) {
			title = "";
		}
		titleInput.value = title;
	} else {
		window.alert("Hit a null.");
	}

	if (notesInput != null) {
		if ( notes == null) {
			notes = "";
		}
		notesInput.value = notes;
	} else {
		window.alert("Hit a null.");
	}

}

function ResetAddForm() {
	editButtonsActive(0);
	ResetAddFormTo( nowShowing.getFullYear(),
	                nowShowing.getMonth() + 1,
                    nowShowing.getDate(),
					nowShowing.getHours() + 1, // upcoming hour
					0, // minutes
					0, // duration hours
					0, // duration minutes
					"", // title
					""); // notes
}

function SubmitAddForm() {
	var addDiv = getAddDiv(); // for speed

	var title = GetElementWithIdIn("addtitle", addDiv );
	var years = GetElementWithIdIn("addyear", addDiv );
	var months = GetElementWithIdIn("addmonth", addDiv );
	var days = GetElementWithIdIn("addday", addDiv );
	var timehrs = GetElementWithIdIn("addtimehrs", addDiv );
	var timemin = GetElementWithIdIn("addtimemin", addDiv );
	var durhrs = GetElementWithIdIn("adddurhrs", addDiv );
	var durmin = GetElementWithIdIn("adddurmin", addDiv );
	var notes = GetElementWithIdIn("addnotes", addDiv );

	if ( ( title == null ) ||
		( years == null ) ||
		( months == null ) ||
		( days == null ) ||
		( timehrs == null ) ||
		( timemin == null ) ||
		( durhrs == null ) ||
		( durmin == null ) ||
		( notes == null ) ) {
		window.alert("Something is null.  Addition failed.\n");
	} else {
		// months go 0..11 
		var apptid = "appt" + (new Date().getTime());
		Add( new Appointment( new Date ( years.value, months.value - 1,
		                                 days.value, timehrs.value,
										 timemin.value ),
		                      title.value,
							  notes.value,
							  (durhrs.value * 60) + (durmin.value * 1),
							  apptid));
		// ResetAddForm();
		divActive(1);
	}
}

// ************************************************************
// Edit code, which uses add form
// ************************************************************

function getApptById( id ) {
	for (var i = 0 ; i < allAppts.length ; i++ ) {
		if ( allAppts[i].id == id ) {
			return allAppts[i];
		}
	}
	return null;
}

function removeApptById ( id ) {
	for (var i = 0 ; i < allAppts.length ; i++ ) {
		if ( allAppts[i].id == id ) {
			// Remove from list
			allAppts[i] = allAppts[allAppts.length - 1];
			allAppts[allAppts.length - 1] = null;
			allAppts.length -= 1;
			return;
		}
	}
}

function EditApptById( theid ) {
	idEditing = theid; // global variable

	var apptEditing = getApptById( idEditing );
	if ( apptEditing == null) {
		window.alert("Null appointment.  Something's wrong.\n");
		return;
		}
	divActive(0); // XXX This should come later, but...
	editButtonsActive(1);
	ResetAddFormTo( apptEditing.date.getFullYear(),
	                apptEditing.date.getMonth() + 1,
					apptEditing.date.getDate(),
					apptEditing.date.getHours(),
					apptEditing.date.getMinutes(),
					Math.floor(apptEditing.dur / 60),
					apptEditing.dur % 60,
					apptEditing.title,
					apptEditing.desc );
}

function getAttrLiteral(rdf_datasource,service,attr_name) {
	var attr = rdf_datasource.GetTarget(service,
					 RDFService.GetResource(rdf_ns + attr_name),
					 true);
	if (attr)
		attr = attr.QueryInterface(
					Components.interfaces.nsIRDFLiteral);
	return attr;
}

function makeUnAssert ( rdf_datasource, resource, name ) {
	var target = getAttrLiteral(rdf_datasource, resource, name);
	var data = getAttr(rdf_datasource, resource, name); // function above
	var arc = RDFService.GetResource(rdf_ns + name);
	rdf_datasource.Unassert(resource, arc, target);
}

function DeleteEditing() {
	// delete from our list:
	removeApptById(idEditing);

	// delete from the data source
	var sourceNode = RDFService.GetResource(RDFFileName +"#" + idEditing);
	appts_container.RemoveElement( sourceNode, true );

	// Need to unassert all of sourceNode's relationships
	makeUnAssert(rdf_datasource, sourceNode, 'year');
	makeUnAssert(rdf_datasource, sourceNode, 'month');
	makeUnAssert(rdf_datasource, sourceNode, 'date');
	makeUnAssert(rdf_datasource, sourceNode, 'hour');
	makeUnAssert(rdf_datasource, sourceNode, 'minute');
	makeUnAssert(rdf_datasource, sourceNode, 'title');
	makeUnAssert(rdf_datasource, sourceNode, 'notes');
	makeUnAssert(rdf_datasource, sourceNode, 'duration');

	try {
		rdf_remote_datasource.Flush(); // must be a RemoteDataSource, after 1999-06-23
	} catch (ex) {
		window.alert("Caught exception [[" + ex + "]] in Flush().\n");
	}

}

function EditFormSubmit() {
	DeleteEditing();
	SubmitAddForm();
	divActive(1);
}

function EditFormDelete() {
	DeleteEditing();
	divActive(1);
}

function EditFormCancel() {
	divActive(1);
}

// ************************************************************
// Startup Code
// ************************************************************

function Init()
{
	// Initialize the Sidebar

	// Install all the datasources named in the Flash Registry into
	// the tree control. Datasources are listed as members of the
	// NC:FlashDataSources sequence, and are loaded in the order that
	// they appear in that sequence.
	try {
		// First try to construct a new one and load it
		// synchronously. nsIRDFService::GetDataSource() loads RDF/XML
		// asynchronously by default.
		rdf_datasource = Components.classes['@mozilla.org/rdf/datasource;1?name=xml-datasource'].createInstance();
		rdf_datasource = rdf_datasource.QueryInterface(Components.interfaces.nsIRDFDataSource);

		rdf_remote_datasource = rdf_datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
		rdf_remote_datasource.Init(RDFFileName); // this will throw if it's already been opened and registered.

		dump("Reading datasource synchronously.\n");
		// read it in synchronously.
		rdf_remote_datasource.Refresh(true);
	}
	catch (ex) {
		// if we get here, then the RDF/XML has been opened and read
		// once. We just need to grab the datasource.
		dump("Datasource already read.  Grabbing.\n");
		rdf_datasource = RDFService.GetDataSource(RDFFileName);
	}

	// Create a 'container' wrapper around the appts_resourcename
	// resource so we can use some utility routines that make access a
	// bit easier.
	// NOT var, so it's global
	appts_container = Components.classes['@mozilla.org/rdf/container;1'].createInstance();
	appts_container = appts_container.QueryInterface(Components.interfaces.nsIRDFContainer);

	appts_resource = RDFService.GetResource(appts_resourcename);
	appts_container.Init(rdf_datasource, appts_resource);
}

function Boot() {

	if (document.getElementById("ApptsWindow")) {
dump("Calling init:\n");
		Init();

		// XXX Bug 9136 - this doesn't work when form is hidden.
		// ResetAddForm();

		// ShowSchedule();
dump("Calling divActive(1):\n");
		divActive(1);
dump("Done initializing.\n");
		// and let events take things from there...
	} else {
		setTimeout("Boot()", 0);
	}
}
