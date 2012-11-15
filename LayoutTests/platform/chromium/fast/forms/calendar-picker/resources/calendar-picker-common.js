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

function selectedMonth() {
    var selected = popupWindow.document.querySelectorAll(".day.day-selected");
    if (selected.length === 0)
        return null;
    return selected[0].dataset.monthValue;
}

function selectedWeek() {
    var selected = popupWindow.document.querySelectorAll(".day.day-selected");
    if (selected.length === 0)
        return null;
    return selected[0].dataset.weekValue;
}
