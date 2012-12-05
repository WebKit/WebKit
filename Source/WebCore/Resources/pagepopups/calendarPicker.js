"use strict";
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// FIXME:
//  - Touch event

/**
 * CSS class names.
 *
 * @enum {string}
 */
var ClassNames = {
    Available: "available",
    CancelButton: "cancel-button",
    ClearButton: "clear-button",
    Day: "day",
    DayLabel: "day-label",
    DayLabelContainer: "day-label-container",
    DaysArea: "days-area",
    DaysAreaContainer: "days-area-container",
    Monday: "monday",
    MonthMode: "month-mode",
    MonthSelector: "month-selector",
    MonthSelectorBox: "month-selector-box",
    MonthSelectorPopup: "month-selector-popup",
    MonthSelectorPopupContents: "month-selector-popup-contents",
    MonthSelectorPopupEntry: "month-selector-popup-entry",
    MonthSelectorWall: "month-selector-wall",
    NoFocusRing: "no-focus-ring",
    NotThisMonth: "not-this-month",
    Selected: "day-selected",
    SelectedMonthYear: "selected-month-year",
    Sunday: "sunday",
    TodayButton: "today-button",
    TodayClearArea: "today-clear-area",
    Unavailable: "unavailable",
    WeekContainer: "week-container",
    WeekColumn: "week-column",
    WeekMode: "week-mode",
    YearMonthArea: "year-month-area",
    YearMonthButton: "year-month-button",
    YearMonthButtonLeft: "year-month-button-left",
    YearMonthButtonRight: "year-month-button-right",
    YearMonthUpper: "year-month-upper"
};

/**
 * @type {Object}
 */
var global = {
    argumentsReceived: false,
    params: null,
    picker: null
};

// ----------------------------------------------------------------
// Utility functions

/**
 * @return {!string} lowercase locale name. e.g. "en-us"
 */
function getLocale() {
    return (global.params.locale || "en-us").toLowerCase();
}

/**
 * @return {!string} lowercase language code. e.g. "en"
 */
function getLanguage() {
    var locale = getLocale();
    var result = locale.match(/^([a-z]+)/);
    if (!result)
        return "en";
    return result[1];
}

/**
 * @param {!number} number
 * @return {!string}
 */
function localizeNumber(number) {
    return window.pagePopupController.localizeNumberString(number);
}

/**
 * @const
 * @type {number}
 */
var ImperialEraLimit = 2087;

/**
 * @param {!number} year
 * @param {!number} month
 * @return {!string}
 */
function formatJapaneseImperialEra(year, month) {
    // We don't show an imperial era if it is greater than 99 becase of space
    // limitation.
    if (year > ImperialEraLimit)
        return "";
    if (year > 1989)
        return "(平成" + localizeNumber(year - 1988) + "年)";
    if (year == 1989)
        return "(平成元年)";
    if (year >= 1927)
        return "(昭和" + localizeNumber(year - 1925) + "年)";
    if (year > 1912)
        return "(大正" + localizeNumber(year - 1911) + "年)";
    if (year == 1912 && month >= 7)
        return "(大正元年)";
    if (year > 1868)
        return "(明治" + localizeNumber(year - 1867) + "年)";
    if (year == 1868)
        return "(明治元年)";
    return "";
}

/**
 * @return {!string}
 */
Month.prototype.toLocaleString = function() {
    if (isNaN(this.year) || isNaN(this.year))
        return "Invalid Month";
    if (getLanguage() == "ja")
        return "" + this.year + "年" + formatJapaneseImperialEra(this.year, this.month) + " " + (this.month + 1) + "月";
    return window.pagePopupController.formatMonth(this.year, this.month);
};

function createUTCDate(year, month, date) {
    var newDate = new Date(0);
    newDate.setUTCFullYear(year);
    newDate.setUTCMonth(month);
    newDate.setUTCDate(date);
    return newDate;
};

/**
 * @param {string} dateString
 * @return {?Day|Week|Month}
 */
function parseDateString(dateString) {
    var month = Month.parse(dateString);
    if (month)
        return month;
    var week = Week.parse(dateString);
    if (week)
        return week;
    return Day.parse(dateString);
}

/**
 * @constructor
 * @param {!number|Day} valueOrDayOrYear
 * @param {!number=} month
 * @param {!number=} date
 */
function Day(valueOrDayOrYear, month, date) {
    var dateObject;
    if (arguments.length == 3)
        dateObject = createUTCDate(valueOrDayOrYear, month, date);
    else if (valueOrDayOrYear instanceof Day)
        dateObject = createUTCDate(valueOrDayOrYear.year, valueOrDayOrYear.month, valueOrDayOrYear.date);
    else
        dateObject = new Date(valueOrDayOrYear);
    this.year = dateObject.getUTCFullYear();    
    this.month = dateObject.getUTCMonth();
    this.date = dateObject.getUTCDate();
};

Day.ISOStringRegExp = /^(\d+)-(\d+)-(\d+)/;

/**
 * @param {!string} str
 * @return {?Month}
 */
Day.parse = function(str) {
    var match = Day.ISOStringRegExp.exec(str);
    if (!match)
        return null;
    var year = parseInt(match[1], 10);
    var month = parseInt(match[2], 10) - 1;
    var date = parseInt(match[3], 10);
    return new Day(year, month, date);
};

/**
 * @param {!Date} date
 * @return {!Day}
 */
Day.createFromDate = function(date) {
    return new Day(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate());
};

/**
 * @return {!Day}
 */
Day.createFromToday = function() {
    var now = new Date();
    return new Day(now.getFullYear(), now.getMonth(), now.getDate());
};

/**
 * @param {!Day} other
 * @return {!bool}
 */
Day.prototype.equals = function(other) {
    return this.year === other.year && this.month === other.month && this.date === other.date;
};

/**
 * @return {!Day}
 */
Day.prototype.previous = function() {
    return new Day(this.year, this.month, this.date - 1);
};

/**
 * @return {!Day}
 */
Day.prototype.next = function() {
    return new Day(this.year, this.month, this.date + 1);
};

/**
 * @return {!Date}
 */
Day.prototype.startDate = function() {
    return createUTCDate(this.year, this.month, this.date);
};

/**
 * @return {!Date}
 */
Day.prototype.endDate = function() {
    return createUTCDate(this.year, this.month, this.date + 1);
};

/**
 * @return {!number}
 */
Day.prototype.valueOf = function() {
    return this.startDate().getTime();
};

/**
 * @return {!string}
 */
Day.prototype.toString = function() {
    var yearString = String(this.year);
    if (yearString.length < 4)
        yearString = ("000" + yearString).substr(-4, 4);
    return yearString + "-" + ("0" + (this.month + 1)).substr(-2, 2) + "-" + ("0" + this.date).substr(-2, 2);
};

// See WebCore/platform/DateComponents.h.
Day.Minimum = new Day(-62135596800000.0);
Day.Maximum = new Day(8640000000000000.0);
// See WebCore/html/DayInputType.cpp.
Day.DefaultStep = 86400000;
Day.DefaultStepBase = 0;

/**
 * @constructor
 * @param {!number|Week} valueOrWeekOrYear
 * @param {!number=} week
 */
function Week(valueOrWeekOrYear, week) {
    if (arguments.length === 2) {
        this.year = valueOrWeekOrYear;
        this.week = week;
        // Number of years per year is either 52 or 53.
        if (this.week < 1 || (this.week > 52 && this.week > Week.numberOfWeeksInYear(this.year))) {
            var normalizedWeek = Week.createFromDate(this.startDate());
            this.year = normalizedWeek.year;
            this.week = normalizedWeek.week;
        }
    } else if (valueOrWeekOrYear instanceof Week) {
        this.year = valueOrWeekOrYear.year;
        this.week = valueOrWeekOrYear.week;
    } else {
        var week = Week.createFromDate(new Date(valueOrWeekOrYear));
        this.year = week.year;
        this.week = week.week;
    }
}

