var monthStrings = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
// Date object that represents the currently displayed month
var monthOnDisplay;
// Table that maps a date (in milliseconds) to the corresponding Day object
var dateToDayObjectMap;
// Calendar type currently selected
var selectedCalendarType = "home";
// Calendar event currently selected
var selectedCalendarEvent = null;
// The highest ID number that we have assigned so far
var highestID = 0;
// Date (in milliseconds) of the first day displayed in the calendar
var calendarStartTime = 0;
// Date (in milliseconds) of the last day displayed in the calendar
var calendarEndTime = 0;
// Audio sound for network connection lost event
var networkDropSound = new Audio("Boom.aiff");

// Override day box height if the number of rows in calendar is not 5 
var insertedStyleRuleIndexForDayBox = -1;
function updateCalendarRowCount(count) 
{
    var stylesheet = document.styleSheets[0];
    if (count == 5) {   // The case our stylesheet already handles, and probably the most common case
        if (insertedStyleRuleIndexForDayBox >= 0) {
            // Remove the style rule we previously added
            stylesheet.deleteRule(insertedStyleRuleIndexForDayBox);
        }
        insertedStyleRuleIndexForDayBox = -1;
        return;
    }
    if (insertedStyleRuleIndexForDayBox < 0)
        insertedStyleRuleIndexForDayBox = stylesheet.insertRule(".day.box { }", stylesheet.cssRules.length);
    var styleRule = stylesheet.cssRules[insertedStyleRuleIndexForDayBox];
    var h = 100 / count;
    styleRule.style.height = h + "%";
}

function initMonthOnDisplay() 
{
    monthOnDisplay = CalendarState.currentMonth();
    monthOnDisplayUpdated();
}

function monthOnDisplayUpdated() 
{
    var monthTitle = monthStrings[monthOnDisplay.getMonth()] + " " + monthOnDisplay.getFullYear();
    document.getElementById("monthTitle").innerText = monthTitle;
    CalendarState.setCurrentMonth(monthOnDisplay.toString());
    initDaysGrid();
    CalendarDatabase.openWebKitCalendarEvents(); 
}

function initDaysGrid() 
{
    // Figure out the first day that should be displayed in this grid.
    var displayedDate = new Date(monthOnDisplay);
    displayedDate.setDate(displayedDate.getDate() - displayedDate.getDay());
    
    // Fill out the entire grid
    var daysGrid = document.getElementById("daysGrid");
    daysGrid.removeChildren();
    dateToDayObjectMap = new Object();
    
    calendarStartTime = displayedDate.getTime();
    var doneWithThisMonth = false;
    var rows = 0;
    while (!doneWithThisMonth) {
        rows++;
        for (var i = 0; i < 7; ++i) {
            var dateTime = displayedDate.getTime();
            var dayObj = new Day(displayedDate);
            dateToDayObjectMap[dateTime] = dayObj;
            dayObj.attach(daysGrid);
            // Increment by a day
            displayedDate.setDate(displayedDate.getDate() + 1);
        }
        doneWithThisMonth = (displayedDate.getMonth() != monthOnDisplay.getMonth());
    }
    updateCalendarRowCount(rows);
    displayedDate.setDate(displayedDate.getDate() - 1);     // Roll back to the last displayed date
    displayedDate.setHours(23, 59, 59, 999);
    calendarEndTime = displayedDate.getTime();
}

function isCalendarTypeVisible(type) 
{
    var input = document.getElementById(type);
    if (!input || input.tagName != "INPUT")
        return;
    return input.checked;
}

function addEventsOfCalendarType(calendarType)
{
    CalendarDatabase.loadEventsFromDBForCalendarType(calendarType);
}

function removeEventsOfCalendarType(calendarType)
{
    var daysGridElement = document.getElementById("daysGrid");
    var dayElements = daysGridElement.childNodes;
    for (var i = 0; i < dayElements.length; i++) {
        var dayObj = dayObjectFromElement(dayElements[i]);
        if (!dayObj)
            continue;
        dayObj.hideEventsOfCalendarType(calendarType);
    }
}

