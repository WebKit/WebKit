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
 * @enum {number}
 */
var WeekDay = {
    Sunday: 0,
    Monday: 1,
    Tuesday: 2,
    Wednesday: 3,
    Thursday: 4,
    Friday: 5,
    Saturday: 6
};

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

function createUTCDate(year, month, date) {
    var newDate = new Date(0);
    newDate.setUTCFullYear(year);
    newDate.setUTCMonth(month);
    newDate.setUTCDate(date);
    return newDate;
}

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
 * @const
 * @type {number}
 */
var DaysPerWeek = 7;

/**
 * @const
 * @type {number}
 */
var MonthsPerYear = 12;

/**
 * @const
 * @type {number}
 */
var MillisecondsPerDay = 24 * 60 * 60 * 1000;

/**
 * @const
 * @type {number}
 */
var MillisecondsPerWeek = DaysPerWeek * MillisecondsPerDay;

/**
 * @constructor
 */
function DateType() {
}

/**
 * @constructor
 * @extends DateType
 * @param {!number} year
 * @param {!number} month
 * @param {!number} date
 */
function Day(year, month, date) {
    var dateObject = createUTCDate(year, month, date);
    if (isNaN(dateObject.valueOf()))
        throw "Invalid date";
    /**
     * @type {number}
     * @const
     */
    this.year = dateObject.getUTCFullYear();   
     /**
     * @type {number}
     * @const
     */  
    this.month = dateObject.getUTCMonth();
    /**
     * @type {number}
     * @const
     */
    this.date = dateObject.getUTCDate();
};

Day.prototype = Object.create(DateType.prototype);

Day.ISOStringRegExp = /^(\d+)-(\d+)-(\d+)/;

/**
 * @param {!string} str
 * @return {?Day}
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
 * @param {!number} value
 * @return {!Day}
 */
Day.createFromValue = function(millisecondsSinceEpoch) {
    return Day.createFromDate(new Date(millisecondsSinceEpoch))
};

/**
 * @param {!Date} date
 * @return {!Day}
 */
Day.createFromDate = function(date) {
    if (isNaN(date.valueOf()))
        throw "Invalid date";
    return new Day(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate());
};

/**
 * @param {!Day} day
 * @return {!Day}
 */
Day.createFromDay = function(day) {
    return day;
};

/**
 * @return {!Day}
 */
Day.createFromToday = function() {
    var now = new Date();
    return new Day(now.getFullYear(), now.getMonth(), now.getDate());
};

/**
 * @param {!DateType} other
 * @return {!boolean}
 */
Day.prototype.equals = function(other) {
    return other instanceof Day && this.year === other.year && this.month === other.month && this.date === other.date;
};

/**
 * @param {!number=} offset
 * @return {!Day}
 */
Day.prototype.previous = function(offset) {
    if (typeof offset === "undefined")
        offset = 1;
    return new Day(this.year, this.month, this.date - offset);
};

/**
 * @param {!number=} offset
 * @return {!Day}
 */