Week.MillisecondsPerWeek = 7 * 24 * 60 * 60 * 1000;
Week.ISOStringRegExp = /^(\d+)-[wW](\d+)$/;
// See WebCore/platform/DateComponents.h.
Week.Minimum = new Week(1, 1);
Week.Maximum = new Week(275760, 37);
// See WebCore/html/WeekInputType.cpp.
Week.DefaultStep = 604800000;
Week.DefaultStepBase = -259200000;

/**
 * @param {!string} str
 * @return {?Week}
 */
Week.parse = function(str) {
    var match = Week.ISOStringRegExp.exec(str);
    if (!match)
        return null;
    var year = parseInt(match[1], 10);
    var week = parseInt(match[2], 10);
    return new Week(year, week);
};

/**
 * @param {!Date} date
 * @return {!Week}
 */
Week.createFromDate = function(date) {
    var year = date.getUTCFullYear();
    if (year <= Week.Maximum.year && Week.weekOneStartDateForYear(year + 1).getTime() <= date.getTime())
        year++;
    else if (year > 1 && Week.weekOneStartDateForYear(year).getTime() > date.getTime())
        year--;
    var week = 1 + Week._numberOfWeeksSinceDate(Week.weekOneStartDateForYear(year), date);
    return new Week(year, week);
};

/**
 * @return {!Week}
 */
Week.createFromToday = function() {
    var now = new Date();
    return Week.createFromDate(createUTCDate(now.getFullYear(), now.getMonth(), now.getDate()));
};

/**
 * @param {!number} year
 * @return {!Date}
 */
Week.weekOneStartDateForYear = function(year) {
    if (year < 1)
        return createUTCDate(1, 0, 1);
    // The week containing January 4th is week one.
    var yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
    return createUTCDate(year, 0, 4 - (yearStartDay + 6) % 7);
};

/**
 * @param {!number} year
 * @return {!number}
 */
Week.numberOfWeeksInYear = function(year) {
    if (year < 1 || year > Week.Maximum.year)
        return 0;
    else if (year === Week.Maximum.year)
        return Week.Maximum.week;
    return Week._numberOfWeeksSinceDate(Week.weekOneStartDateForYear(year), Week.weekOneStartDateForYear(year + 1));
};

/**
 * @param {!Date} baseDate
 * @param {!Date} date
 * @return {!number}
 */
Week._numberOfWeeksSinceDate = function(baseDate, date) {
    return Math.floor((date.getTime() - baseDate.getTime()) / Week.MillisecondsPerWeek);
};

/**
 * @param {!Week} other
 * @return {!bool}
 */
Week.prototype.equals = function(other) {
    return this.year === other.year && this.week === other.week;
};

/**
 * @return {!Week}
 */
Week.prototype.previous = function() {
    return new Week(this.year, this.week - 1);
};

/**
 * @return {!Week}
 */
Week.prototype.next = function() {
    return new Week(this.year, this.week + 1);
};

/**
 * @return {!Date}
 */
Week.prototype.startDate = function() {
    var weekStartDate = Week.weekOneStartDateForYear(this.year);
    weekStartDate.setUTCDate(weekStartDate.getUTCDate() + (this.week - 1) * 7);
    return weekStartDate;
};

/**
 * @return {!Date}
 */
Week.prototype.endDate = function() {
    if (this.equals(Week.Maximum))
        return Day.Maximum.startDate();
    return this.next().startDate();
};

/**
 * @return {!number}
 */
Week.prototype.valueOf = function() {
    return this.startDate().getTime() - createUTCDate(1970, 0, 1).getTime();
};

/**
 * @return {!string}
 */
Week.prototype.toString = function() {
    var yearString = String(this.year);
    if (yearString.length < 4)
        yearString = ("000" + yearString).substr(-4, 4);
    return yearString + "-W" + ("0" + this.week).substr(-2, 2);
};

/**
 * @constructor
 * @param {!number|Month} valueOrMonthOrYear
 * @param {!number=} month
 */
function Month(valueOrMonthOrYear, month) {
    if (arguments.length == 2) {
        this.year = valueOrMonthOrYear;
        this.month = month;
    } else if (valueOrMonthOrYear instanceof Month) {
        this.year = valueOrMonthOrYear.year;
        this.month = valueOrMonthOrYear.month;
    } else {
        this.year = 1970;
        this.month = valueOrMonthOrYear;
    }
    this.year = this.year + Math.floor(this.month / 12);
    this.month = this.month < 0 ? this.month % 12 + 12 : this.month % 12;
    if (this.year <= 0 || Month.Maximum < this) {
        this.year = NaN;
        this.month = NaN;
    }
};

Month.ISOStringRegExp = /^(\d+)-(\d+)$/;

// See WebCore/platform/DateComponents.h.
Month.Minimum = new Month(1, 0);
Month.Maximum = new Month(275760, 8);
// See WebCore/html/MonthInputType.cpp.
Month.DefaultStep = 1;
Month.DefaultStepBase = 0;

/**
 * @param {!string} str
 * @return {?Month}
 */
Month.parse = function(str) {
    var match = Month.ISOStringRegExp.exec(str);
    if (!match)
        return null;
    var year = parseInt(match[1], 10);
    var month = parseInt(match[2], 10) - 1;
    return new Month(year, month);
};

/**
 * @param {!Date} date
 * @return {!Month}
 */
Month.createFromDate = function(date) {
    return new Month(date.getUTCFullYear(), date.getUTCMonth());
};

/**
 * @return {!Month}
 */
Month.createFromToday = function() {
    var now = new Date();
    return new Month(now.getFullYear(), now.getMonth());
};

/**
 * @param {!Month} other
 * @return {!bool}
 */
Month.prototype.equals = function(other) {
    return this.year === other.year && this.month === other.month;
};

/**
 * @return {!Month}
 */
Month.prototype.previous = function() {
    return new Month(this.year, this.month - 1);
};

/**
 * @return {!Month}
 */
Month.prototype.next = function() {
    return new Month(this.year, this.month + 1);
};

/**
 * @return {!Date}
 */
Month.prototype.startDate = function() {
    return createUTCDate(this.year, this.month, 1);
};

/**
 * @return {!Date}
 */
Month.prototype.endDate = function() {
    if (this.equals(Month.Maximum))
        return Day.Maximum.startDate();
    return this.next().startDate();
};

/**
 * @return {!number}
 */
Month.prototype.valueOf = function() {
    return (this.year - 1970) * 12 + this.month;
};

/**
 * @return {!string}
 */
Month.prototype.toString = function() {
    var yearString = String(this.year);
    if (yearString.length < 4)
        yearString = ("000" + yearString).substr(-4, 4);
    return yearString + "-" + ("0" + (this.month + 1)).substr(-2, 2);
};

// ----------------------------------------------------------------
// Initialization

/**
 * @param {Event} event
 */
function handleMessage(event) {
    if (global.argumentsReceived)
        return;
    global.argumentsReceived = true;
    initialize(JSON.parse(event.data));
}

function handleArgumentsTimeout() {
    if (global.argumentsReceived)
        return;
    var args = {
        dayLabels : ["d1", "d2", "d3", "d4", "d5", "d6", "d7"],
        todayLabel : "Today",
        clearLabel : "Clear",
        cancelLabel : "Cancel",
        currentValue : "",
        weekStartDay : 0,
        step : CalendarPicker.DefaultStepScaleFactor,
        stepBase: CalendarPicker.DefaultStepBase
    };
    initialize(args);
}

