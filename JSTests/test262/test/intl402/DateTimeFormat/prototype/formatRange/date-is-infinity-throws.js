// Copyright 2019 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Throws a RangeError if date arg is cast to an Infinity value
info: |
  Intl.DateTimeFormat.prototype.formatRange ( startDate , endDate )

  1. Let dtf be this value.
  2. If Type(dtf) is not Object, throw a TypeError exception.
  3. If dtf does not have an [[InitializedDateTimeFormat]] internal slot, throw a TypeError exception.
  4. If startDate is undefined or endDate is undefined, throw a RangeError exception.
  5. Let x be ? ToNumber(startDate).
  6. Let y be ? ToNumber(endDate).
  7. If x is greater than y, throw a RangeError exception.
  8. Return ? FormatDateTimeRange(dtf, x, y).

  FormatDateTimeRange ( dateTimeFormat, x, y )

  1. Let parts be ? PartitionDateTimeRangePattern(dateTimeFormat, x, y).

  PartitionDateTimeRangePattern ( dateTimeFormat, x, y )
  
    1. Let x be TimeClip(x).
  2. If x is NaN, throw a RangeError exception.
  3. Let y be TimeClip(y).
  4. If y is NaN, throw a RangeError exception.

  TimeClip ( time )
  1. If time is not finite, return NaN.

features: [Intl.DateTimeFormat-formatRange]
---*/

var dtf = new Intl.DateTimeFormat();

var date = new Date();

assert.throws(RangeError, function() {
  dtf.formatRange(Infinity, date);
}, "+Infinity/date");

assert.throws(RangeError, function() {
  dtf.formatRange(-Infinity, date);
}, "-Infinity/date");

assert.throws(RangeError, function() {
  dtf.formatRange(date, Infinity);
}, "date/+Infinity");

assert.throws(RangeError, function() {
  dtf.formatRange(date, -Infinity);
}, "date/-Infinity");

assert.throws(RangeError, function() {
  dtf.formatRange(Infinity, Infinity);
}, "+Infinity/+Infinity");

assert.throws(RangeError, function() {
  dtf.formatRange(-Infinity, -Infinity);
}, "-Infinity/-Infinity");

assert.throws(RangeError, function() {
  dtf.formatRange(Infinity, -Infinity);
}, "+Infinity/-Infinity");

assert.throws(RangeError, function() {
  dtf.formatRange(-Infinity, Infinity);
}, "-Infinity/+Infinity");