// Event handlers --------------------------------------------------

function pageLoaded() 
{
    document.getElementById("gridView").addEventListener("selectstart", stopEvent, true);
    document.getElementById("searchResults").addEventListener("selectstart", stopEvent, true);
    document.body.addEventListener("keyup", keyUpHandler, false);
    document.body.addEventListener("online", displayOnlineStatus, true);
    document.body.addEventListener("offline", displayOnlineStatus, true);
    displayOnlineStatus();

    initSearchArea();
    // Initialize the checked states of the calendars
    var calendarCheckboxes = document.getElementById("calendarList").getElementsByTagName("INPUT");
    for (var i = 0; i < calendarCheckboxes.length; i++)
        calendarCheckboxes[i].checked = CalendarState.calendarChecked(calendarCheckboxes[i].id);
    // Initialize the calendar grid.
    initMonthOnDisplay();
}

function previousMonth() 
{
    monthOnDisplay.setMonth(monthOnDisplay.getMonth() - 1);
    monthOnDisplayUpdated();
}

function nextMonth() 
{
    monthOnDisplay.setMonth(monthOnDisplay.getMonth() + 1);
    monthOnDisplayUpdated();
}

function calendarSelected(event) 
{
    if (event.target.tagName == "INPUT")
        return;
    var oldSelectedInput = document.getElementById(selectedCalendarType);
    var oldListItemElement = oldSelectedInput.findParentOfTagName("LI");
    oldListItemElement.removeStyleClass("selected");
    event.target.addStyleClass("selected");
    selectedCalendarType = event.target.getElementsByTagName("INPUT")[0].id;
}

function calendarClicked(event) 
{
    if (event.target.tagName != "INPUT")
        return;
    var calendarType = event.target.id;
    var checked = event.target.checked;
    CalendarState.setCalendarChecked(calendarType, checked);
    if (checked)
        addEventsOfCalendarType(calendarType);
    else
        removeEventsOfCalendarType(calendarType);
}

function keyUpHandler(event) 
{
    switch (event.keyIdentifier) {
        case "U+007F":   // Delete key
            if (selectedCalendarEvent && selectedCalendarEvent.day)
                selectedCalendarEvent.day.deleteEvent(selectedCalendarEvent);
            break;
        case "Enter":
            if (selectedCalendarEvent && !document.getElementById("eventOverlay").hasStyleClass("show"))
                selectedCalendarEvent.selected();
            break;
        default:
            break;
    }
}

function eventDetailsDismissed(event) 
{
    if (selectedCalendarEvent) {
        selectedCalendarEvent.listItemNode.removeStyleClass("selected");
        // Update selected event
        selectedCalendarEvent.detailsUpdated();
    }
    
    // Hide event details
    var eventOverlayElement = document.getElementById("eventOverlay");
    // FIXME: would have added transitionend listener here but not supported yet.  Use setTimeout for now.
    // Set timer to hide map and disclosure arrow - this is delayed so things wouldn't change while event details is fading out.
    setTimeout("hideMapDisclosureArrow()", 1000);
    eventOverlayElement.removeStyleClass("show");

    selectedCalendarEvent = null;

    // Show the gridView
    document.getElementById("gridView").removeStyleClass("inactive");
}

function searchForEvent(query) 
{
    if (query.length == 0) {
        var searchResultsList = document.getElementById("searchResults");
        searchResultsList.removeChildren();
        unhighlightAllEvents();
        return;
    }
    query = "%" + query + "%";
        
    CalendarDatabase.queryEventsInDB(query);
}

// Online status ----------------------------------------------------

function isOnline()
{
    return navigator.onLine;
}