/**
 * @param {!Object} config
 * @return {?string} An error message, or null if the argument has no errors.
 */
CalendarPicker.validateConfig = function(config) {
    if (!config.dayLabels)
        return "No dayLabels.";
    if (config.dayLabels.length != 7)
        return "dayLabels is not an array with 7 elements.";
    if (!config.clearLabel)
        return "No clearLabel.";
    if (!config.todayLabel)
        return "No todayLabel.";
    if (config.weekStartDay) {
        if (config.weekStartDay < 0 || config.weekStartDay > 6)
            return "Invalid weekStartDay: " + config.weekStartDay;
    }
    return null;
}

/**
 * @param {!Object} args
 */
function initialize(args) { 
    global.params = args;
    var errorString = CalendarPicker.validateConfig(args);
    if (args.suggestionValues)
        errorString = errorString || SuggestionPicker.validateConfig(args)
    if (errorString) {
        var main = $("main");
        main.textContent = "Internal error: " + errorString;
        resizeWindow(main.offsetWidth, main.offsetHeight);
    } else {
        if (global.params.suggestionValues && global.params.suggestionValues.length)
            openSuggestionPicker();
        else
            openCalendarPicker();
    }
}

function closePicker() {
    if (global.picker)
        global.picker.cleanup();
    var main = $("main");
    main.innerHTML = "";
    main.className = "";
};

function openSuggestionPicker() {
    closePicker();
    global.picker = new SuggestionPicker($("main"), global.params);
};

function openCalendarPicker() {
    closePicker();
    global.picker = new CalendarPicker($("main"), global.params);
};

/**
 * @constructor
 * @param {!Element} element
 * @param {!Object} config
 */
function CalendarPicker(element, config) {
    Picker.call(this, element, config);
    if (this._config.mode === "month") {
        this.selectionConstructor = Month;
        this._daysTable = new MonthPickerDaysTable(this);
        this._element.classList.add(ClassNames.MonthMode);
    } else if (this._config.mode === "week") {
        this.selectionConstructor = Week;
        this._daysTable = new WeekPickerDaysTable(this);
        this._element.classList.add(ClassNames.WeekMode);
    } else {
        this.selectionConstructor = Day;
        this._daysTable = new DaysTable(this);
    }
    this._element.classList.add("calendar-picker");
    this._element.classList.add("preparing");
    this.isPreparing = true;
    this._handleWindowResizeBound = this._handleWindowResize.bind(this);
    window.addEventListener("resize", this._handleWindowResizeBound, false);
    // We assume this._config.min/max are valid dates or months.
    var minimum = (typeof this._config.min !== "undefined") ? parseDateString(this._config.min) : this.selectionConstructor.Minimum;
    var maximum = (typeof this._config.max !== "undefined") ? parseDateString(this._config.max) : this.selectionConstructor.Maximum;
    this._minimumValue = minimum.valueOf();
    this._maximumValue = maximum.valueOf();
    this.step = (typeof this._config.step !== undefined) ? Number(this._config.step) : this.selectionConstructor.DefaultStep;
    this.stepBase = (typeof this._config.stepBase !== "undefined") ? Number(this._config.stepBase) : this.selectionConstructor.DefaultStepBase;
    this._minimumMonth = Month.createFromDate(minimum.startDate());
    this.maximumMonth = Month.createFromDate(maximum.startDate());
    this._currentMonth = new Month(NaN, NaN);
    this._yearMonthController = new YearMonthController(this);
    this._hadKeyEvent = false;
    this._layout();
    var initialSelection = parseDateString(this._config.currentValue);
    if (!initialSelection)
        initialSelection = this.selectionConstructor.createFromToday();
    if (initialSelection.valueOf() < this._minimumValue)
        initialSelection = new this.selectionConstructor(this._minimumValue);
    else if (initialSelection.valueOf() > this._maximumValue)
        initialSelection = new this.selectionConstructor(this._maximumValue);
    this.showMonth(Month.createFromDate(initialSelection.startDate()));
    this._daysTable.selectRangeAndShowEntireRange(initialSelection);
    this.fixWindowSize();
    this._handleBodyKeyDownBound = this._handleBodyKeyDown.bind(this);
    document.body.addEventListener("keydown", this._handleBodyKeyDownBound, false);
}
CalendarPicker.prototype = Object.create(Picker.prototype);

CalendarPicker.NavigationBehaviour = {
    None: 0,
    Animate: 1 << 0,
    KeepSelectionPosition: 1 << 1
};

CalendarPicker.prototype._handleWindowResize = function() {
    this._element.classList.remove("preparing");
    this.isPreparing = false;
};

CalendarPicker.prototype.cleanup = function() {
    document.body.removeEventListener("keydown", this._handleBodyKeyDownBound, false);
};

CalendarPicker.prototype._layout = function() {
    if (this._config.isLocaleRTL)
        this._element.classList.add("rtl");
    this._yearMonthController.attachTo(this._element);
    this._daysTable.attachTo(this._element);
    this._layoutButtons();
    // DaysTable will have focus but we don't want to show its focus ring until the first key event.
    this._element.classList.add(ClassNames.NoFocusRing);
};

CalendarPicker.prototype.handleToday = function() {
    var today = this.selectionConstructor.createFromToday();
    this._daysTable.selectRangeAndShowEntireRange(today);
    this.submitValue(today.toString());
};

CalendarPicker.prototype.handleClear = function() {
    this.submitValue("");
};

CalendarPicker.prototype.fixWindowSize = function() {
    var yearMonthRightElement = this._element.getElementsByClassName(ClassNames.YearMonthButtonRight)[0];
    var daysAreaElement = this._element.getElementsByClassName(ClassNames.DaysArea)[0];
    var headers = daysAreaElement.getElementsByClassName(ClassNames.DayLabel);
    var maxCellWidth = 0;
    for (var i = 1; i < headers.length; ++i) {
        if (maxCellWidth < headers[i].offsetWidth)
            maxCellWidth = headers[i].offsetWidth;
    }
    var weekColumnWidth = headers[0].offsetWidth;
    if (maxCellWidth > weekColumnWidth)
        weekColumnWidth = maxCellWidth;
    headers[0].style.width = weekColumnWidth + "px";
    var DaysAreaContainerBorder = 1;
    var yearMonthEnd;
    var daysAreaEnd;
    if (global.params.isLocaleRTL) {
        var startOffset = this._element.offsetLeft + this._element.offsetWidth;
        yearMonthEnd = startOffset - yearMonthRightElement.offsetLeft;
        daysAreaEnd = startOffset - (daysAreaElement.offsetLeft + daysAreaElement.offsetWidth) + weekColumnWidth + maxCellWidth * 7 + DaysAreaContainerBorder;
    } else {
        yearMonthEnd = yearMonthRightElement.offsetLeft + yearMonthRightElement.offsetWidth;
        daysAreaEnd = daysAreaElement.offsetLeft + weekColumnWidth + maxCellWidth * 7 + DaysAreaContainerBorder;
    }
    var maxEnd = Math.max(yearMonthEnd, daysAreaEnd);
    var MainPadding = 6; // FIXME: Fix name.
    var MainBorder = 1;
    var desiredBodyWidth = maxEnd + MainPadding + MainBorder;

    var elementHeight = this._element.offsetHeight;
    this._element.style.width = "auto";
    daysAreaElement.style.width = "100%";
    daysAreaElement.style.tableLayout = "fixed";
    this._element.getElementsByClassName(ClassNames.YearMonthUpper)[0].style.display = "-webkit-box";
    this._element.getElementsByClassName(ClassNames.MonthSelectorBox)[0].style.display = "block";
    resizeWindow(desiredBodyWidth, elementHeight);
};

