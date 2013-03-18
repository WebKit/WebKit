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

function highlightedMonthButton() {
    skipAnimation();
    var year = popupWindow.global.picker.monthPopupView.yearListView.selectedRow + 1;
    return Array.prototype.map.call(popupWindow.document.querySelectorAll(".month-button.highlighted"), function(element) {
        return new popupWindow.Month(year, Number(element.dataset.month)).toString();
    }).sort().join();
}

function hoverOverMonthPopupButton() {
    skipAnimation();
    var buttonElement = popupWindow.global.picker.calendarHeaderView.monthPopupButton.element;
    var offset = cumulativeOffset(buttonElement);
    eventSender.mouseMoveTo(offset[0] + buttonElement.offsetWidth / 2, offset[1] + buttonElement.offsetHeight / 2);
}

function clickMonthPopupButton() {
    hoverOverMonthPopupButton();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function clickYearListCell(year) {
    skipAnimation();
    var row = year - 1;
    var rowCell = popupWindow.global.picker.monthPopupView.yearListView.cellAtRow(row);

    var rowScrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollOffsetForRow(row);
    var scrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var scrollViewOffset = cumulativeOffset(popupWindow.global.picker.monthPopupView.yearListView.scrollView.element);
    var rowCellCenterX = scrollViewOffset[0] + rowCell.element.offsetWidth / 2;
    var rowCellCenterY = scrollViewOffset[1] + rowOffsetFromViewportTop + rowCell.element.offsetHeight / 2;
    eventSender.mouseMoveTo(rowCellCenterX, rowCellCenterY);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function hoverOverMonthButton(year, month) {
    skipAnimation();
    var row = year - 1;
    var rowCell = popupWindow.global.picker.monthPopupView.yearListView.cellAtRow(row);

    var rowScrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollOffsetForRow(row);
    var scrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var button = popupWindow.global.picker.monthPopupView.yearListView.buttonForMonth(new popupWindow.Month(year, month));
    var buttonOffset = cumulativeOffset(button);
    var rowCellOffset = cumulativeOffset(rowCell.element);
    var buttonOffsetRelativeToRowCell = [buttonOffset[0] - rowCellOffset[0], buttonOffset[1] - rowCellOffset[1]];

    var scrollViewOffset = cumulativeOffset(popupWindow.global.picker.monthPopupView.yearListView.scrollView.element);
    var buttonCenterX = scrollViewOffset[0] + buttonOffsetRelativeToRowCell[0] + button.offsetWidth / 2;
    var buttonCenterY = scrollViewOffset[1] + buttonOffsetRelativeToRowCell[1] + rowOffsetFromViewportTop + button.offsetHeight / 2;
    eventSender.mouseMoveTo(buttonCenterX, buttonCenterY);
}

function clickMonthButton(year, month) {
    hoverOverMonthButton(year, month);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

var lastYearListViewScrollOffset = NaN;
function checkYearListViewScrollOffset() {
    skipAnimation();
    var scrollOffset = popupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var change = lastYearListViewScrollOffset - scrollOffset;
    lastYearListViewScrollOffset = scrollOffset;
    return change;
}

function isCalendarTableScrollingWithAnimation() {
    var animator = popupWindow.global.picker.calendarTableView.scrollView.scrollAnimator();
    if (!animator)
        return false;
    return animator.isRunning();
}