function displayOnlineStatus()
{
    var statusIcon = document.getElementById("onlineStatusIcon");
    if (isOnline())
        statusIcon.removeStyleClass("offline");
    else {
        statusIcon.addStyleClass("offline");
        networkDropSound.play();
    }
}

// Map revelation ---------------------------------------------------

function hideMap() 
{
    document.getElementById("eventDisclosureArrow").removeStyleClass("expanded");
    document.getElementById("map").removeStyleClass("show");
    document.getElementById("details").removeStyleClass("showingMap");
}

function showMap() 
{
    document.getElementById("eventDisclosureArrow").addStyleClass("expanded");
    document.getElementById("map").addStyleClass("show");
    document.getElementById("details").addStyleClass("showingMap");
}

function toggleArrow() 
{
    var newMapShowState = !document.getElementById("map").hasStyleClass("show");
    if (newMapShowState)
        showMap();
    else
        hideMap();
}

function hideMapDisclosureArrow() 
{
    document.getElementById("eventDisclosureArrow").removeStyleClass("show");
    document.getElementById("map").removeChildren();
    hideMap();
}

function showMapDisclosureArrow() 
{
    document.getElementById("eventDisclosureArrow").addStyleClass("show");
}

var locationImage;

function mapImageReceived(image) 
{
    if (!image)
        return;
    image.addStyleClass("mapImage");
    document.getElementById("map").appendChild(image);
    showMapDisclosureArrow();
}

function requestMapImage() 
{
    if (!isOnline())
        return;
    if (!locationImage)
        locationImage = new LocationImage();
    var address = document.getElementById("eventLocation").innerText;
    locationImage.requestLocationImage(address, mapImageReceived);
}

// Search area ------------------------------------------------------

var dividerBarDragOffset = 0;
var dividerBarHeight = 24;

function initSearchArea() 
{
    document.getElementById("dividerBar").addEventListener("mousedown", startDividerBarDragging, true);
}

function startDividerBarDragging(event) 
{
    var dividerBarElement = document.getElementById("dividerBar");
    if (event.target !== dividerBarElement)
        return;

    document.addEventListener("mousemove", dividerBarDragging, true);
    document.addEventListener("mouseup", endDividerBarDragging, true);
    
    document.body.style.cursor = "row-resize";
    
    dividerBarDragOffset = event.pageY - document.getElementById("searchArea").totalOffsetTop;
    stopEvent(event);
}

function dividerBarDragging(event) 
{
    var dividerBarElement = document.getElementById("dividerBar");
    var searchAreaElement = document.getElementById("searchArea");
    var sidePanelHeight = document.getElementById("sidePanel").offsetHeight;
    var calendarListElement = document.getElementById("calendarList");
    var calendarListBottom = calendarListElement.totalOffsetTop + calendarListElement.offsetHeight;
    
    var dividerTop = event.pageY + dividerBarDragOffset;
    dividerTop = Number.constrain(dividerTop, calendarListBottom, sidePanelHeight - dividerBarHeight);
    var searchAreaTop = dividerTop + dividerBarHeight;
    
    dividerBarElement.style.top = dividerTop + "px";
    searchAreaElement.style.top = searchAreaTop + "px";

    stopEvent(event);
}

function endDividerBarDragging(event) 
{
    document.removeEventListener("mousemove", dividerBarDragging, true);
    document.removeEventListener("mouseup", endDividerBarDragging, true);

    document.body.style.removeProperty("cursor");
    dividerBarDragOffset = 0;
    stopEvent(event);
}

// LocalStorage access ---------------------------------------------