CalendarPicker.prototype._layoutButtons = function() {
    var container = createElement("div", ClassNames.TodayClearArea);
    this.today = createElement("input", ClassNames.TodayButton);
    this.today.disabled = !this.isValidDate(this.selectionConstructor.createFromToday());
    this.today.type = "button";
    this.today.value = this._config.todayLabel;
    this.today.addEventListener("click", this.handleToday.bind(this), false);
    container.appendChild(this.today);
    this.clear = null;
    if (!this._config.required) {
        this.clear = createElement("input", ClassNames.ClearButton);
        this.clear.type = "button";
        this.clear.value = this._config.clearLabel;
        this.clear.addEventListener("click", this.handleClear.bind(this), false);
        container.appendChild(this.clear);
    }
    this._element.appendChild(container);

    this.lastFocusableControl = this.clear || this.today;
};

/**
 * @param {!Month} month
 * @return {!bool}
 */
CalendarPicker.prototype.shouldShowMonth = function(month) {
    return this._minimumMonth.valueOf() <= month.valueOf() && this.maximumMonth.valueOf() >= month.valueOf();
};

/**
 * @param {!Month} month
 * @param {!CalendarPicker.NavigationBehaviour=} navigationBehaviour
 */
CalendarPicker.prototype.showMonth = function(month, navigationBehaviour) {
    if (this._currentMonth.equals(month))
        return;
    else if (month.valueOf() < this._minimumMonth.valueOf())
        month = this._minimumMonth;
    else if (month.valueOf() > this.maximumMonth.valueOf())
        month = this.maximumMonth;
    this._yearMonthController.setMonth(month);
    this._daysTable.navigateToMonth(month, navigationBehaviour || CalendarPicker.NavigationBehaviour.None);
    this._currentMonth = month;
};

/**
 * @return {!Month}
 */
CalendarPicker.prototype.currentMonth = function() {
    return this._currentMonth;
};

// ----------------------------------------------------------------

/**
 * @constructor
 * @param {!CalendarPicker} picker
 */
function YearMonthController(picker) {
    this.picker = picker;
}

/**
 * @param {!Element} element
 */
YearMonthController.prototype.attachTo = function(element) {
    var outerContainer = createElement("div", ClassNames.YearMonthArea);

    var innerContainer = createElement("div", ClassNames.YearMonthUpper);
    outerContainer.appendChild(innerContainer);

    this._attachLeftButtonsTo(innerContainer);

    var box = createElement("div", ClassNames.MonthSelectorBox);
    innerContainer.appendChild(box);
    // We can't use <select> popup in PagePopup.
    this._monthPopup = createElement("div", ClassNames.MonthSelectorPopup);
    this._monthPopup.addEventListener("click", this._handleYearMonthChange.bind(this), false);
    this._monthPopup.addEventListener("keydown", this._handleMonthPopupKey.bind(this), false);
    this._monthPopup.addEventListener("mousemove", this._handleMouseMove.bind(this), false);
    this._updateSelectionOnMouseMove = true;
    this._monthPopup.tabIndex = 0;
    this._monthPopupContents = createElement("div", ClassNames.MonthSelectorPopupContents);
    this._monthPopup.appendChild(this._monthPopupContents);
    box.appendChild(this._monthPopup);
    this._month = createElement("div", ClassNames.MonthSelector);
    this._month.addEventListener("click", this._showPopup.bind(this), false);
    box.appendChild(this._month);

    this._attachRightButtonsTo(innerContainer);
    element.appendChild(outerContainer);

    this._wall = createElement("div", ClassNames.MonthSelectorWall);
    this._wall.addEventListener("click", this._closePopup.bind(this), false);
    element.appendChild(this._wall);

    var month = this.picker.maximumMonth;
    var maxWidth = 0;
    for (var m = 0; m < 12; ++m) {
        this._month.textContent = month.toLocaleString();
        maxWidth = Math.max(maxWidth, this._month.offsetWidth);
        month = month.previous();
    }
    if (getLanguage() == "ja" && ImperialEraLimit < this.picker.maximumMonth.year) {
        for (var m = 0; m < 12; ++m) {
            this._month.textContent = new Month(ImperialEraLimit, m).toLocaleString();
            maxWidth = Math.max(maxWidth, this._month.offsetWidth);
        }
    }
    this._month.style.minWidth = maxWidth + 'px';

    this.picker.firstFocusableControl = this._left2; // FIXME: Should it be this.month?
};

YearMonthController.addTenYearsButtons = false;

/**
 * @param {!Element} parent
 */
YearMonthController.prototype._attachLeftButtonsTo = function(parent) {
    var container = createElement("div", ClassNames.YearMonthButtonLeft);
    parent.appendChild(container);

    if (YearMonthController.addTenYearsButtons) {
        this._left3 = createElement("input", ClassNames.YearMonthButton);
        this._left3.type = "button";
        this._left3.value = "<<<";
        this._left3.addEventListener("click", this._handleButtonClick.bind(this), false);
        container.appendChild(this._left3);
    }

    this._left2 = createElement("input", ClassNames.YearMonthButton);
    this._left2.type = "button";
    this._left2.value = "<<";
    this._left2.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._left2);

    this._left1 = createElement("input", ClassNames.YearMonthButton);
    this._left1.type = "button";
    this._left1.value = "<";
    this._left1.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._left1);
};

/**
 * @param {!Element} parent
 */
YearMonthController.prototype._attachRightButtonsTo = function(parent) {
    var container = createElement("div", ClassNames.YearMonthButtonRight);
    parent.appendChild(container);
    this._right1 = createElement("input", ClassNames.YearMonthButton);
    this._right1.type = "button";
    this._right1.value = ">";
    this._right1.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._right1);

    this._right2 = createElement("input", ClassNames.YearMonthButton);
    this._right2.type = "button";
    this._right2.value = ">>";
    this._right2.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._right2);

    if (YearMonthController.addTenYearsButtons) {
        this._right3 = createElement("input", ClassNames.YearMonthButton);
        this._right3.type = "button";
        this._right3.value = ">>>";
        this._right3.addEventListener("click", this._handleButtonClick.bind(this), false);
        container.appendChild(this._right3);
    }
};

/**
 * @param {!Month} month
 */
YearMonthController.prototype.setMonth = function(month) {
    var monthValue = month.valueOf();
    if (this._left3)
        this._left3.disabled = !this.picker.shouldShowMonth(new Month(monthValue - 13));
    this._left2.disabled = !this.picker.shouldShowMonth(new Month(monthValue - 2));
    this._left1.disabled = !this.picker.shouldShowMonth(new Month(monthValue - 1));
    this._right1.disabled = !this.picker.shouldShowMonth(new Month(monthValue + 1));
    this._right2.disabled = !this.picker.shouldShowMonth(new Month(monthValue + 2));
    if (this._right3)
        this._left3.disabled = !this.picker.shouldShowMonth(new Month(monthValue + 13));
    this._month.innerText = month.toLocaleString();
    while (this._monthPopupContents.hasChildNodes())
        this._monthPopupContents.removeChild(this._monthPopupContents.firstChild);

    for (var m = monthValue - 6; m <= monthValue + 6; m++) {
        var month = new Month(m);
        if (!this.picker.shouldShowMonth(month))
            continue;
        var option = createElement("div", ClassNames.MonthSelectorPopupEntry, month.toLocaleString());
        option.dataset.value = month.toString();
        this._monthPopupContents.appendChild(option);
        if (m == monthValue)
            option.classList.add(ClassNames.SelectedMonthYear);
    }
};

