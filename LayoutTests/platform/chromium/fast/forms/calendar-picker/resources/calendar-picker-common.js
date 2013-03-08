function currentMonth() {
    return popupWindow.global.picker.currentMonth().toString();
}

function availableDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell:not(.disabled):not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".day-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function availableWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".week-number-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedValue() {
    var highlight = popupWindow.global.picker.highlight();
    if (highlight)
        return highlight.toString();
    return null;
}

function selectedValue() {
    var selection = popupWindow.global.picker.selection();
    if (selection)
        return selection.toString();
    return null;
}

function skipAnimation() {
    popupWindow.AnimationManager.shared._animationFrameCallback(Infinity);
}

function hoverOverDayCellAt(column, row) {
    skipAnimation();
    var offset = cumulativeOffset(popupWindow.global.picker.calendarTableView.element);
    var x = offset[0];
    var y = offset[1];
    if (popupWindow.global.picker.calendarTableView.hasWeekNumberColumn)
        x += popupWindow.WeekNumberCell.Width;
    x += (column + 0.5) * popupWindow.DayCell.Width;
    y += (row + 0.5) * popupWindow.DayCell.Height + popupWindow.CalendarTableHeaderView.Height;
    eventSender.mouseMoveTo(x, y);
};

function clickDayCellAt(column, row) {
    hoverOverDayCellAt(column, row);
    eventSender.mouseDown();
    eventSender.mouseUp();
}