var CalendarState = {
    currentMonth: function() 
    {
        var month = new Date();
        // Retrieve the month on display saved from last time this app is launched.
        if (localStorage.monthOnDisplay)
            month.setTime(Date.parse(localStorage.monthOnDisplay));
        // First time we load this page - just use the current month.
        month.setDate(1);
        month.setHours(0, 0, 0, 0);
        return month;
    },

    setCurrentMonth: function(monthString) 
    {
        localStorage.monthOnDisplay = monthString;
    },

    toCalendarKey: function(calendarType) 
    {
        return calendarType + "CalendarChecked";
    },

    calendarChecked: function(calendarType) 
    {
        var value = localStorage.getItem(CalendarState.toCalendarKey(calendarType));
        if (!value)
            return true;
        return (value == "yes") ? true : false;
    },

    setCalendarChecked: function(calendarType, checked) 
    {
        localStorage.setItem(CalendarState.toCalendarKey(calendarType), checked ? "yes" : "no");
    },
}

// Database access -------------------------------------------------

var CalendarDatabase = {
    // The Events database object
    db: null,
    
    // Flag that tracks if we have opened the database for the very first time
    dbOpened: false,

    // REVIEW: can probably have a method that takes in a SQL statement, the arguments to the statement, optional callback and error
    // callback methods and execute the transaction on the database, and then have other methods just call that one method.
    // But we'll spell out the steps to make the database transaction in each method here for demonstration purposes.

    openWebKitCalendarEvents: function() 
    {
        // We have already made sure the WebKitCalendarEvents table has been created. Just load events directly.
        if (CalendarDatabase.dbOpened) {
            CalendarDatabase.loadEventsFromDB();
            return;
        }
        CalendarDatabase.dbOpened = true;
        
        // Query for opening WebKitCalendarEvents table
        var openTableStatement = "CREATE TABLE IF NOT EXISTS WebKitCalendarEvents (id REAL UNIQUE, eventTitle TEXT, eventLocation TEXT, startTime REAL, endTime REAL, eventCalendar TEXT, eventDetails TEXT)";
        
        // SQLStatementCallback - gets called after the table is created
        function sqlStatementCallback(result) { CalendarDatabase.loadEventsFromDB(); };

        // SQLStatementErrorCallback - gets called if there's an error opening the table
        function sqlStatementErrorCallback(tx, err) { alert("Error opening WebKitCalendarEvents: " + err.message); };

        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(openTableStatement, [], sqlStatementCallback, sqlStatementErrorCallback); };

        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    open: function() 
    {
        try {
            if (!window.openDatabase) {
                alert("Couldn't open the database.  Please try with a WebKit nightly with the database feature enabled.");
                return;
            }
            CalendarDatabase.db = openDatabase("Events", "1.0", "Events Database", 1000000);
            if (!CalendarDatabase.db)
                alert("Failed to open the database on disk.");
        } catch(err) { }
    },

    loadEventsFromDB: function() 
    {
        var self = this;

        // SQL query to retrieve all the events for this month
        var eventsQuery = "SELECT id, eventTitle, eventLocation, startTime, endTime, eventCalendar, eventDetails FROM WebKitCalendarEvents WHERE (startTime BETWEEN ? and ?)";
        
        // Arguments to the SQL query above
        var sqlArguments = [calendarStartTime, calendarEndTime];
        
        // SQLStatementCallback to process the query result
        function sqlStatementCallback(tx, result) { self.processLoadedEvents(result.rows); };
        
        // SQLStatementErrorCallback that handles any error from the query
        function sqlStatementErrorCallback(tx, error) { alert("Failed to retrieve events from database - " + error.message); };
        
        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(eventsQuery, sqlArguments, sqlStatementCallback, sqlStatementErrorCallback); };
        
        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    saveAsNewEventToDB: function(calendarEvent) 
    {
        // SQL statement to insert new event into the database table
        var insertEventStatement = "INSERT INTO WebKitCalendarEvents(id, eventTitle, eventLocation, startTime, endTime, eventCalendar, eventDetails) VALUES (?, ?, ?, ?, ?, ?, ?)";
        
        // Arguments to the SQL statement above
        var sqlArguments = [calendarEvent.id, calendarEvent.title, calendarEvent.location, calendarEvent.from.getTime(), calendarEvent.to.getTime(), calendarEvent.calendar, calendarEvent.details];
        
        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(insertEventStatement, sqlArguments); };

        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    saveEventToDB: function(calendarEvent) {
        // SQL statement to update an event in the database table
        var updateEventStatement = "UPDATE WebKitCalendarEvents SET eventTitle = ?, eventLocation = ?, startTime = ?, endTime = ?, eventCalendar = ?, eventDetails = ? WHERE id = ?";
        
        // Arguments to the SQL statement above
        var sqlArguments = [calendarEvent.title, calendarEvent.location, calendarEvent.from.getTime(), calendarEvent.to.getTime(), calendarEvent.calendar, calendarEvent.details, calendarEvent.id];
        
        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(updateEventStatement, sqlArguments); };

        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    deleteEventFromDB: function(calendarEvent) 
    {
        // SQL statement to delete an event from the database table
        var deleteEventStatement = "DELETE FROM WebKitCalendarEvents WHERE id = ?";
        
        // Arguments to the SQL statement above
        var sqlArguments = [calendarEvent.id];

        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(deleteEventStatement, sqlArguments); };
        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    queryEventsInDB: function(query) 
    {
        var self = this;

        // SQL query to search for events with keyword
        var searchEventQuery = "SELECT id, eventTitle, eventLocation, startTime, endTime, eventCalendar, eventDetails FROM WebKitCalendarEvents WHERE eventTitle LIKE ? OR eventDetails LIKE ? OR eventLocation LIKE ?";
        
        // Arguments to the SQL query
        var sqlArguments = [query, query, query];
        
        // SQLStatementCallback that processes the query result
        function sqlStatementCallback(tx, result) { self.processQueryResults(result.rows); };
        
        // SQLStatementErrorCallback that reports any error from the query
        function sqlStatementErrorCallback(tx, error) { alert("Failed to retrieve events from database - " + error.message); };
        
        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(searchEventQuery, sqlArguments, sqlStatementCallback, sqlTransactionCallback); };
        
        CalendarDatabase.db.transaction(sqlTransactionCallback);    
    },

    loadEventsFromDBForCalendarType: function(calendarType) 
    {
        var self = this;

        // SQL query to retrieve all the events for this month
        var eventsQuery = "SELECT id, eventTitle, eventLocation, startTime, endTime, eventCalendar, eventDetails FROM WebKitCalendarEvents WHERE (startTime BETWEEN ? and ?) AND eventCalendar = ?";
        
        // Arguments to the SQL query above
        var sqlArguments = [calendarStartTime, calendarEndTime, calendarType];
        
        // SQLStatementCallback to process the query result
        function sqlStatementCallback(tx, result) { self.processLoadedEvents(result.rows); };
        
        // SQLStatementErrorCallback that handles any error from the query
        function sqlStatementErrorCallback(tx, error) { alert("Failed to retrieve events from database - " + error.message); };
        
        // SQLTransactionCallback
        function sqlTransactionCallback(tx) { tx.executeSql(eventsQuery, sqlArguments, sqlStatementCallback, sqlStatementErrorCallback); };
        
        CalendarDatabase.db.transaction(sqlTransactionCallback);
    },

    processLoadedEvents: function(rows)
    {
        for (var i = 0; i < rows.length; i++) {
            var row = rows.item(i);
            var dayDate = Date.dayDateFromTime(row["startTime"]);
            var dayObj = dateToDayObjectMap[dayDate.getTime()];
            if (!dayObj)
                continue;

            dayObj.insertEvent(CalendarEvent.calendarEventFromResultRow(row, false));

            // Keep track of the highest id seen.
            if (row["id"] > highestID)
                highestID = row["id"];
        }
    },

    processQueryResults: function(rows)
    {
        var searchResultsList = document.getElementById("searchResults");
        searchResultsList.removeChildren();
        unhighlightAllEvents();
        for (var i = 0; i < rows.length; i++) {
            var row = rows.item(i);
            var calendarEvent = CalendarEvent.calendarEventFromResultRow(row, true);
            highlightEventInCalendar(calendarEvent);
            searchResultsList.appendChild(calendarEvent.searchResultAsListItem());
        }
    }
}