YearMonthController.prototype._showPopup = function() {
    this._monthPopup.style.display = "block";
    this._monthPopup.style.zIndex = "1000"; // Larger than the days area.
    this._monthPopup.style.left = this._month.offsetLeft + (this._month.offsetWidth - this._monthPopup.offsetWidth) / 2 + "px";
    this._monthPopup.style.top = this._month.offsetTop + this._month.offsetHeight + "px";

    this._wall.style.display = "block";
    this._wall.style.zIndex = "999"; // This should be smaller than the z-index of monthPopup.

    var popupHeight = this._monthPopup.clientHeight;
    var fullHeight = this._monthPopupContents.clientHeight;
    if (fullHeight > popupHeight) {
        var selected = this._getSelection();
        if (selected) {
           var bottom = selected.offsetTop + selected.clientHeight;
           if (bottom > popupHeight)
               this._monthPopup.scrollTop = bottom - popupHeight;
        }
        this._monthPopup.style.webkitPaddingEnd = getScrollbarWidth() + 'px';
    }
    this._monthPopup.focus();
};

YearMonthController.prototype._closePopup = function() {
    this._monthPopup.style.display = "none";
    this._wall.style.display = "none";
    var container = document.querySelector("." + ClassNames.DaysAreaContainer);
    container.focus();
};

/**
 * @return {Element} Selected element in the month-year popup.
 */
YearMonthController.prototype._getSelection = function()
{
    return document.querySelector("." + ClassNames.SelectedMonthYear);
}

/**
 * @param {Event} event
 */
YearMonthController.prototype._handleMouseMove = function(event)
{
    if (!this._updateSelectionOnMouseMove) {
        // Selection update turned off while navigating with keyboard to prevent a mouse
        // move trigged during a scroll from resetting the selection. Automatically
        // rearm control to enable mouse-based selection.
        this._updateSelectionOnMouseMove = true;
    } else {
        var target = event.target;
        var selection = this._getSelection();
        if (target && target != selection && target.classList.contains(ClassNames.MonthSelectorPopupEntry)) {
            if (selection)
                selection.classList.remove(ClassNames.SelectedMonthYear);
            target.classList.add(ClassNames.SelectedMonthYear);
        }
    }
    event.stopPropagation();
    event.preventDefault();
}

/**
 * @param {Event} event
 */
YearMonthController.prototype._handleMonthPopupKey = function(event)
{
    var key = event.keyIdentifier;
    if (key == "Down") {
        var selected = this._getSelection();
        if (selected) {
            var next = selected.nextSibling;
            if (next) {
                selected.classList.remove(ClassNames.SelectedMonthYear);
                next.classList.add(ClassNames.SelectedMonthYear);
                var bottom = next.offsetTop + next.clientHeight;
                if (bottom > this._monthPopup.scrollTop + this._monthPopup.clientHeight) {
                    this._updateSelectionOnMouseMove = false;
                    this._monthPopup.scrollTop = bottom - this._monthPopup.clientHeight;
                }
            }
        }
        event.stopPropagation();
        event.preventDefault();
    } else if (key == "Up") {
        var selected = this._getSelection();
        if (selected) {
            var previous = selected.previousSibling;
            if (previous) {
                selected.classList.remove(ClassNames.SelectedMonthYear);
                previous.classList.add(ClassNames.SelectedMonthYear);
                if (previous.offsetTop < this._monthPopup.scrollTop) {
                    this._updateSelectionOnMouseMove = false;
                    this._monthPopup.scrollTop = previous.offsetTop;
                }
            }
        }
        event.stopPropagation();
        event.preventDefault();
    } else if (key == "U+001B") {
        this._closePopup();
        event.stopPropagation();
        event.preventDefault();
    } else if (key == "Enter") {
        this._handleYearMonthChange();
        event.stopPropagation();
        event.preventDefault();
    }
}

YearMonthController.prototype._handleYearMonthChange = function() {
    this._closePopup();
    var selection = this._getSelection();
    if (!selection)
        return;
    this.picker.showMonth(Month.parse(selection.dataset.value));
};

/**
 * @const
 * @type {number}
 */
YearMonthController.PreviousTenYears = -120;
/**
 * @const
 * @type {number}
 */
YearMonthController.PreviousYear = -12;
/**
 * @const
 * @type {number}
 */
YearMonthController.PreviousMonth = -1;
/**
 * @const
 * @type {number}
 */
YearMonthController.NextMonth = 1;
/**
 * @const
 * @type {number}
 */
YearMonthController.NextYear = 12;
/**
 * @const
 * @type {number}
 */
YearMonthController.NextTenYears = 120;

/**
 * @param {Event} event
 */
YearMonthController.prototype._handleButtonClick = function(event) {
    if (event.target == this._left3)
        this.moveRelatively(YearMonthController.PreviousTenYears);
    else if (event.target == this._left2)
        this.moveRelatively(YearMonthController.PreviousYear);
    else if (event.target == this._left1)
        this.moveRelatively(YearMonthController.PreviousMonth);
    else if (event.target == this._right1)
        this.moveRelatively(YearMonthController.NextMonth)
    else if (event.target == this._right2)
        this.moveRelatively(YearMonthController.NextYear);
    else if (event.target == this._right3)
        this.moveRelatively(YearMonthController.NextTenYears);
    else
        return;
};

/**
 * @param {!number} amount
 */
YearMonthController.prototype.moveRelatively = function(amount) {
    var current = this.picker.currentMonth().valueOf();
    var updated = new Month(current + amount);
    this.picker.showMonth(updated, CalendarPicker.NavigationBehaviour.Animate | CalendarPicker.NavigationBehaviour.KeepSelectionPosition);
};

// ----------------------------------------------------------------

/**
 * @constructor
 * @param {!CalendarPicker} picker
 */
function DaysTable(picker) {
    this.picker = picker;
}

/**
 * @return {!boolean}
 */
DaysTable.prototype._hasSelection = function() {
    return !!this._firstNodeInSelectedRange();
}

/**
 * The number of week lines in the screen.
 * @const
 * @type {number}
 */
DaysTable._Weeks = 6;

/**
 * @param {!Element} element
 */
DaysTable.prototype.attachTo = function(element) {
    this._daysContainer = createElement("table", ClassNames.DaysArea);
    this._daysContainer.addEventListener("click", this._handleDayClick.bind(this), false);
    this._daysContainer.addEventListener("mouseover", this._handleMouseOver.bind(this), false);
    this._daysContainer.addEventListener("mouseout", this._handleMouseOut.bind(this), false);
    this._daysContainer.addEventListener("webkitTransitionEnd", this._moveInDays.bind(this), false);
    var container = createElement("tr", ClassNames.DayLabelContainer);
    var weekStartDay = global.params.weekStartDay || 0;
    container.appendChild(createElement("th", ClassNames.DayLabel + " " + ClassNames.WeekColumn, global.params.weekLabel));
    for (var i = 0; i < 7; i++)
        container.appendChild(createElement("th", ClassNames.DayLabel, global.params.dayLabels[(weekStartDay + i) % 7]));
    this._daysContainer.appendChild(container);
    this._days = [];
    this._weekNumbers = [];
    for (var w = 0; w < DaysTable._Weeks; w++) {
        container = createElement("tr", ClassNames.WeekContainer);
        var week = [];
        var weekNumberNode = createElement("td", ClassNames.Day + " " + ClassNames.WeekColumn, " ");
        weekNumberNode.dataset.positionX = -1;
        weekNumberNode.dataset.positionY = w;
        this._weekNumbers.push(weekNumberNode);
        container.appendChild(weekNumberNode);
        for (var d = 0; d < 7; d++) {
            var day = createElement("td", ClassNames.Day, " ");
            day.setAttribute("data-position-x", String(d));
            day.setAttribute("data-position-y", String(w));
            week.push(day);
            container.appendChild(day);
        }
        this._days.push(week);
        this._daysContainer.appendChild(container);
    }
    container = createElement("div", ClassNames.DaysAreaContainer);
    container.appendChild(this._daysContainer);
    container.tabIndex = 0;
    container.addEventListener("keydown", this._handleKey.bind(this), false);
    element.appendChild(container);

    container.focus();
};

