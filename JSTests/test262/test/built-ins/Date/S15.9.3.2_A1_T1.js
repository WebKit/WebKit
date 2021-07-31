// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When Date is called as part of a new expression it is
    a constructor: it initialises the newly created object
esid: sec-date-value
description: Checking types of newly created objects and it values
includes: [dateConstants.js]
---*/

if (typeof new Date(date_1899_end) !== "object") {
  throw new Test262Error("#1.1: typeof new Date(date_1899_end) === 'object'");
}

if (new Date(date_1899_end) === undefined) {
  throw new Test262Error("#1.2: new Date(date_1899_end) === undefined");
}

var x13 = new Date(date_1899_end);
if (typeof x13 !== "object") {
  throw new Test262Error("#1.3: typeof new Date(date_1899_end) !== 'object'");
}

var x14 = new Date(date_1899_end);
if (x14 === undefined) {
  throw new Test262Error("#1.4: new Date(date_1899_end) !== undefined");
}

if (typeof new Date(date_1900_start) !== "object") {
  throw new Test262Error("#2.1: typeof new Date(date_1900_start) === 'object'");
}

if (new Date(date_1900_start) === undefined) {
  throw new Test262Error("#2.2: new Date(date_1900_start) === undefined");
}

var x23 = new Date(date_1900_start);
if (typeof x23 !== "object") {
  throw new Test262Error("#2.3: typeof new Date(date_1900_start) !== 'object'");
}

var x24 = new Date(date_1900_start);
if (x24 === undefined) {
  throw new Test262Error("#2.4: new Date(date_1900_start) !== undefined");
}

if (typeof new Date(date_1969_end) !== "object") {
  throw new Test262Error("#3.1: typeof new Date(date_1969_end) === 'object'");
}

if (new Date(date_1969_end) === undefined) {
  throw new Test262Error("#3.2: new Date(date_1969_end) === undefined");
}

var x33 = new Date(date_1969_end);
if (typeof x33 !== "object") {
  throw new Test262Error("#3.3: typeof new Date(date_1969_end) !== 'object'");
}

var x34 = new Date(date_1969_end);
if (x34 === undefined) {
  throw new Test262Error("#3.4: new Date(date_1969_end) !== undefined");
}

if (typeof new Date(date_1970_start) !== "object") {
  throw new Test262Error("#4.1: typeof new Date(date_1970_start) === 'object'");
}

if (new Date(date_1970_start) === undefined) {
  throw new Test262Error("#4.2: new Date(date_1970_start) === undefined");
}

var x43 = new Date(date_1970_start);
if (typeof x43 !== "object") {
  throw new Test262Error("#4.3: typeof new Date(date_1970_start) !== 'object'");
}

var x44 = new Date(date_1970_start);
if (x44 === undefined) {
  throw new Test262Error("#4.4: new Date(date_1970_start) !== undefined");
}

if (typeof new Date(date_1999_end) !== "object") {
  throw new Test262Error("#5.1: typeof new Date(date_1999_end) === 'object'");
}

if (new Date(date_1999_end) === undefined) {
  throw new Test262Error("#5.2: new Date(date_1999_end) === undefined");
}

var x53 = new Date(date_1999_end);
if (typeof x53 !== "object") {
  throw new Test262Error("#5.3: typeof new Date(date_1999_end) !== 'object'");
}

var x54 = new Date(date_1999_end);
if (x54 === undefined) {
  throw new Test262Error("#5.4: new Date(date_1999_end) !== undefined");
}

if (typeof new Date(date_2000_start) !== "object") {
  throw new Test262Error("#6.1: typeof new Date(date_2000_start) === 'object'");
}

if (new Date(date_2000_start) === undefined) {
  throw new Test262Error("#6.2: new Date(date_2000_start) === undefined");
}

var x63 = new Date(date_2000_start);
if (typeof x63 !== "object") {
  throw new Test262Error("#6.3: typeof new Date(date_2000_start) !== 'object'");
}

var x64 = new Date(date_2000_start);
if (x64 === undefined) {
  throw new Test262Error("#6.4: new Date(date_2000_start) !== undefined");
}

if (typeof new Date(date_2099_end) !== "object") {
  throw new Test262Error("#7.1: typeof new Date(date_2099_end) === 'object'");
}

if (new Date(date_2099_end) === undefined) {
  throw new Test262Error("#7.2: new Date(date_2099_end) === undefined");
}

var x73 = new Date(date_2099_end);
if (typeof x73 !== "object") {
  throw new Test262Error("#7.3: typeof new Date(date_2099_end) !== 'object'");
}

var x74 = new Date(date_2099_end);
if (x74 === undefined) {
  throw new Test262Error("#7.4: new Date(date_2099_end) !== undefined");
}

if (typeof new Date(date_2100_start) !== "object") {
  throw new Test262Error("#8.1: typeof new Date(date_2100_start) === 'object'");
}

if (new Date(date_2100_start) === undefined) {
  throw new Test262Error("#8.2: new Date(date_2100_start) === undefined");
}

var x83 = new Date(date_2100_start);
if (typeof x83 !== "object") {
  throw new Test262Error("#8.3: typeof new Date(date_2100_start) !== 'object'");
}

var x84 = new Date(date_2100_start);
if (x84 === undefined) {
  throw new Test262Error("#8.4: new Date(date_2100_start) !== undefined");
}