// Open Database!
CalendarDatabase.open();

function highlightEventInCalendar(calendarEvent)
{
    var itemsToHighlight = document.querySelectorAll("ul.contents li." + calendarEvent.calendar);
    for (var i = 0; i < itemsToHighlight.length; ++i) {
        var listItem = itemsToHighlight[i];
        if (listItem.innerText !== calendarEvent.title)
            continue;
        if (listItem.hasStyleClass("highlighted"))
            continue;
        listItem.addStyleClass("highlighted");
        break;
    }
}

function unhighlightAllEvents()
{
    var highlightedItems = document.querySelectorAll("ul.contents li.highlighted");
    for (var i = 0; i < highlightedItems.length; ++i)
        highlightedItems[i].removeStyleClass("highlighted");
}

// Day object -------------------------------------------------------

function dayObjectFromElement(element) 
{
    var parent = element;
    while (parent && !parent.dayObject)
        parent = parent.parentNode;
    return parent ? parent.dayObject : null;
}

function Day(date) 
{
    this.date = new Date(date);
    this.divNode = null;
    this.contentsListNode = null;
    this.eventsArray = null;
}

Day.prototype.attach = function(parent) 
{
    /* The HTML of each day looks like this:
        <div class="day box">
            <div class="date">1</div>
            <ul class="contents">
                <li>Event 1</li>
                <li>Event 2</li>
            </ul>
        </div>
    */

    if (this.divNode)
        throw("We have already created html elements for this day!");
    this.divNode = document.createElement("div");
    this.divNode.dayObject = this;
    this.divNode.addStyleClass("day");
    this.divNode.addStyleClass("box");
    if (this.date.getMonth() != monthOnDisplay.getMonth())
        this.divNode.addStyleClass("notThisMonth");
    if (this.date.isToday())
        this.divNode.addStyleClass("today");
    
    var dateDiv = document.createElement("div");
    dateDiv.addStyleClass("date");
    dateDiv.innerText = this.date.getDate();
    this.divNode.appendChild(dateDiv);
    
    this.contentsListNode = document.createElement("ul");
    this.contentsListNode.addStyleClass("contents");
    this.contentsListNode.addEventListener("dblclick", Day.newEvent, false);
    this.divNode.appendChild(this.contentsListNode);
    
    parent.appendChild(this.divNode);
}