/**
 * @param {!number} value
 * @return {!boolean}
 */
CalendarPicker.prototype._stepMismatch = function(value) {
    var nextAllowedValue = Math.ceil((value - this.stepBase) / this.step) * this.step + this.stepBase;
    return nextAllowedValue >= value + this.selectionConstructor.DefaultStep
}

/**
 * @param {!number} value
 * @return {!boolean}
 */
CalendarPicker.prototype._outOfRange = function(value) {
    return value < this._minimumValue || value > this._maximumValue;
}

/**
 * @param {!Month|Day} range
 * @return {!boolean}
 */
CalendarPicker.prototype.isValidDate = function(range) {
    var value = range.valueOf();
    return !this._outOfRange(value) && !this._stepMismatch(value);
}

/**
 * @param {!Month} month
 */
DaysTable.prototype._renderMonth = function(month) {
    var dayIterator = month.startDate();
    var monthStartDay = dayIterator.getUTCDay();
    var weekStartDay = global.params.weekStartDay || 0;
    var startOffset = weekStartDay - monthStartDay;
    if (startOffset >= 0)
        startOffset -= 7;
    dayIterator.setUTCDate(startOffset + 1);
    var mondayOffset = (8 - weekStartDay) % 7;
    var sundayOffset = weekStartDay % 7;
    for (var w = 0; w < DaysTable._Weeks; w++) {
        for (var d = 0; d < 7; d++) {
            var iterMonth = Month.createFromDate(dayIterator);
            var iterWeek = Week.createFromDate(dayIterator);
            var time = dayIterator.getTime();
            var element = this._days[w][d];
            element.innerText = localizeNumber(dayIterator.getUTCDate());
            element.className = ClassNames.Day;
            element.dataset.submitValue = Day.createFromDate(dayIterator).toString();
            element.dataset.weekValue = iterWeek.toString();
            element.dataset.monthValue = iterMonth.toString();
            if (isNaN(time)) {
                element.innerText = "-";
                element.classList.add(ClassNames.Unavailable);
            } else if (!this.picker.isValidDate(this._rangeForNode(element)))
                element.classList.add(ClassNames.Unavailable);
            else if (!iterMonth.equals(month)) {
                element.classList.add(ClassNames.Available);
                element.classList.add(ClassNames.NotThisMonth);
            } else
                element.classList.add(ClassNames.Available);
            if (d === mondayOffset) {
                element.classList.add(ClassNames.Monday);
                if (this._weekNumbers[w]) {
                    this._weekNumbers[w].dataset.weekValue = iterWeek.toString();
                    this._weekNumbers[w].innerText = localizeNumber(iterWeek.week);
                    if (element.classList.contains(ClassNames.Available))
                        this._weekNumbers[w].classList.add(ClassNames.Available);
                    else
                        this._weekNumbers[w].classList.add(ClassNames.Unavailable);
                }
            } else if (d === sundayOffset)
                element.classList.add(ClassNames.Sunday);
            dayIterator.setUTCDate(dayIterator.getUTCDate() + 1);
        }
    }
};

/**
 * @param {!Month} month
 * @param {!CalendarPicker.NavigationBehaviour} navigationBehaviour
 */
DaysTable.prototype.navigateToMonth = function(month, navigationBehaviour) {
    var firstNodeInSelectedRange = this._firstNodeInSelectedRange();
    if (navigationBehaviour & CalendarPicker.NavigationBehaviour.Animate)
        this._startMoveInAnimation(month);
    this._renderMonth(month);
    if (navigationBehaviour & CalendarPicker.NavigationBehaviour.KeepSelectionPosition && firstNodeInSelectedRange) {
        var x = parseInt(firstNodeInSelectedRange.dataset.positionX, 10);
        var y = parseInt(firstNodeInSelectedRange.dataset.positionY, 10);
        this._selectRangeAtPosition(x, y);
    }
};

/**
 * @param {!Month} month
 */
DaysTable.prototype._startMoveInAnimation = function(month) {
    if (this.picker.isPreparing)
        return;
    var daysStyle = this._daysContainer.style;
    daysStyle.position = "relative";
    daysStyle.webkitTransition = "left 0.1s ease";
    daysStyle.left = (this.picker.currentMonth().valueOf() > month.valueOf() ? "" : "-") + this._daysContainer.offsetWidth + "px";
};

DaysTable.prototype._moveInDays = function() {
    var daysStyle = this._daysContainer.style;
    if (daysStyle.left == "0px")
        return;
    daysStyle.webkitTransition = "";
    daysStyle.left = (daysStyle.left.charAt(0) == "-" ? "" : "-") + this._daysContainer.offsetWidth + "px";
    this._daysContainer.offsetLeft; // Force to layout.
    daysStyle.webkitTransition = "left 0.1s ease";
    daysStyle.left = "0px";
};

/**
 * @param {!Day} day
 */
DaysTable.prototype._markRangeAsSelected = function(day) {
    var dateString = day.toString();
    for (var w = 0; w < DaysTable._Weeks; w++) {
        for (var d = 0; d < 7; d++) {
            if (this._days[w][d].dataset.submitValue == dateString) {
                this._days[w][d].classList.add(ClassNames.Selected);
                break;
            }
        }
    }
};

/**
 * @param {!Day} day
 */
DaysTable.prototype.selectRange = function(day) {
    this._deselect();
    if (this.startDate() > day.startDate() || this.endDate() < day.endDate())
        this.picker.showMonth(Month.createFromDate(day.startDate()), CalendarPicker.NavigationBehaviour.Animate);
    this._markRangeAsSelected(day);
};

/**
 * @param {!Day} day
 */
DaysTable.prototype.selectRangeAndShowEntireRange = function(day) {
    this.selectRange(day);
};

/**
 * @param {!Element} dayNode
 */
DaysTable.prototype._selectRangeContainingNode = function(dayNode) {
    var range = this._rangeForNode(dayNode);
    if (!range)
        return;
    this.selectRange(range);
};

/**
 * @param {!Element} dayNode
 * @return {?Day}
 */
DaysTable.prototype._rangeForNode = function(dayNode) {
    if (!dayNode)
        return null;
    return Day.parse(dayNode.dataset.submitValue);
};

/**
 * @return {!Date}
 */
DaysTable.prototype.startDate = function() {
    return Day.parse(this._days[0][0].dataset.submitValue).startDate();
};

/**
 * @return {!Date}
 */
DaysTable.prototype.endDate = function() {
    return Day.parse(this._days[DaysTable._Weeks - 1][7 - 1].dataset.submitValue).endDate();
};

/**
 * @param {!number} x
 * @param {!number} y
 */
DaysTable.prototype._selectRangeAtPosition = function(x, y) {
    var node = x === -1 ? this._weekNumbers[y] : this._days[y][x];
    this._selectRangeContainingNode(node);
};