Day.prototype.next = function(offset) {
 if (typeof offset === "undefined")
     offset = 1;
    return new Day(this.year, this.month, this.date + offset);
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
 * @return {!Day}
 */
Day.prototype.firstDay = function() {
    return this;
};

/**
 * @return {!Day}
 */
Day.prototype.middleDay = function() {
    return this;
};

/**
 * @return {!Day}
 */
Day.prototype.lastDay = function() {
    return this;
};

/**
 * @return {!number}
 */
Day.prototype.valueOf = function() {
    return createUTCDate(this.year, this.month, this.date).getTime();
};

/**
 * @return {!WeekDay}
 */
Day.prototype.weekDay = function() {
    return createUTCDate(this.year, this.month, this.date).getUTCDay();
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
Day.Minimum = Day.createFromValue(-62135596800000.0);
Day.Maximum = Day.createFromValue(8640000000000000.0);

// See WebCore/html/DayInputType.cpp.
Day.DefaultStep = 86400000;
Day.DefaultStepBase = 0;

/**
 * @constructor
 * @extends DateType
 * @param {!number} year
 * @param {!number} week
 */
function Week(year, week) { 
    /**
     * @type {number}
     * @const
     */
    this.year = year;
    /**
     * @type {number}
     * @const
     */
    this.week = week;
    // Number of years per year is either 52 or 53.
    if (this.week < 1 || (this.week > 52 && this.week > Week.numberOfWeeksInYear(this.year))) {
        var normalizedWeek = Week.createFromDay(this.firstDay());
        this.year = normalizedWeek.year;
        this.week = normalizedWeek.week;
    }
}

Week.ISOStringRegExp = /^(\d+)-[wW](\d+)$/;

// See WebCore/platform/DateComponents.h.
Week.Minimum = new Week(1, 1);
Week.Maximum = new Week(275760, 37);

// See WebCore/html/WeekInputType.cpp.
Week.DefaultStep = 604800000;
Week.DefaultStepBase = -259200000;

Week.EpochWeekDay = createUTCDate(1970, 0, 0).getUTCDay();

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
 * @param {!number} millisecondsSinceEpoch
 * @return {!Week}
 */
Week.createFromValue = function(millisecondsSinceEpoch) {
    return Week.createFromDate(new Date(millisecondsSinceEpoch))
};

/**
 * @param {!Date} date
 * @return {!Week}
 */
Week.createFromDate = function(date) {
    if (isNaN(date.valueOf()))
        throw "Invalid date";
    var year = date.getUTCFullYear();
    if (year <= Week.Maximum.year && Week.weekOneStartDateForYear(year + 1).getTime() <= date.getTime())
        year++;
    else if (year > 1 && Week.weekOneStartDateForYear(year).getTime() > date.getTime())
        year--;
    var week = 1 + Week._numberOfWeeksSinceDate(Week.weekOneStartDateForYear(year), date);
    return new Week(year, week);
};

/**
 * @param {!Day} day
 * @return {!Week}
 */
Week.createFromDay = function(day) {
    var year = day.year;
    if (year <= Week.Maximum.year && Week.weekOneStartDayForYear(year + 1) <= day)
        year++;
    else if (year > 1 && Week.weekOneStartDayForYear(year) > day)
        year--;
    var week = Math.floor(1 + (day.valueOf() - Week.weekOneStartDayForYear(year).valueOf()) / MillisecondsPerWeek);
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
    return createUTCDate(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
};

/**
 * @param {!number} year
 * @return {!Day}
 */
Week.weekOneStartDayForYear = function(year) {
    if (year < 1)
        return Day.Minimum;
    // The week containing January 4th is week one.
    var yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
    return new Day(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
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
    return Math.floor((date.getTime() - baseDate.getTime()) / MillisecondsPerWeek);
};

/**
 * @param {!DateType} other
 * @return {!boolean}
 */
Week.prototype.equals = function(other) {
    return other instanceof Week && this.year === other.year && this.week === other.week;
};

/**
 * @param {!number=} offset
 * @return {!Week}
 */
Week.prototype.previous = function(offset) {
    if (typeof offset === "undefined")
        offset = 1;
    return new Week(this.year, this.week - offset);
};

/**
 * @param {!number=} offset
 * @return {!Week}
 */
Week.prototype.next = function(offset) {
    if (typeof offset === "undefined")
        offset = 1;
    return new Week(this.year, this.week + offset);
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
 * @return {!Day}
 */
Week.prototype.firstDay = function() {
    var weekOneStartDay = Week.weekOneStartDayForYear(this.year);
    return weekOneStartDay.next((this.week - 1) * DaysPerWeek);
};

/**
 * @return {!Day}
 */
Week.prototype.middleDay = function() {
    return this.firstDay().next(3);
};

/**
 * @return {!Day}
 */
Week.prototype.lastDay = function() {
    if (this.equals(Week.Maximum))
        return Day.Maximum;
    return this.next().firstDay().previous();
};

/**
 * @return {!number}
 */
Week.prototype.valueOf = function() {
    return this.firstDay().valueOf() - createUTCDate(1970, 0, 1).getTime();
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
 * @extends DateType
 * @param {!number} year
 * @param {!number} month
 */
function Month(year, month) { 
    /**
     * @type {number}
     * @const
     */
    this.year = year + Math.floor(month / MonthsPerYear);
    /**
     * @type {number}
     * @const
     */
    this.month = month % MonthsPerYear < 0 ? month % MonthsPerYear + MonthsPerYear : month % MonthsPerYear;
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
 * @param {!number} value
 * @return {!Month}
 */
Month.createFromValue = function(monthsSinceEpoch) {
    return new Month(1970, monthsSinceEpoch)
};

/**
 * @param {!Date} date
 * @return {!Month}
 */
Month.createFromDate = function(date) {
    if (isNaN(date.valueOf()))
        throw "Invalid date";
    return new Month(date.getUTCFullYear(), date.getUTCMonth());
};

/**
 * @param {!Day} day
 * @return {!Month}
 */
Month.createFromDay = function(day) {
    return new Month(day.year, day.month);
};

/**
 * @return {!Month}
 */
Month.createFromToday = function() {
    var now = new Date();
    return new Month(now.getFullYear(), now.getMonth());
};

/**
 * @return {!boolean}
 */
Month.prototype.containsDay = function(day) {
    return this.year === day.year && this.month === day.month;
};

/**
 * @param {!Month} other
 * @return {!boolean}
 */
Month.prototype.equals = function(other) {
    return other instanceof Month && this.year === other.year && this.month === other.month;
};

/**
 * @param {!number=} offset
 * @return {!Month}
 */
Month.prototype.previous = function(offset) {
    if (typeof offset === "undefined")
        offset = 1;
    return new Month(this.year, this.month - offset);
};

/**
 * @param {!number=} offset
 * @return {!Month}
 */
Month.prototype.next = function(offset) {
    if (typeof offset === "undefined")
        offset = 1;
    return new Month(this.year, this.month + offset);
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
 * @return {!Day}
 */
Month.prototype.firstDay = function() {
    return new Day(this.year, this.month, 1);
};

/**
 * @return {!Day}
 */
Month.prototype.middleDay = function() {
    return new Day(this.year, this.month, this.month === 2 ? 14 : 15);
};

/**
 * @return {!Day}
 */
Month.prototype.lastDay = function() {
    if (this.equals(Month.Maximum))
        return Day.Maximum;
    return this.next().firstDay().previous();
};

/**
 * @return {!number}
 */
Month.prototype.valueOf = function() {
    return (this.year - 1970) * MonthsPerYear + this.month;
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

/**
 * @return {!string}
 */
Month.prototype.toLocaleString = function() {
    if (global.params.locale === "ja")
        return "" + this.year + "年" + formatJapaneseImperialEra(this.year, this.month) + " " + (this.month + 1) + "月";
    return window.pagePopupController.formatMonth(this.year, this.month);
};

/**
 * @return {!string}
 */
Month.prototype.toShortLocaleString = function() {
    return window.pagePopupController.formatShortMonth(this.year, this.month);
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
 */
function EventEmitter() {
};

/**
 * @param {!string} type
 * @param {!function({...*})} callback
 */
EventEmitter.prototype.on = function(type, callback) {
    console.assert(callback instanceof Function);
    if (!this._callbacks)
        this._callbacks = {};
    if (!this._callbacks[type])
        this._callbacks[type] = [];
    this._callbacks[type].push(callback);
};

EventEmitter.prototype.hasListener = function(type) {
    if (!this._callbacks)
        return false;
    var callbacksForType = this._callbacks[type];
    if (!callbacksForType)
        return false;
    return callbacksForType.length > 0;
};

/**
 * @param {!string} type
 * @param {!function(Object)} callback
 */
EventEmitter.prototype.removeListener = function(type, callback) {
    if (!this._callbacks)
        return;
    var callbacksForType = this._callbacks[type];
    if (!callbacksForType)
        return;
    callbacksForType.splice(callbacksForType.indexOf(callback), 1);
    if (callbacksForType.length === 0)
        delete this._callbacks[type];
};

/**
 * @param {!string} type
 * @param {...*} var_args
 */
EventEmitter.prototype.dispatchEvent = function(type) {
    if (!this._callbacks)
        return;
    var callbacksForType = this._callbacks[type];
    if (!callbacksForType)
        return;
    for (var i = 0; i < callbacksForType.length; ++i) {
        callbacksForType[i].apply(this, Array.prototype.slice.call(arguments, 1));
    }
};

// Parameter t should be a number between 0 and 1.
var AnimationTimingFunction = {
    Linear: function(t){
        return t;
    },
    EaseInOut: function(t){
        t *= 2;
        if (t < 1)
            return Math.pow(t, 3) / 2;
        t -= 2;
        return Math.pow(t, 3) / 2 + 1;
    }
};

/**
 * @constructor
 * @extends EventEmitter
 */
function AnimationManager() {
    EventEmitter.call(this);

    this._isRunning = false;
    this._runningAnimatorCount = 0;
    this._runningAnimators = {};
    this._animationFrameCallbackBound = this._animationFrameCallback.bind(this);
}

AnimationManager.prototype = Object.create(EventEmitter.prototype);

AnimationManager.EventTypeAnimationFrameWillFinish = "animationFrameWillFinish";

AnimationManager.prototype._startAnimation = function() {
    if (this._isRunning)
        return;
    this._isRunning = true;
    window.webkitRequestAnimationFrame(this._animationFrameCallbackBound);
};

AnimationManager.prototype._stopAnimation = function() {
    if (!this._isRunning)
        return;
    this._isRunning = false;
};

/**
 * @param {!Animator} animator
 */
AnimationManager.prototype.add = function(animator) {
    if (this._runningAnimators[animator.id])
        return;
    this._runningAnimators[animator.id] = animator;
    this._runningAnimatorCount++;
    if (this._needsTimer())
        this._startAnimation();
};

/**
 * @param {!Animator} animator
 */
AnimationManager.prototype.remove = function(animator) {
    if (!this._runningAnimators[animator.id])
        return;
    delete this._runningAnimators[animator.id];
    this._runningAnimatorCount--;
    if (!this._needsTimer())
        this._stopAnimation();
};

AnimationManager.prototype._animationFrameCallback = function(now) {
    if (this._runningAnimatorCount > 0) {
        for (var id in this._runningAnimators) {
            this._runningAnimators[id].onAnimationFrame(now);
        }
    }
    this.dispatchEvent(AnimationManager.EventTypeAnimationFrameWillFinish);
    if (this._isRunning)
        window.webkitRequestAnimationFrame(this._animationFrameCallbackBound);
};

/**
 * @return {!boolean}
 */
AnimationManager.prototype._needsTimer = function() {
    return this._runningAnimatorCount > 0 || this.hasListener(AnimationManager.EventTypeAnimationFrameWillFinish);
};

/**
 * @param {!string} type
 * @param {!Function} callback
 * @override
 */
AnimationManager.prototype.on = function(type, callback) {
    EventEmitter.prototype.on.call(this, type, callback);
    if (this._needsTimer())
        this._startAnimation();
};

/**
 * @param {!string} type
 * @param {!Function} callback
 * @override
 */
AnimationManager.prototype.removeListener = function(type, callback) {
    EventEmitter.prototype.removeListener.call(this, type, callback);
    if (!this._needsTimer())
        this._stopAnimation();
};

AnimationManager.shared = new AnimationManager();

/**
 * @constructor
 * @extends EventEmitter
 */
function Animator() {
    EventEmitter.call(this);

    this.id = Animator._lastId++;
    this._from = 0;
    this._to = 0;
    this._delta = 0;
    this.duration = 100;
    this.step = null;
    this._lastStepTime = null;
    this.progress = 0.0;
    this.timingFunction = AnimationTimingFunction.Linear;
}

Animator.prototype = Object.create(EventEmitter.prototype);

Animator._lastId = 0;

Animator.EventTypeDidAnimationStop = "didAnimationStop";

/**
 * @param {!number} value
 */
Animator.prototype.setFrom = function(value) {
    this._from = value;
    this._delta = this._to - this._from;
};

/**
 * @param {!number} value
 */
Animator.prototype.setTo = function(value) {
    this._to = value;
    this._delta = this._to - this._from;
};

Animator.prototype.start = function() {
    this._lastStepTime = Date.now();
    this.progress = 0.0;
    this._isRunning = true;
    this.currentValue = this._from;
    AnimationManager.shared.add(this);
};

Animator.prototype.stop = function() {
    if (!this._isRunning)
        return;
    this._isRunning = false;
    this.currentValue = this._to;
    this.step(this);
    AnimationManager.shared.remove(this);
    this.dispatchEvent(Animator.EventTypeDidAnimationStop, this);
};

/**
 * @param {!number} now
 */
Animator.prototype.onAnimationFrame = function(now) {
    this.progress += (now - this._lastStepTime) / this.duration;
    if (this.progress >= 1.0) {
        this.progress = 1.0;
        this.stop();
        return;
    }
    this.currentValue = this.timingFunction(this.progress) * this._delta + this._from;
    this.step(this);
    this._lastStepTime = now;
};

/**
 * @constructor
 * @extends EventEmitter
 * @param {?Element} element
 * View adds itself as a property on the element so we can access it from Event.target.
 */
function View(element) {
    EventEmitter.call(this);
    /**
     * @type {Element}
     * @const
     */
    this.element = element || createElement("div");
    this.element.$view = this;
    this.bindCallbackMethods();
}

View.prototype = Object.create(EventEmitter.prototype);

/**
 * @param {!Element} ancestorElement
 * @return {?Object}
 */
View.prototype.offsetRelativeTo = function(ancestorElement) {
    var x = 0;
    var y = 0;
    var element = this.element;
    while (element) {
        x += element.offsetLeft  || 0;
        y += element.offsetTop || 0;
        element = element.offsetParent;
        if (element === ancestorElement)
            return {x: x, y: y};
    }
    return null;
};

/**
 * @param {!View|Node} parent
 * @param {?View|Node=} before
 */
View.prototype.attachTo = function(parent, before) {
    if (parent instanceof View)
        return this.attachTo(parent.element, before);
    if (typeof before === "undefined")
        before = null;
    if (before instanceof View)
        before = before.element;
    parent.insertBefore(this.element, before);
};

View.prototype.bindCallbackMethods = function() {
    for (var methodName in this) {
        if (!/^on[A-Z]/.test(methodName))
            continue;
        if (this.hasOwnProperty(methodName))
            continue;
        var method = this[methodName];
        if (!(method instanceof Function))
            continue;
        this[methodName] = method.bind(this);
    }
};

/**
 * @constructor
 * @extends View
 */
function ScrollView() {
    View.call(this, createElement("div", ScrollView.ClassNameScrollView));
    /**
     * @type {Element}
     * @const
     */
    this.contentElement = createElement("div", ScrollView.ClassNameScrollViewContent);
    this.element.appendChild(this.contentElement);
    /**
     * @type {number}
     */
    this.minimumContentOffset = -Infinity;
    /**
     * @type {number}
     */
    this.maximumContentOffset = Infinity;
    /**
     * @type {number}
     * @protected
     */
    this._contentOffset = 0;
    /**
     * @type {number}
     * @protected
     */
    this._width = 0;
    /**
     * @type {number}
     * @protected
     */
    this._height = 0;
    /**
     * @type {Animator}
     * @protected
     */
    this._scrollAnimator = new Animator();
    this._scrollAnimator.step = this.onScrollAnimatorStep;

    /**
     * @type {?Object}
     */
    this.delegate = null;

    this.element.addEventListener("mousewheel", this.onMouseWheel, false);

    /**
     * The content offset is partitioned so the it can go beyond the CSS limit
     * of 33554433px.
     * @type {number}
     * @protected
     */
    this._partitionNumber = 0;
}

ScrollView.prototype = Object.create(View.prototype);

ScrollView.PartitionHeight = 100000;
ScrollView.ClassNameScrollView = "scroll-view";
ScrollView.ClassNameScrollViewContent = "scroll-view-content";

/**
 * @param {!number} width
 */
ScrollView.prototype.setWidth = function(width) {
    console.assert(isFinite(width));
    if (this._width === width)
        return;
    this._width = width;
    this.element.style.width = this._width + "px";
};

/**
 * @return {!number}
 */
ScrollView.prototype.width = function() {
    return this._width;
};

/**
 * @param {!number} height
 */
ScrollView.prototype.setHeight = function(height) {
    console.assert(isFinite(height));
    if (this._height === height)
        return;
    this._height = height;
    this.element.style.height = height + "px";
    if (this.delegate)
        this.delegate.scrollViewDidChangeHeight(this);
};

/**
 * @return {!number}
 */
ScrollView.prototype.height = function() {
    return this._height;
};

/**
 * @param {!Animator} animator
 */
ScrollView.prototype.onScrollAnimatorStep = function(animator) {
    this.setContentOffset(animator.currentValue);
};

/**
 * @param {!number} offset
 * @param {?boolean} animate
 */
ScrollView.prototype.scrollTo = function(offset, animate) {
    console.assert(isFinite(offset));
    if (!animate) {
        this.setContentOffset(offset);
        return;
    }
    this._scrollAnimator.setFrom(this._contentOffset);
    this._scrollAnimator.setTo(offset);
    this._scrollAnimator.duration = 300;
    this._scrollAnimator.start();
};

/**
 * @param {!number} offset
 * @param {?boolean} animate
 */
ScrollView.prototype.scrollBy = function(offset, animate) {
    this.scrollTo(this._contentOffset + offset, animate);
};

/**
 * @return {!number}
 */
ScrollView.prototype.contentOffset = function() {
    return this._contentOffset;
};

/**
 * @param {?Event} event
 */
ScrollView.prototype.onMouseWheel = function(event) {
    this.setContentOffset(this._contentOffset - event.wheelDelta / 30);
    event.stopPropagation();
    event.preventDefault();
};


/**
 * @param {!number} value
 */
ScrollView.prototype.setContentOffset = function(value) {
    console.assert(isFinite(value));
    value = Math.min(this.maximumContentOffset - this._height, Math.max(this.minimumContentOffset, Math.floor(value)));
    if (this._contentOffset === value)
        return;
    var newPartitionNumber = Math.floor(value / ScrollView.PartitionHeight);    
    var partitionChanged = this._partitionNumber !== newPartitionNumber;
    this._partitionNumber = newPartitionNumber;
    this._contentOffset = value;
    this.contentElement.style.webkitTransform = "translate(0, " + (-this.contentPositionForContentOffset(this._contentOffset)) + "px)";
    if (this.delegate) {
        this.delegate.scrollViewDidChangeContentOffset(this);
        if (partitionChanged)
            this.delegate.scrollViewDidChangePartition(this);
    }
};

/**
 * @param {!number} offset
 */
ScrollView.prototype.contentPositionForContentOffset = function(offset) {
    return offset - this._partitionNumber * ScrollView.PartitionHeight;
};

/**
 * @constructor
 * @extends View
 */
function ListCell() {
    View.call(this, createElement("div", ListCell.ClassNameListCell));
    
    /**
     * @type {!number}
     */
    this.row = NaN;
    /**
     * @type {!number}
     */
    this._width = 0;
    /**
     * @type {!number}
     */
    this._position = 0;
}

ListCell.prototype = Object.create(View.prototype);

ListCell.DefaultRecycleBinLimit = 64;
ListCell.ClassNameListCell = "list-cell";
ListCell.ClassNameHidden = "hidden";

/**
 * @return {!Array} An array to keep thrown away cells.
 */
ListCell.prototype._recycleBin = function() {
    console.assert(false, "NOT REACHED: ListCell.prototype._recycleBin needs to be overridden.");
    return [];
};

ListCell.prototype.throwAway = function() {
    this.hide();
    var limit = typeof this.constructor.RecycleBinLimit === "undefined" ? ListCell.DefaultRecycleBinLimit : this.constructor.RecycleBinLimit;
    var recycleBin = this._recycleBin();
    if (recycleBin.length < limit)
        recycleBin.push(this);
};

ListCell.prototype.show = function() {
    this.element.classList.remove(ListCell.ClassNameHidden);
};

ListCell.prototype.hide = function() {
    this.element.classList.add(ListCell.ClassNameHidden);
};

/**
 * @return {!number} Width in pixels.
 */
ListCell.prototype.width = function(){
    return this._width;
};

/**
 * @param {!number} width Width in pixels.
 */
ListCell.prototype.setWidth = function(width){
    if (this._width === width)
        return;
    this._width = width;
    this.element.style.width = this._width + "px";
};

/**
 * @return {!number} Position in pixels.
 */
ListCell.prototype.position = function(){
    return this._position;
};

/**
 * @param {!number} y Position in pixels.
 */
ListCell.prototype.setPosition = function(y) {
    if (this._position === y)
        return;
    this._position = y;
    this.element.style.webkitTransform = "translate(0, " + this._position + "px)";
};

/**
 * @param {!boolean} selected
 */
ListCell.prototype.setSelected = function(selected) {
    if (this._selected === selected)
        return;
    this._selected = selected;
    if (this._selected)
        this.element.classList.add("selected");
    else
        this.element.classList.remove("selected");
};

/**
 * @constructor
 * @extends View
 */
function ListView() {
    View.call(this, createElement("div", ListView.ClassNameListView));
    this.element.tabIndex = 0;

    /**
     * @type {!number}
     * @private
     */
    this._width = 0;
    /**
     * @type {!Object}
     * @private
     */
    this._cells = {};

    /**
     * @type {!number}
     */
    this.selectedRow = ListView.NoSelection;

    /**
     * @type {!ScrollView}
     */
    this.scrollView = new ScrollView();
    this.scrollView.delegate = this;
    this.scrollView.minimumContentOffset = 0;
    this.scrollView.setWidth(0);
    this.scrollView.setHeight(0);
    this.scrollView.attachTo(this);

    this.element.addEventListener("click", this.onClick, false);

    /**
     * @type {!boolean}
     * @private
     */
    this._needsUpdateCells = false;
}

ListView.prototype = Object.create(View.prototype);

ListView.NoSelection = -1;
ListView.ClassNameListView = "list-view";

ListView.prototype.onAnimationFrameWillFinish = function() {
    if (this._needsUpdateCells)
        this.updateCells();
};

/**
 * @param {!boolean} needsUpdateCells
 */
ListView.prototype.setNeedsUpdateCells = function(needsUpdateCells) {
    if (this._needsUpdateCells === needsUpdateCells)
        return;
    this._needsUpdateCells = needsUpdateCells;
    if (this._needsUpdateCells)
        AnimationManager.shared.on(AnimationManager.EventTypeAnimationFrameWillFinish, this.onAnimationFrameWillFinish);
    else
        AnimationManager.shared.removeListener(AnimationManager.EventTypeAnimationFrameWillFinish, this.onAnimationFrameWillFinish);
};

/**
 * @param {!number} row
 * @return {?ListCell}
 */
ListView.prototype.cellAtRow = function(row) {
    return this._cells[row];
};

/**
 * @param {!number} offset Scroll offset in pixels.
 * @return {!number}
 */
ListView.prototype.rowAtScrollOffset = function(offset) {
    console.assert(false, "NOT REACHED: ListView.prototype.rowAtScrollOffset needs to be overridden.");
    return 0;
};

/**
 * @param {!number} row
 * @return {!number} Scroll offset in pixels.
 */
ListView.prototype.scrollOffsetForRow = function(row) {
    console.assert(false, "NOT REACHED: ListView.prototype.scrollOffsetForRow needs to be overridden.");
    return 0;
};

/**
 * @param {!number} row
 * @return {!ListCell}
 */
ListView.prototype.addCellIfNecessary = function(row) {
    var cell = this._cells[row];
    if (cell)
        return cell;
    cell = this.prepareNewCell(row);
    cell.attachTo(this.scrollView.contentElement);
    cell.setWidth(this._width);
    cell.setPosition(this.scrollView.contentPositionForContentOffset(this.scrollOffsetForRow(row)));
    this._cells[row] = cell;
    return cell;
};

/**
 * @param {!number} row
 * @return {!ListCell}
 */
ListView.prototype.prepareNewCell = function(row) {
    console.assert(false, "NOT REACHED: ListView.prototype.prepareNewCell should be overridden.");
    return new ListCell();
};

/**
 * @param {!ListCell} cell
 */
ListView.prototype.throwAwayCell = function(cell) {
    delete this._cells[cell.row];
    cell.throwAway();
};

/**
 * @return {!number}
 */
ListView.prototype.firstVisibleRow = function() {
    return this.rowAtScrollOffset(this.scrollView.contentOffset());
};

/**
 * @return {!number}
 */
ListView.prototype.lastVisibleRow = function() {
    return this.rowAtScrollOffset(this.scrollView.contentOffset() + this.scrollView.height() - 1);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangeContentOffset = function(scrollView) {
    this.setNeedsUpdateCells(true);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangeHeight = function(scrollView) {
    this.setNeedsUpdateCells(true);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangePartition = function(scrollView) {
    this.setNeedsUpdateCells(true);
};

ListView.prototype.updateCells = function() {
    var firstVisibleRow = this.firstVisibleRow();
    var lastVisibleRow = this.lastVisibleRow();
    console.assert(firstVisibleRow <= lastVisibleRow);
    for (var c in this._cells) {
        var cell = this._cells[c];
        if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
            this.throwAwayCell(cell);
    }
    for (var i = firstVisibleRow; i <= lastVisibleRow; ++i) {
        var cell = this._cells[i];
        if (cell)
            cell.setPosition(this.scrollView.contentPositionForContentOffset(this.scrollOffsetForRow(cell.row)));
        else
            this.addCellIfNecessary(i);
    }
    this.setNeedsUpdateCells(false);
};

/**
 * @return {!number} Width in pixels.
 */
ListView.prototype.width = function() {
    return this._width;
};

/**
 * @param {!number} width Width in pixels.
 */
ListView.prototype.setWidth = function(width) {
    if (this._width === width)
        return;
    this._width = width;
    this.scrollView.setWidth(this._width);
    for (var c in this._cells) {
        this._cells[c].setWidth(this._width);
    }
    this.element.style.width = this._width + "px";
    this.setNeedsUpdateCells(true);
};

/**
 * @return {!number} Height in pixels.
 */
ListView.prototype.height = function() {
    return this.scrollView.height();
};

/**
 * @param {!number} height Height in pixels.
 */
ListView.prototype.setHeight = function(height) {
    this.scrollView.setHeight(height);
};

/**
 * @param {?Event} event
 */
ListView.prototype.onClick = function(event) {
    var clickedCellElement = enclosingNodeOrSelfWithClass(event.target, ListCell.ClassNameListCell);
    if (!clickedCellElement)
        return;
    var clickedCell = clickedCellElement.$view;
    if (clickedCell.row !== this.selectedRow)
        this.select(clickedCell.row);
};

/**
 * @param {!number} row
 */
ListView.prototype.select = function(row) {
    if (this.selectedRow === row)
        return;
    this.deselect();
    if (row === ListView.NoSelection)
        return;
    this.selectedRow = row;
    var selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
        selectedCell.setSelected(true);
};

ListView.prototype.deselect = function() {
    if (this.selectedRow === ListView.NoSelection)
        return;
    var selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
        selectedCell.setSelected(false);
    this.selectedRow = ListView.NoSelection;
};

/**
 * @param {!number} row
 * @param {!boolean} animate
 */
ListView.prototype.scrollToRow = function(row, animate) {
    this.scrollView.scrollTo(this.scrollOffsetForRow(row), animate);
};

/**
 * @constructor
 * @extends View
 * @param {!ScrollView} scrollView
 */
function ScrubbyScrollBar(scrollView) {
    View.call(this, createElement("div", ScrubbyScrollBar.ClassNameScrubbyScrollBar));

    /**
     * @type {!Element}
     * @const
     */
    this.thumb = createElement("div", ScrubbyScrollBar.ClassNameScrubbyScrollThumb);
    this.element.appendChild(this.thumb);

    /**
     * @type {!ScrollView}
     * @const
     */
    this.scrollView = scrollView;

    /**
     * @type {!number}
     * @protected
     */
    this._height = 0;
    /**
     * @type {!number}
     * @protected
     */
    this._thumbHeight = 0;
    /**
     * @type {!number}
     * @protected
     */
    this._thumbPosition = 0;

    this.setHeight(0);
    this.setThumbHeight(ScrubbyScrollBar.ThumbHeight);

    /**
     * @type {?Animator}
     * @protected
     */
    this._thumbStyleTopAnimator = null;

    /** 
     * @type {?number}
     * @protected
     */
    this._timer = null;
    
    this.element.addEventListener("mousedown", this.onMouseDown, false);
}

ScrubbyScrollBar.prototype = Object.create(View.prototype);

ScrubbyScrollBar.ScrollInterval = 16;
ScrubbyScrollBar.ThumbMargin = 2;
ScrubbyScrollBar.ThumbHeight = 30;
ScrubbyScrollBar.ClassNameScrubbyScrollBar = "scrubby-scroll-bar";
ScrubbyScrollBar.ClassNameScrubbyScrollThumb = "scrubby-scroll-thumb";

/**
 * @return {!number} Height of the view in pixels.
 */
ScrubbyScrollBar.prototype.height = function() {
    return this._height;
};

/**
 * @param {!number} height Height of the view in pixels.
 */
ScrubbyScrollBar.prototype.setHeight = function(height) {
    if (this._height === height)
        return;
    this._height = height;
    this.element.style.height = this._height + "px";
    this.thumb.style.top = ((this._height - this._thumbHeight) / 2) + "px";
    this._thumbPosition = 0;
};

/**
 * @param {!number} height Height of the scroll bar thumb in pixels.
 */
ScrubbyScrollBar.prototype.setThumbHeight = function(height) {
    if (this._thumbHeight === height)
        return;
    this._thumbHeight = height;
    this.thumb.style.height = this._thumbHeight + "px";
    this.thumb.style.top = ((this._height - this._thumbHeight) / 2) + "px";
    this._thumbPosition = 0;
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype._setThumbPositionFromEvent = function(event) {
    var thumbMin = ScrubbyScrollBar.ThumbMargin;
    var thumbMax = this._height - this._thumbHeight - ScrubbyScrollBar.ThumbMargin * 2;
    var y = event.clientY - this.element.getBoundingClientRect().top - this.element.clientTop + this.element.scrollTop;
    var thumbPosition = y - this._thumbHeight / 2;
    thumbPosition = Math.max(thumbPosition, thumbMin);
    thumbPosition = Math.min(thumbPosition, thumbMax);
    this.thumb.style.top = thumbPosition + "px";
    this._thumbPosition = 1.0 - (thumbPosition - thumbMin) / (thumbMax - thumbMin) * 2;
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onMouseDown = function(event) {
    this._setThumbPositionFromEvent(event);

    window.addEventListener("mousemove", this.onWindowMouseMove, false);
    window.addEventListener("mouseup", this.onWindowMouseUp, false);
    if (this._thumbStyleTopAnimator)
        this._thumbStyleTopAnimator.stop();
    this._timer = setInterval(this.onScrollTimer, ScrubbyScrollBar.ScrollInterval);
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowMouseMove = function(event) {
    this._setThumbPositionFromEvent(event);
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowMouseUp = function(event) {
    this._thumbStyleTopAnimator = new Animator();
    this._thumbStyleTopAnimator.step = this.onThumbStyleTopAnimationStep;
    this._thumbStyleTopAnimator.setFrom(this.thumb.offsetTop);
    this._thumbStyleTopAnimator.setTo((this._height - this._thumbHeight) / 2);
    this._thumbStyleTopAnimator.timingFunction = AnimationTimingFunction.EaseInOut;
    this._thumbStyleTopAnimator.duration = 100;
    this._thumbStyleTopAnimator.start();
    
    window.removeEventListener("mousemove", this.onWindowMouseMove, false);
    window.removeEventListener("mouseup", this.onWindowMouseUp, false);
    clearInterval(this._timer);
};

/**
 * @param {!Animator} animator
 */
ScrubbyScrollBar.prototype.onThumbStyleTopAnimationStep = function(animator) {
    this.thumb.style.top = animator.currentValue + "px";
};

ScrubbyScrollBar.prototype.onScrollTimer = function() {
    var scrollAmount = Math.pow(this._thumbPosition, 2) * 10;
    if (this._thumbPosition > 0)
        scrollAmount = -scrollAmount;
    this.scrollView.scrollBy(scrollAmount, false);
};

/**
 * @constructor
 * @extends ListCell
 * @param {!Array} shortMonthLabels
 */
function YearListCell(shortMonthLabels) {
    ListCell.call(this);
    this.element.classList.add(YearListCell.ClassNameYearListCell);
    this.element.style.height = YearListCell.Height + "px";

    /**
     * @type {!Element}
     * @const
     */
    this.label = createElement("div", YearListCell.ClassNameLabel, "----");
    this.element.appendChild(this.label);

    /**
     * @type {!Array} Array of the 12 month button elements.
     * @const
     */
    this.monthButtons = [];
    var monthChooserElement = createElement("div", YearListCell.ClassNameMonthChooser);
    for (var r = 0; r < YearListCell.ButtonRows; ++r) {
        var buttonsRow = createElement("div", YearListCell.ClassNameMonthButtonsRow);
        for (var c = 0; c < YearListCell.ButtonColumns; ++c) {
            var month = c + r * YearListCell.ButtonColumns;
            var button = createElement("button", YearListCell.ClassNameMonthButton, shortMonthLabels[month]);
            button.dataset.month = month;
            buttonsRow.appendChild(button);
            this.monthButtons.push(button);
        }
        monthChooserElement.appendChild(buttonsRow);
    }
    this.element.appendChild(monthChooserElement);

    /**
     * @type {!boolean}
     * @private
     */
    this._selected = false;
    /**
     * @type {!number}
     * @private
     */
    this._height = 0;
}

YearListCell.prototype = Object.create(ListCell.prototype);

YearListCell.Height = 25;
YearListCell.ButtonRows = 3;
YearListCell.ButtonColumns = 4;
YearListCell.SelectedHeight = 121;
YearListCell.ClassNameYearListCell = "year-list-cell";
YearListCell.ClassNameLabel = "label";
YearListCell.ClassNameMonthChooser = "month-chooser";
YearListCell.ClassNameMonthButtonsRow = "month-buttons-row";
YearListCell.ClassNameMonthButton = "month-button";
YearListCell.ClassNameHighlighted = "highlighted";

YearListCell._recycleBin = [];

/**
 * @return {!Array}
 * @override
 */
YearListCell.prototype._recycleBin = function() {
    return YearListCell._recycleBin;
};

/**
 * @param {!number} row
 */
YearListCell.prototype.reset = function(row) {
    this.row = row;
    this.label.textContent = row + 1;
    for (var i = 0; i < this.monthButtons.length; ++i) {
        this.monthButtons[i].classList.remove(YearListCell.ClassNameHighlighted);
    }
    this.show();
};

/**
 * @return {!number} The height in pixels.
 */
YearListCell.prototype.height = function() {
    return this._height;
};

/**
 * @param {!number} height Height in pixels.
 */
YearListCell.prototype.setHeight = function(height) {
    if (this._height === height)
        return;
    this._height = height;
    this.element.style.height = this._height + "px";
};

/**
 * @constructor
 * @extends ListView
 * @param {!Month} minimumMonth
 * @param {!Month} maximumMonth
 */
function YearListView(minimumMonth, maximumMonth) {
    ListView.call(this);
    this.element.classList.add("year-list-view");

    /**
     * @type {?Month}
     */
    this.highlightedMonth = null;
    /**
     * @type {!Month}
     * @const
     * @protected
     */
    this._minimumMonth = minimumMonth;
    /**
     * @type {!Month}
     * @const
     * @protected
     */
    this._maximumMonth = maximumMonth;

    this.scrollView.minimumContentOffset = (this._minimumMonth.year - 1) * YearListCell.Height;
    this.scrollView.maximumContentOffset = (this._maximumMonth.year - 1) * YearListCell.Height + YearListCell.SelectedHeight;
    
    /**
     * @type {!Object}
     * @const
     * @protected
     */
    this._runningAnimators = {};
    /**
     * @type {!Array}
     * @const
     * @protected
     */
    this._animatingRows = [];
    /**
     * @type {!boolean}
     * @protected
     */
    this._ignoreMouseOutUntillNextMouseOver = false;
    
    /**
     * @type {!ScrubbyScrollBar}
     * @const
     */
    this.scrubbyScrollBar = new ScrubbyScrollBar(this.scrollView);
    this.scrubbyScrollBar.attachTo(this);
    
    this.element.addEventListener("mouseover", this.onMouseOver, false);
    this.element.addEventListener("mouseout", this.onMouseOut, false);
    this.element.addEventListener("keydown", this.onKeyDown, false);
}

YearListView.prototype = Object.create(ListView.prototype);

YearListView.Height = YearListCell.SelectedHeight - 1;
YearListView.EventTypeYearListViewDidHide = "yearListViewDidHide";
YearListView.EventTypeYearListViewDidSelectMonth = "yearListViewDidSelectMonth";

/**
 * @param {?Event} event
 */
YearListView.prototype.onMouseOver = function(event) {
    var monthButtonElement = enclosingNodeOrSelfWithClass(event.target, YearListCell.ClassNameMonthButton);
    if (!monthButtonElement)
        return;
    var cellElement = enclosingNodeOrSelfWithClass(monthButtonElement, YearListCell.ClassNameYearListCell);
    var cell = cellElement.$view;
    this.highlightMonth(new Month(cell.row + 1, parseInt(monthButtonElement.dataset.month, 10)));
    this._ignoreMouseOutUntillNextMouseOver = false;
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onMouseOut = function(event) {
    if (this._ignoreMouseOutUntillNextMouseOver)
        return;
    var monthButtonElement = enclosingNodeOrSelfWithClass(event.target, YearListCell.ClassNameMonthButton);
    if (!monthButtonElement) {
        this.dehighlightMonth();
    }
};

/**
 * @param {!number} width Width in pixels.
 * @override
 */
YearListView.prototype.setWidth = function(width) {
    ListView.prototype.setWidth.call(this, width - this.scrubbyScrollBar.element.offsetWidth);
    this.element.style.width = width + "px";
};

/**
 * @param {!number} height Height in pixels.
 * @override
 */
YearListView.prototype.setHeight = function(height) {
    ListView.prototype.setHeight.call(this, height);
    this.scrubbyScrollBar.setHeight(height);
};

/**
 * @enum {number}
 */
YearListView.RowAnimationDirection = {
    Opening: 0,
    Closing: 1
};

/**
 * @param {!number} row
 * @param {!YearListView.RowAnimationDirection} direction
 */
YearListView.prototype._animateRow = function(row, direction) {
    var fromValue = direction === YearListView.RowAnimationDirection.Closing ? YearListCell.SelectedHeight : YearListCell.Height;
    var oldAnimator = this._runningAnimators[row];
    if (oldAnimator) {
        oldAnimator.stop();
        fromValue = oldAnimator.currentValue;
    }
    var cell = this.cellAtRow(row);
    var animator = new Animator();
    animator.step = this.onCellHeightAnimatorStep;
    animator.setFrom(fromValue);
    animator.setTo(direction === YearListView.RowAnimationDirection.Opening ? YearListCell.SelectedHeight : YearListCell.Height);
    animator.timingFunction = AnimationTimingFunction.EaseInOut;
    animator.duration = 300;
    animator.row = row;
    animator.on(Animator.EventTypeDidAnimationStop, this.onCellHeightAnimatorDidStop);
    this._runningAnimators[row] = animator;
    this._animatingRows.push(row);
    this._animatingRows.sort();
    animator.start();
};

/**
 * @param {?Animator} animator
 */
YearListView.prototype.onCellHeightAnimatorDidStop = function(animator) {
    delete this._runningAnimators[animator.row];
    var index = this._animatingRows.indexOf(animator.row);
    this._animatingRows.splice(index, 1);
};

/**
 * @param {!Animator} animator
 */
YearListView.prototype.onCellHeightAnimatorStep = function(animator) {
    var cell = this.cellAtRow(animator.row);
    if (cell)
        cell.setHeight(animator.currentValue);
    this.updateCells();
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onClick = function(event) {
    var oldSelectedRow = this.selectedRow;
    ListView.prototype.onClick.call(this, event);
    var year = this.selectedRow + 1;
    if (this.selectedRow !== oldSelectedRow) {
        var month = this.highlightedMonth ? this.highlightedMonth.month : 0;
        this.dispatchEvent(YearListView.EventTypeYearListViewDidSelectMonth, this, new Month(year, month));
        this.scrollView.scrollTo(this.selectedRow * YearListCell.Height, true);
    } else {
        var monthButton = enclosingNodeOrSelfWithClass(event.target, YearListCell.ClassNameMonthButton);
        if (!monthButton)
            return;
        var month = parseInt(monthButton.dataset.month, 10);
        this.dispatchEvent(YearListView.EventTypeYearListViewDidSelectMonth, this, new Month(year, month));
        this.hide();
    }
};

/**
 * @param {!number} scrollOffset
 * @return {!number}
 * @override
 */
YearListView.prototype.rowAtScrollOffset = function(scrollOffset) {
    var remainingOffset = scrollOffset;
    var lastAnimatingRow = 0;
    var rowsWithIrregularHeight = this._animatingRows.slice();
    if (this.selectedRow > -1 && !this._runningAnimators[this.selectedRow]) {
        rowsWithIrregularHeight.push(this.selectedRow);
        rowsWithIrregularHeight.sort();
    }
    for (var i = 0; i < rowsWithIrregularHeight.length; ++i) {
        var row = rowsWithIrregularHeight[i];
        var animator = this._runningAnimators[row];
        var rowHeight = animator ? animator.currentValue : YearListCell.SelectedHeight;
        if (remainingOffset <= (row - lastAnimatingRow) * YearListCell.Height) {
            return lastAnimatingRow + Math.floor(remainingOffset / YearListCell.Height);
        }
        remainingOffset -= (row - lastAnimatingRow) * YearListCell.Height;
        if (remainingOffset <= (rowHeight - YearListCell.Height))
            return row;
        remainingOffset -= rowHeight - YearListCell.Height;
        lastAnimatingRow = row;
    }
    return lastAnimatingRow + Math.floor(remainingOffset / YearListCell.Height);
};

/**
 * @param {!number} row
 * @return {!number}
 * @override
 */
YearListView.prototype.scrollOffsetForRow = function(row) {
    var scrollOffset = row * YearListCell.Height;
    for (var i = 0; i < this._animatingRows.length; ++i) {
        var animatingRow = this._animatingRows[i];
        if (animatingRow >= row)
            break;
        var animator = this._runningAnimators[animatingRow];
        scrollOffset += animator.currentValue - YearListCell.Height;
    }
    if (this.selectedRow > -1 && this.selectedRow < row && !this._runningAnimators[this.selectedRow]) {
        scrollOffset += YearListCell.SelectedHeight - YearListCell.Height;
    }
    return scrollOffset;
};

/**
 * @param {!number} row
 * @return {!YearListCell}
 * @override
 */
YearListView.prototype.prepareNewCell = function(row) {
    var cell = YearListCell._recycleBin.pop() || new YearListCell(global.params.shortMonthLabels);
    cell.reset(row);
    cell.setSelected(this.selectedRow === row);
    if (this.highlightedMonth && row === this.highlightedMonth.year - 1) {
        cell.monthButtons[this.highlightedMonth.month].classList.add(YearListCell.ClassNameHighlighted);
    }
    for (var i = 0; i < cell.monthButtons.length; ++i) {
        var month = new Month(row + 1, i);
        cell.monthButtons[i].disabled = this._minimumMonth > month || this._maximumMonth < month;
    }
    var animator = this._runningAnimators[row];
    if (animator)
        cell.setHeight(animator.currentValue);
    else if (row === this.selectedRow)
        cell.setHeight(YearListCell.SelectedHeight);
    else
        cell.setHeight(YearListCell.Height);
    return cell;
};

/**
 * @override
 */
YearListView.prototype.updateCells = function() {
    var firstVisibleRow = this.firstVisibleRow();
    var lastVisibleRow = this.lastVisibleRow();
    console.assert(firstVisibleRow <= lastVisibleRow);
    for (var c in this._cells) {
        var cell = this._cells[c];
        if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
            this.throwAwayCell(cell);
    }
    for (var i = firstVisibleRow; i <= lastVisibleRow; ++i) {
        var cell = this._cells[i];
        if (cell)
            cell.setPosition(this.scrollView.contentPositionForContentOffset(this.scrollOffsetForRow(cell.row)));
        else
            this.addCellIfNecessary(i);
    }
    this.setNeedsUpdateCells(false);
};

/**
 * @override
 */
YearListView.prototype.deselect = function() {
    if (this.selectedRow === ListView.NoSelection)
        return;
    var selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
        selectedCell.setSelected(false);
    this._animateRow(this.selectedRow, YearListView.RowAnimationDirection.Closing);
    this.selectedRow = ListView.NoSelection;
    this.setNeedsUpdateCells(true);
};

YearListView.prototype.deselectWithoutAnimating = function() {
    if (this.selectedRow === ListView.NoSelection)
        return;
    var selectedCell = this._cells[this.selectedRow];
    if (selectedCell) {
        selectedCell.setSelected(false);
        selectedCell.setHeight(YearListCell.Height);
    }
    this.selectedRow = ListView.NoSelection;
    this.setNeedsUpdateCells(true);
};

/**
 * @param {!number} row
 * @override
 */
YearListView.prototype.select = function(row) {
    if (this.selectedRow === row)
        return;
    this.deselect();
    if (row === ListView.NoSelection)
        return;
    this.selectedRow = row;
    if (this.selectedRow !== ListView.NoSelection) {
        var selectedCell = this._cells[this.selectedRow];
        this._animateRow(this.selectedRow, YearListView.RowAnimationDirection.Opening);
        if (selectedCell)
            selectedCell.setSelected(true);
        var month = this.highlightedMonth ? this.highlightedMonth.month : 0;
        this.highlightMonth(new Month(this.selectedRow + 1, month));
    }
    this.setNeedsUpdateCells(true);
};

/**
 * @param {!number} row
 */
YearListView.prototype.selectWithoutAnimating = function(row) {
    if (this.selectedRow === row)
        return;
    this.deselectWithoutAnimating();
    if (row === ListView.NoSelection)
        return;
    this.selectedRow = row;
    if (this.selectedRow !== ListView.NoSelection) {
        var selectedCell = this._cells[this.selectedRow];
        if (selectedCell) {
            selectedCell.setSelected(true);
            selectedCell.setHeight(YearListCell.SelectedHeight);
        }
        var month = this.highlightedMonth ? this.highlightedMonth.month : 0;
        this.highlightMonth(new Month(this.selectedRow + 1, month));
    }
    this.setNeedsUpdateCells(true);
};

/**
 * @param {!Month} month
 * @return {?HTMLButtonElement}
 */
YearListView.prototype.buttonForMonth = function(month) {
    if (!month)
        return null;
    var row = month.year - 1;
    var cell = this.cellAtRow(row);
    if (!cell)
        return null;
    return cell.monthButtons[month.month];
};

YearListView.prototype.dehighlightMonth = function() {
    if (!this.highlightedMonth)
        return;
    var monthButton = this.buttonForMonth(this.highlightedMonth);
    if (monthButton) {
        monthButton.classList.remove(YearListCell.ClassNameHighlighted);
    }
    this.highlightedMonth = null;
};

/**
 * @param {!Month} month
 */
YearListView.prototype.highlightMonth = function(month) {
    if (this.highlightedMonth && this.highlightedMonth.equals(month))
        return;
    this.dehighlightMonth();
    this.highlightedMonth = month;
    if (!this.highlightedMonth)
        return;
    var monthButton = this.buttonForMonth(this.highlightedMonth);
    if (monthButton) {
        monthButton.classList.add(YearListCell.ClassNameHighlighted);
    }
};

/**
 * @param {!Month} month
 */
YearListView.prototype.show = function(month) {
    this._ignoreMouseOutUntillNextMouseOver = true;
    
    this.scrollToRow(month.year - 1, false);
    this.selectWithoutAnimating(month.year - 1);
    this.highlightMonth(month);
};

YearListView.prototype.hide = function() {
    this.dispatchEvent(YearListView.EventTypeYearListViewDidHide, this);
};

/**
 * @param {!Month} month
 */
YearListView.prototype._moveHighlightTo = function(month) {
    this.highlightMonth(month);
    this.select(this.highlightedMonth.year - 1);

    this.dispatchEvent(YearListView.EventTypeYearListViewDidSelectMonth, this, month);
    this.scrollView.scrollTo(this.selectedRow * YearListCell.Height, true);
    return true;
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onKeyDown = function(event) {
    var key = event.keyIdentifier;
    var eventHandled = false;
    if (key == "U+0054") // 't' key.
        eventHandled = this._moveHighlightTo(Month.createFromToday());
    else if (this.highlightedMonth) {
        if (global.params.isLocaleRTL ? key == "Right" : key == "Left")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.previous());
        else if (key == "Up")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.previous(YearListCell.ButtonColumns));
        else if (global.params.isLocaleRTL ? key == "Left" : key == "Right")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.next());
        else if (key == "Down")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.next(YearListCell.ButtonColumns));
        else if (key == "PageUp")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.previous(MonthsPerYear));
        else if (key == "PageDown")
            eventHandled = this._moveHighlightTo(this.highlightedMonth.next(MonthsPerYear));
        else if (key == "Enter") {
            this.dispatchEvent(YearListView.EventTypeYearListViewDidSelectMonth, this, this.highlightedMonth);
            this.hide();
            eventHandled = true;
        }
    } else if (key == "Up") {
        this.scrollView.scrollBy(-YearListCell.Height, true);
        eventHandled = true;
    } else if (key == "Down") {
        this.scrollView.scrollBy(YearListCell.Height, true);
        eventHandled = true;
    } else if (key == "PageUp") {
        this.scrollView.scrollBy(-this.scrollView.height(), true);
        eventHandled = true;
    } else if (key == "PageDown") {
        this.scrollView.scrollBy(this.scrollView.height(), true);
        eventHandled = true;
    }

    if (eventHandled) {
        event.stopPropagation();
        event.preventDefault();
    }
};

/**
 * @constructor
 * @extends View
 * @param {!Month} minimumMonth
 * @param {!Month} maximumMonth
 */
function MonthPopupView(minimumMonth, maximumMonth) {
    View.call(this, createElement("div", MonthPopupView.ClassNameMonthPopupView));

    /**
     * @type {!YearListView}
     * @const
     */
    this.yearListView = new YearListView(minimumMonth, maximumMonth);
    this.yearListView.attachTo(this);

    /**
     * @type {!boolean}
     */
    this.isVisible = false;

    this.element.addEventListener("click", this.onClick, false);
}

MonthPopupView.prototype = Object.create(View.prototype);

MonthPopupView.ClassNameMonthPopupView = "month-popup-view";

MonthPopupView.prototype.show = function(initialMonth, calendarTableRect) {
    this.isVisible = true;
    document.body.appendChild(this.element);
    this.yearListView.setWidth(calendarTableRect.width - 2);
    this.yearListView.setHeight(YearListView.Height);
    if (global.params.isLocaleRTL)
        this.yearListView.element.style.right = calendarTableRect.x + "px";
    else
        this.yearListView.element.style.left = calendarTableRect.x + "px";
    this.yearListView.element.style.top = calendarTableRect.y + "px";
    this.yearListView.show(initialMonth);
    this.yearListView.element.focus();
};

MonthPopupView.prototype.hide = function() {
    if (!this.isVisible)
        return;
    this.isVisible = false;
    this.element.parentNode.removeChild(this.element);
    this.yearListView.hide();
};

/**
 * @param {?Event} event
 */
MonthPopupView.prototype.onClick = function(event) {
    if (event.target !== this.element)
        return;
    this.hide();
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
        initialSelection = new this.selectionConstructor.createFromValue(this._minimumValue);
    else if (initialSelection.valueOf() > this._maximumValue)
        initialSelection = new this.selectionConstructor.createFromValue(this._maximumValue);
    this.showMonth(Month.createFromDate(initialSelection.startDate()));
    this._daysTable.selectRangeAndShowEntireRange(initialSelection);
    this.fixWindowSize();
    this._handleBodyKeyDownBound = this._handleBodyKeyDown.bind(this);
    document.body.addEventListener("keydown", this._handleBodyKeyDownBound, false);
    this.recordAction(CalendarPicker.Action.Opened);
}
CalendarPicker.prototype = Object.create(Picker.prototype);

CalendarPicker.NavigationBehaviour = {
    None: 0,
    Animate: 1 << 0,
    KeepSelectionPosition: 1 << 1
};

CalendarPicker.Action = {
    Opened:                     0,
    ClickedTodayButton:         1,
    ClickedClearButton:         2,
    ClickedDay:                 3,
    ClickedForwardMonthButton:  4,
    ClickedForwardYearButton:   5,
    ClickedBackwardMonthButton: 6,
    ClickedBackwardYearButton:  7,
    OpenedMonthPopup:           8,
    UsedMonthPopup:             9
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

CalendarPicker.prototype.recordAction = function(action) {
    window.pagePopupController.histogramEnumeration("CalendarPicker.Action", action, Object.keys(CalendarPicker.Action).length - 1);
};

CalendarPicker.prototype.handleToday = function() {
    this.recordAction(CalendarPicker.Action.ClickedTodayButton);
    var today = this.selectionConstructor.createFromToday();
    this._daysTable.selectRangeAndShowEntireRange(today);
    this.submitValue(today.toString());
};

CalendarPicker.prototype.handleClear = function() {
    this.recordAction(CalendarPicker.Action.ClickedClearButton);
    this.submitValue("");
};

CalendarPicker.prototype.fixWindowSize = function() {
    var yearMonthRightElement = this._element.getElementsByClassName(ClassNames.YearMonthButtonRight)[0];
    var daysAreaElement = this._element.getElementsByClassName(ClassNames.DaysArea)[0];
    var todayClearArea = this._element.getElementsByClassName(ClassNames.TodayClearArea)[0];
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
    var todayClearAreaEnd;
    if (global.params.isLocaleRTL) {
        var startOffset = this._element.offsetLeft + this._element.offsetWidth;
        yearMonthEnd = startOffset - yearMonthRightElement.offsetLeft;
        daysAreaEnd = startOffset - (daysAreaElement.offsetLeft + daysAreaElement.offsetWidth) + weekColumnWidth + maxCellWidth * 7 + DaysAreaContainerBorder;
        todayClearAreaEnd = startOffset - todayClearArea.offsetLeft;
    } else {
        yearMonthEnd = yearMonthRightElement.offsetLeft + yearMonthRightElement.offsetWidth;
        daysAreaEnd = daysAreaElement.offsetLeft + weekColumnWidth + maxCellWidth * 7 + DaysAreaContainerBorder;
        todayClearAreaEnd = todayClearArea.offsetLeft + todayClearArea.offsetWidth;
    }
    var maxEnd = Math.max(yearMonthEnd, daysAreaEnd, todayClearAreaEnd);
    var MainPadding = 10; // FIXME: Fix name.
    var MainBorder = 1;
    var desiredBodyWidth = maxEnd + MainPadding + MainBorder * 2;

    var elementHeight = this._element.offsetHeight;
    this._element.style.width = "auto";
    daysAreaElement.style.width = "100%";
    daysAreaElement.style.tableLayout = "fixed";
    todayClearArea.style.display = "block";
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

YearMonthController.LeftPointingTriangle = "<svg width='4' height='7'><polygon points='0,3.5 4,7 4,0' style='fill:#6e6e6e;' /></svg>";
YearMonthController.LeftPointingDoubleTriangle = "<svg width='9' height='7'><polygon points='0,3.5 4,7 4,0' style='fill:#6e6e6e;' /><polygon points='5,3.5 9,7 9,0' style='fill:#6e6e6e;' /></svg>";
YearMonthController.RightPointingTriangle = "<svg width='4' height='7'><polygon points='0,7 0,0, 4,3.5' style='fill:#6e6e6e;' /></svg>";
YearMonthController.RightPointingDoubleTriangle = "<svg width='9' height='7'><polygon points='4,3.5 0,7 0,0' style='fill:#6e6e6e;' /><polygon points='9,3.5 5,7 5,0' style='fill:#6e6e6e;' /></svg>";

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
    this._monthLabel = createElement("span");
    this._month.appendChild(this._monthLabel);
    var disclosureTriangle = createElement("span");
    disclosureTriangle.innerHTML = "<svg width='7' height='5'><polygon points='0,1 7,1 3.5,5' style='fill:#000000;' /></svg>";
    this._month.appendChild(disclosureTriangle);
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
        this._monthLabel.textContent = month.toLocaleString();
        maxWidth = Math.max(maxWidth, this._month.offsetWidth);
        month = month.previous();
    }
    if (getLanguage() == "ja" && ImperialEraLimit < this.picker.maximumMonth.year) {
        for (var m = 0; m < 12; ++m) {
            this._monthLabel.textContent = new Month(ImperialEraLimit, m).toLocaleString();
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
        this._left3 = createElement("button", ClassNames.YearMonthButton);
        this._left3.textContent = "<<<";
        this._left3.addEventListener("click", this._handleButtonClick.bind(this), false);
        container.appendChild(this._left3);
    }

    this._left2 = createElement("button", ClassNames.YearMonthButton);
    this._left2.innerHTML = global.params.isLocaleRTL ? YearMonthController.RightPointingDoubleTriangle : YearMonthController.LeftPointingDoubleTriangle;
    this._left2.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._left2);

    this._left1 = createElement("button", ClassNames.YearMonthButton);
    this._left1.innerHTML = global.params.isLocaleRTL ? YearMonthController.RightPointingTriangle : YearMonthController.LeftPointingTriangle;
    this._left1.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._left1);
};

/**
 * @param {!Element} parent
 */
YearMonthController.prototype._attachRightButtonsTo = function(parent) {
    var container = createElement("div", ClassNames.YearMonthButtonRight);
    parent.appendChild(container);
    this._right1 = createElement("button", ClassNames.YearMonthButton);
    this._right1.innerHTML = global.params.isLocaleRTL ? YearMonthController.LeftPointingTriangle : YearMonthController.RightPointingTriangle;
    this._right1.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._right1);

    this._right2 = createElement("button", ClassNames.YearMonthButton);
    this._right2.innerHTML = global.params.isLocaleRTL ? YearMonthController.LeftPointingDoubleTriangle : YearMonthController.RightPointingDoubleTriangle;
    this._right2.addEventListener("click", this._handleButtonClick.bind(this), false);
    container.appendChild(this._right2);

    if (YearMonthController.addTenYearsButtons) {
        this._right3 = createElement("button", ClassNames.YearMonthButton);
        this._right3.textContent = ">>>";
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
        this._left3.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue - 13));
    this._left2.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue - 2));
    this._left1.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue - 1));
    this._right1.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue + 1));
    this._right2.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue + 2));
    if (this._right3)
        this._left3.disabled = !this.picker.shouldShowMonth(Month.createFromValue(monthValue + 13));
    this._monthLabel.innerText = month.toLocaleString();
    while (this._monthPopupContents.hasChildNodes())
        this._monthPopupContents.removeChild(this._monthPopupContents.firstChild);

    for (var m = monthValue - 6; m <= monthValue + 6; m++) {
        var month = Month.createFromValue(m);
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
    this.picker.recordAction(CalendarPicker.Action.OpenedMonthPopup);
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
    this.picker.recordAction(CalendarPicker.Action.UsedMonthPopup);
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
    var button = enclosingNodeOrSelfWithClass(event.target, ClassNames.YearMonthButton);
    if (button == this._left3)
        this.moveRelatively(YearMonthController.PreviousTenYears);
    else if (button == this._left2) {
        this.picker.recordAction(CalendarPicker.Action.ClickedBackwardYearButton);
        this.moveRelatively(YearMonthController.PreviousYear);
    } else if (button == this._left1) {
        this.picker.recordAction(CalendarPicker.Action.ClickedBackwardMonthButton);
        this.moveRelatively(YearMonthController.PreviousMonth);
    } else if (button == this._right1) {
        this.picker.recordAction(CalendarPicker.Action.ClickedForwardMonthButton);
        this.moveRelatively(YearMonthController.NextMonth)
    } else if (button == this._right2) {
        this.picker.recordAction(CalendarPicker.Action.ClickedForwardYearButton);
        this.moveRelatively(YearMonthController.NextYear);
    } else if (button == this._right3)
        this.moveRelatively(YearMonthController.NextTenYears);
    else
        return;
};

/**
 * @param {!number} amount
 */
YearMonthController.prototype.moveRelatively = function(amount) {
    var current = this.picker.currentMonth().valueOf();
    var updated = Month.createFromValue(current + amount);
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
    this.picker.recordAction(CalendarPicker.Action.ClickedDay);
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