Day.newEvent = function(event) 
{
    var element = event.target;
    var dayObj = dayObjectFromElement(element);
    if (!dayObj || dayObj.contentsListNode != element)
        return;
        
    var calendarEvent = new CalendarEvent(dayObj.date, dayObj, selectedCalendarType, false);
    calendarEvent.title = "New Event";
    calendarEvent.from = dayObj.defaultEventStartTime();
    var endTime = new Date(calendarEvent.from);
    endTime.setHours(endTime.getHours() + 1);
    calendarEvent.to = endTime;
    dayObj.insertEvent(calendarEvent);
    CalendarDatabase.saveAsNewEventToDB(calendarEvent);
    selectedCalendarEvent = calendarEvent;
    calendarEvent.show();
    
    stopEvent(event);
}

Day.prototype.insertEvent = function(calendarEvent) 
{
    if (!this.eventsArray)
        this.eventsArray = new Array();
    // Remove the event from array if it's already there.
    var index = this.eventsArray.indexOf(calendarEvent);
    if (index >= 0)
        this.eventsArray.splice(index, 1);
    calendarEvent.detach();
    // Don't display this in the calendar if the calendar is unchecked
    if (!isCalendarTypeVisible(calendarEvent.calendar))
        return;
    // We want to insert the calendarEvent in order of start time
    for (index = 0; index < this.eventsArray.length; index++) {
        if (this.eventsArray[index].from > calendarEvent.from)
            break;
    }
    this.eventsArray.splice(index, 0, calendarEvent);
    calendarEvent.attach();
}