/**
 * @return {!Element}
 */
DaysTable.prototype._firstNodeInSelectedRange = function() {
    return this._daysContainer.getElementsByClassName(ClassNames.Selected)[0];
};

DaysTable.prototype._deselect = function() {
    var selectedNodes = this._daysContainer.getElementsByClassName(ClassNames.Selected);
    for (var node = selectedNodes[0]; node; node = selectedNodes[0])
        node.classList.remove(ClassNames.Selected);
};

/**
 * @param {!CalendarPicker.NavigationBehaviour=} navigationBehaviour
 * @return {!boolean}
 */
DaysTable.prototype._maybeSetPreviousMonth = function(navigationBehaviour) {
    if (typeof navigationBehaviour === "undefined")
        navigationBehaviour = CalendarPicker.NavigationBehaviour.Animate;
    var previousMonth = this.picker.currentMonth().previous();
    if (!this.picker.shouldShowMonth(previousMonth))
        return false;
    this.picker.showMonth(previousMonth, navigationBehaviour);
    return true;
};

/**
 * @param {!CalendarPicker.NavigationBehaviour=} navigationBehaviour
 * @return {!boolean}
 */
DaysTable.prototype._maybeSetNextMonth = function(navigationBehaviour) {
    if (typeof navigationBehaviour === "undefined")
        navigationBehaviour = CalendarPicker.NavigationBehaviour.Animate;
    var nextMonth = this.picker.currentMonth().next();
    if (!this.picker.shouldShowMonth(nextMonth))
        return false;
    this.picker.showMonth(nextMonth, navigationBehaviour);
    return true;
};

/**
 * @param {Event} event
 */
DaysTable.prototype._handleDayClick = function(event) {
    if (event.target.classList.contains(ClassNames.Available))
        this.picker.submitValue(this._rangeForNode(event.target).toString());
};

/**
 * @param {Event} event
 */
DaysTable.prototype._handleMouseOver = function(event) {
    var node = event.target;
    if (node.classList.contains(ClassNames.Selected))
        return;
    this._selectRangeContainingNode(node);
};

/**
 * @param {Event} event
 */
DaysTable.prototype._handleMouseOut = function(event) {
    this._deselect();
};

/**
 * @param {Event} event
 */
DaysTable.prototype._handleKey = function(event) {
    this.picker.maybeUpdateFocusStyle();
    var x = -1;
    var y = -1;
    var key = event.keyIdentifier;
    var firstNodeInSelectedRange = this._firstNodeInSelectedRange();
    if (firstNodeInSelectedRange) {
        x = parseInt(firstNodeInSelectedRange.dataset.positionX, 10);
        y = parseInt(firstNodeInSelectedRange.dataset.positionY, 10);
    }
    if (!this._hasSelection() && (key == "Left" || key == "Up" || key == "Right" || key == "Down")) {
        // Put the selection on a center cell.
        this.updateSelection(event, 3, Math.floor(DaysTable._Weeks / 2 - 1));
        return;
    }

    if (key == (global.params.isLocaleRTL ? "Right" : "Left")) {
        if (x == 0) {
            if (y == 0) {
                if (!this._maybeSetPreviousMonth())
                    return;
                y = DaysTable._Weeks - 1;
            } else
                y--;
            x = 6;
        } else
            x--;
        this.updateSelection(event, x, y);

    } else if (key == "Up") {
        if (y == 0) {
            if (!this._maybeSetPreviousMonth())
                return;
            y = DaysTable._Weeks - 1;
        } else
            y--;
        this.updateSelection(event, x, y);

    } else if (key == (global.params.isLocaleRTL ? "Left" : "Right")) {
        if (x == 6) {
            if (y == DaysTable._Weeks - 1) {
                if (!this._maybeSetNextMonth())
                    return;
                y = 0;
            } else
                y++;
            x = 0;
        } else
            x++;
        this.updateSelection(event, x, y);

    } else if (key == "Down") {
        if (y == DaysTable._Weeks - 1) {
            if (!this._maybeSetNextMonth())
                return;
            y = 0;
        } else
            y++;
        this.updateSelection(event, x, y);

    } else if (key == "PageUp") {
        if (!this._maybeSetPreviousMonth())
            return;
        this.updateSelection(event, x, y);

    } else if (key == "PageDown") {
        if (!this._maybeSetNextMonth())
            return;
        this.updateSelection(event, x, y);

    } else if (this._hasSelection() && key == "Enter") {
        var dayNode = this._days[y][x];
        if (dayNode.classList.contains(ClassNames.Available)) {
            this.picker.submitValue(dayNode.dataset.submitValue);
            event.stopPropagation();
        }

    } else if (key == "U+0054") { // 't'
        this.selectRangeAndShowEntireRange(Day.createFromToday());
        event.stopPropagation();
        event.preventDefault();
    }
};

/**
 * @param {Event} event
 * @param {!number} x
 * @param {!number} y
 */
DaysTable.prototype.updateSelection = function(event, x, y) {
    this._selectRangeAtPosition(x, y);
    event.stopPropagation();
    event.preventDefault();
};

/**
 * @constructor
 * @param{!CalendarPicker} picker
 */
function MonthPickerDaysTable(picker) {
    DaysTable.call(this, picker);
}
MonthPickerDaysTable.prototype = Object.create(DaysTable.prototype);

/**
 * @param {!Month} month
 * @param {!CalendarPicker.NavigationBehaviour} navigationBehaviour
 */
MonthPickerDaysTable.prototype.navigateToMonth = function(month, navigationBehaviour) {
    var hadSelection = this._hasSelection();
    if (navigationBehaviour & CalendarPicker.NavigationBehaviour.Animate)
        this._startMoveInAnimation(month);
    this._renderMonth(month);
    if (navigationBehaviour & CalendarPicker.NavigationBehaviour.KeepSelectionPosition && hadSelection)
        this.selectRange(month);
};

/**
 * @param {!Month} month
 */
MonthPickerDaysTable.prototype._markRangeAsSelected = function(month) {
    var monthString = month.toString();
    for (var w = 0; w < DaysTable._Weeks; w++) {
        for (var d = 0; d < 7; d++) {
            if (this._days[w][d].dataset.monthValue == monthString) {
                this._days[w][d].classList.add(ClassNames.Selected);
            }
        }
    }
};

/**
 * @param {!Month} month
 */
MonthPickerDaysTable.prototype.selectRange = function(month) {
    this._deselect();
    if (this.startDate() >= month.endDate() || this.endDate() <= month.startDate())
        this.picker.showMonth(month, CalendarPicker.NavigationBehaviour.Animate);
    this._markRangeAsSelected(month);
};

/**
 * @param {!Month} month
 */
MonthPickerDaysTable.prototype.selectRangeAndShowEntireRange = function(month) {
    this._deselect();
    this.picker.showMonth(month, CalendarPicker.NavigationBehaviour.Animate);
    this._markRangeAsSelected(month);
};

/**
 * @param {!Element} dayNode
 * @return {?Month}
 */
MonthPickerDaysTable.prototype._rangeForNode = function(dayNode) {
    if (!dayNode)
        return null;
    return Month.parse(dayNode.dataset.monthValue);
};

/**
 * @param {Event} event
 */
