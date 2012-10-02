window.jsTestIsAsync = true;
if (window.internals)
    internals.settings.setEnableMockPagePopup(true);

var popupWindow = null;

function currentMonth() {
    var element = popupWindow.document.querySelector(".selected-month-year");
    if (!element)
        return null;
    return element.dataset.value;
}

function availableDatesInCurrentMonth() {
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day.available:not(.not-this-month)"), function(element) {
        return element.dataset.submitValue;
    }).sort();
}

function selectedDate() {
    var selected = selectedDates();
    if (selected.length > 1)
        testFailed("selectedDate expects single selected date. Found " + selected.length);
    return selected[0];
}

function selectedDates() {
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day.day-selected"), function(element) {
        return element.dataset.submitValue;
    }).sort();
}

function openPicker(input) {
    sendKey(input, "Down");
    popupWindow = document.getElementById('mock-page-popup').contentWindow;
}

function sendKey(input, keyName) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName);
    input.dispatchEvent(event);
}