Day.prototype.deleteEvent = function(calendarEvent) 
{
    CalendarDatabase.deleteEventFromDB(calendarEvent);
    this.hideEvent(calendarEvent);
}

Day.prototype.hideEvent = function(calendarEvent)
{
    calendarEvent.detach();
    selectedCalendarEvent = null;
    if (!this.eventsArray)
        return;
    var index = this.eventsArray.indexOf(calendarEvent);
    if (index >= 0)
        this.eventsArray.splice(index, 1);
}

Day.prototype.hideEventsOfCalendarType = function(calendarType) 
{
    if (!this.eventsArray)
        return;
    var i = 0;
    while (i < this.eventsArray.length) {
        if (this.eventsArray[i].calendar == calendarType)
            this.hideEvent(this.eventsArray[i]);
        else
            i++;
    }
}

Day.prototype.defaultEventStartTime = function() 
{
    var startTime;
    if (!this.eventsArray || !this.eventsArray.length) {
        startTime = new Date(this.date);
        startTime.setHours(9, 0, 0, 0);     // Default: events start at 9am!
        return startTime;
    }
    var lastEvent = this.eventsArray[this.eventsArray.length-1];
    startTime = new Date(lastEvent.to);
    startTime.roundToHour();
    return startTime;
}

// CalendarEvent object -------------------------------------------------------

function calendarEventFromElement(element) 
{
    var parent = element;
    while (parent) {
        if (parent.calendarEvent)
            return parent.calendarEvent;
        parent = parent.parentNode;
    }
    return null;
}

function CalendarEvent(date, day, calendar, fromSearch) 
{
    this.date = date;
    this.day = day;
    this.fromSearch = fromSearch;
    this.id = ++highestID;
    this.title = "";
    this.location = "";
    this.from = null;
    this.to = null;
    this.calendar = calendar;
    this.details = "";
    this.listItemNode = null;
}

CalendarEvent.prototype.attach = function() 
{
    var parentNode = this.day.contentsListNode;
    if (!parentNode)
        throw("Must have created day box's html hierarchy before adding calendar events.");
    if (this.listItemNode)
        parentNode.removeChild(this.listItemNode);
    this.listItemNode = document.createElement("li");
    this.listItemNode.calendarEvent = this;
    this.listItemNode.addStyleClass(this.calendar);
    this.listItemNode.innerText = this.title;
    this.listItemNode.addEventListener("click", CalendarEvent.eventSelected, false);
    var index = this.day.eventsArray.indexOf(this);
    if (index < 0)
        throw("Cannot attach if CalendarEvent does not belong to a Day object.");
    var adjacentNode = null;
    if (index < this.day.contentsListNode.childNodes.length)
        adjacentNode = this.day.contentsListNode.childNodes[index];
    this.day.contentsListNode.insertBefore(this.listItemNode, adjacentNode);
}

CalendarEvent.prototype.detach = function() 
{
    var parentNode = this.day.contentsListNode;
    if (parentNode && this.listItemNode)
        parentNode.removeChild(this.listItemNode);
    this.listItemNode = null;
}

CalendarEvent.eventSelected = function(event) 
{
    var calendarEvent = calendarEventFromElement(event.target);
    if (!calendarEvent)
        return;
    calendarEvent.selected();
    stopEvent(event);
}

CalendarEvent.prototype.selected = function() 
{
    if (selectedCalendarEvent == this) {
        selectedCalendarEvent.show();
        return;
    }
    if (selectedCalendarEvent)
        selectedCalendarEvent.listItemNode.removeStyleClass("selected");
    selectedCalendarEvent = this;
    if (selectedCalendarEvent)
        selectedCalendarEvent.listItemNode.addStyleClass("selected");
}