MonthPickerDaysTable.prototype._handleKey = function(event) {
    this.picker.maybeUpdateFocusStyle();
    var key = event.keyIdentifier;
    var eventHandled = false;
    var currentMonth = this.picker.currentMonth();
    var firstNodeInSelectedRange = this._firstNodeInSelectedRange();
    var selectedMonth = this._rangeForNode(firstNodeInSelectedRange);
    if (!firstNodeInSelectedRange
        && (key == "Right" || key == "Left" || key == "Up" || key == "Down" || key == "PageUp" || key == "PageDown")) {
        this.selectRange(currentMonth);
        eventHandled = true;
    } else if (key == (global.params.isLocaleRTL ? "Right" : "Left") || key == "Up" || key == "PageUp") {
        if (selectedMonth.valueOf() > currentMonth.valueOf())
            this.selectRangeAndShowEntireRange(currentMonth);
        else
            this.selectRangeAndShowEntireRange(currentMonth.previous());
        eventHandled = true;
    } else if (key == (global.params.isLocaleRTL ? "Left" : "Right") || key == "Down" || key == "PageDown") {
        if (selectedMonth.valueOf() < currentMonth.valueOf())
            this.selectRangeAndShowEntireRange(currentMonth);
        else
            this.selectRangeAndShowEntireRange(currentMonth.next());
        eventHandled = true;
    } else if (selectedMonth && key == "Enter") {
        if (firstNodeInSelectedRange.classList.contains(ClassNames.Available)) {
            this.picker.submitValue(selectedMonth.toString());
            eventHandled = true;
        }
    } else if (key == "U+0054") { // 't'
        this.selectRangeAndShowEntireRange(Month.createFromToday());
        eventHandled = true;
    }
    if (eventHandled) {
        event.stopPropagation();
        event.preventDefault();
    }
};

/**
 * @constructor
 * @param{!CalendarPicker} picker
 */
function WeekPickerDaysTable(picker) {
    DaysTable.call(this, picker);
}
WeekPickerDaysTable.prototype = Object.create(DaysTable.prototype);

/**
 * @param {!Week} week
 */
WeekPickerDaysTable.prototype._markRangeAsSelected = function(week) {
    var weekString = week.toString();
    for (var w = 0; w < DaysTable._Weeks; w++) {
        for (var d = 0; d < 7; d++) {
            if (this._days[w][d].dataset.weekValue == weekString) {
                this._days[w][d].classList.add(ClassNames.Selected);
            }
        }
    }
    for (var i = 0; i < this._weekNumbers.length; ++i) {
        if (this._weekNumbers[i].dataset.weekValue === weekString) {
            this._weekNumbers[i].classList.add(ClassNames.Selected);
            break;
        }
    }
};

/**
 * @param {!Week} week
 */
WeekPickerDaysTable.prototype.selectRange = function(week) {
    this._deselect();
    var weekStartDate = week.startDate();
    var weekEndDate = week.endDate();
    if (this.startDate() >= weekEndDate)
        this.picker.showMonth(Month.createFromDate(weekEndDate), CalendarPicker.NavigationBehaviour.Animate);
    else if (this.endDate() <= weekStartDate)
        this.picker.showMonth(Month.createFromDate(weekStartDate), CalendarPicker.NavigationBehaviour.Animate);
    this._markRangeAsSelected(week);
};

/**
 * @param {!Week} week
 */
WeekPickerDaysTable.prototype.selectRangeAndShowEntireRange = function(week) {
    this._deselect();
    var weekStartDate = week.startDate();
    var weekEndDate = week.endDate();
    if (this.startDate() > weekStartDate)
        this.picker.showMonth(Month.createFromDate(weekStartDate), CalendarPicker.NavigationBehaviour.Animate);
    else if (this.endDate() < weekEndDate)
        this.picker.showMonth(Month.createFromDate(weekEndDate), CalendarPicker.NavigationBehaviour.Animate);
    this._markRangeAsSelected(week);
};

/**
 * @param {!Element} dayNode
 * @return {?Week}
 */
WeekPickerDaysTable.prototype._rangeForNode = function(dayNode) {
    if (!dayNode)
        return null;
    return Week.parse(dayNode.dataset.weekValue);
};

/**
 * @param {!Event} event
 */
WeekPickerDaysTable.prototype._handleKey = function(event) {
    this.picker.maybeUpdateFocusStyle();
    var key = event.keyIdentifier;
    var eventHandled = false;
    var currentMonth = this.picker.currentMonth();
    var firstNodeInSelectedRange = this._firstNodeInSelectedRange();
    var selectedWeek = this._rangeForNode(firstNodeInSelectedRange);
    if (!firstNodeInSelectedRange
        && (key == "Right" || key == "Left" || key == "Up" || key == "Down" || key == "PageUp" || key == "PageDown")) {
        // Put the selection on a center cell.
        this._selectRangeAtPosition(3, Math.floor(DaysTable._Weeks / 2 - 1));
    } else if (key == (global.params.isLocaleRTL ? "Right" : "Left") || key == "Up") {
        this.selectRangeAndShowEntireRange(selectedWeek.previous());
        eventHandled = true;
    } else if (key == (global.params.isLocaleRTL ? "Left" : "Right") || key == "Down") {
        this.selectRangeAndShowEntireRange(selectedWeek.next());
        eventHandled = true;
    } else if (key == "PageUp") {
        if (!this._maybeSetPreviousMonth(CalendarPicker.NavigationBehaviour.Animate | CalendarPicker.NavigationBehaviour.KeepSelectionPosition))
            return;
        eventHandled = true;
    } else if (key == "PageDown") {
        if (!this._maybeSetNextMonth(CalendarPicker.NavigationBehaviour.Animate | CalendarPicker.NavigationBehaviour.KeepSelectionPosition))
            return;
        eventHandled = true;
    } else if (selectedWeek && key == "Enter") {
        if (firstNodeInSelectedRange.classList.contains(ClassNames.Available)) {
            this.picker.submitValue(selectedWeek.toString());
            eventHandled = true;
        }
    } else if (key == "U+0054") { // 't'
        this.selectRangeAndShowEntireRange(Week.createFromToday());
        eventHandled = true;
    }
    if (eventHandled) {
        event.stopPropagation();
        event.preventDefault();
    }
};

/**
 * @param {!Event} event
 */
CalendarPicker.prototype._handleBodyKeyDown = function(event) {
    this.maybeUpdateFocusStyle();
    var key = event.keyIdentifier;
    if (key == "U+0009") {
        if (!event.shiftKey && document.activeElement == this.lastFocusableControl) {
            event.stopPropagation();
            event.preventDefault();
            this.firstFocusableControl.focus();
        } else if (event.shiftKey && document.activeElement == this.firstFocusableControl) {
            event.stopPropagation();
            event.preventDefault();
            this.lastFocusableControl.focus();
        }
    } else if (key == "U+004D") { // 'm'
        this._yearMonthController.moveRelatively(event.shiftKey ? YearMonthController.PreviousMonth : YearMonthController.NextMonth);
    } else if (key == "U+0059") { // 'y'
        this._yearMonthController.moveRelatively(event.shiftKey ? YearMonthController.PreviousYear : YearMonthController.NextYear);
    } else if (key == "U+0044") { // 'd'
        this._yearMonthController.moveRelatively(event.shiftKey ? YearMonthController.PreviousTenYears : YearMonthController.NextTenYears);
    } else if (key == "U+001B") // ESC
        this.handleCancel();
}

CalendarPicker.prototype.maybeUpdateFocusStyle = function() {
    if (this._hadKeyEvent)
        return;
    this._hadKeyEvent = true;
    this._element.classList.remove(ClassNames.NoFocusRing);
}

if (window.dialogArguments) {
    initialize(dialogArguments);
} else {
    window.addEventListener("message", handleMessage, false);
    window.setTimeout(handleArgumentsTimeout, 1000);
}