function minutesString(minutes) 
{
    if (minutes < 10)
        return "0" + minutes;
    return minutes.toString();
}

CalendarEvent.prototype.show = function() 
{
    // Update the event details
    document.getElementById("eventTitle").innerText = this.title;
    document.getElementById("eventLocation").innerText = this.location;
    document.getElementById("eventFromDate").innerText = this.date.toLocaleDateString();
    document.getElementById("eventFromHours").innerText = this.from.getHours();
    document.getElementById("eventFromMinutes").innerText = minutesString(this.from.getMinutes());
    document.getElementById("eventToDate").innerText = this.date.toLocaleDateString();
    document.getElementById("eventToHours").innerText = this.to.getHours();
    document.getElementById("eventToMinutes").innerText = minutesString(this.to.getMinutes());
    document.getElementById("eventCalendarType").value = this.calendar;
    document.getElementById("eventDetails").innerText = this.details;
    
    // Reset the map
    requestMapImage();

    // Fade out the gridView
    document.getElementById("gridView").addStyleClass("inactive");
        
    // Show event details
    document.getElementById("eventOverlay").addStyleClass("show");
}

CalendarEvent.prototype.detailsUpdated = function() 
{
    // FIXME: error checking!!
    this.title = document.getElementById("eventTitle").innerText;
    this.location = document.getElementById("eventLocation").innerText;
    this.from.setHours(document.getElementById("eventFromHours").innerText);
    this.from.setMinutes(document.getElementById("eventFromMinutes").innerText);
    this.to.setHours(document.getElementById("eventToHours").innerText);
    this.to.setMinutes(document.getElementById("eventToMinutes").innerText);
    this.calendar = document.getElementById("eventCalendarType").value;
    this.details = document.getElementById("eventDetails").innerText;

    CalendarDatabase.saveEventToDB(this);
    if (!this.fromSearch && this.day)
        this.day.insertEvent(this);
}

CalendarEvent.prototype.toString = function() 
{
    return "CalendarEvent: " + this.title + " starting on " + this.from.toString();
}

CalendarEvent.prototype.searchResultAsListItem = function() 
{
    var listItem = document.createElement("li");
    var titleText = document.createTextNode(this.title);
    var locationText = this.location.length > 0 ? document.createTextNode(this.location) : null;
    var startTimeText = document.createTextNode(this.from.toLocaleString());
    listItem.appendChild(titleText);
    listItem.appendChild(document.createElement("br"));
    if (locationText) {
        listItem.appendChild(locationText);
        listItem.appendChild(document.createElement("br"));
    }
    listItem.appendChild(startTimeText);
    listItem.calendarEvent = this;
    listItem.addStyleClass(this.calendar);
    listItem.addEventListener("click", CalendarEvent.eventSelected, false);
    this.listItemNode = listItem;
    return listItem;
}

CalendarEvent.calendarEventFromResultRow = function(row, fromSearch) 
{
    var dayDate = Date.dayDateFromTime(row["startTime"]);
    var dayObj = dateToDayObjectMap[dayDate.getTime()];
    var calendarEvent = new CalendarEvent(dayDate, dayObj, row["eventCalendar"], fromSearch);
    calendarEvent.id = row["id"];
    calendarEvent.title = row["eventTitle"];
    calendarEvent.location = row["eventLocation"];
    calendarEvent.from = new Date();
    calendarEvent.from.setTime(row["startTime"]);
    calendarEvent.to = new Date();
    calendarEvent.to.setTime(row["endTime"]);
    calendarEvent.details = row["eventDetails"];
    return calendarEvent;
}

// Miscellaneous methods ------------------------------------------------

function stopEvent(event) 
{
    event.preventDefault();
    event.stopPropagation();
}
