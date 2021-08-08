// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When Date is called as part of a new expression it is
    a constructor: it initializes the newly created object
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 2 arguments, (year, month)
---*/

if (typeof new Date(1899, 11) !== "object") {
  throw new Test262Error("#1.1: typeof new Date(1899, 11) should be 'object'");
}

if (new Date(1899, 11) === undefined) {
  throw new Test262Error("#1.2: new Date(1899, 11) should not be undefined");
}

var x13 = new Date(1899, 11);
if (typeof x13 !== "object") {
  throw new Test262Error("#1.3: typeof new Date(1899, 11) should be 'object'");
}

var x14 = new Date(1899, 11);
if (x14 === undefined) {
  throw new Test262Error("#1.4: new Date(1899, 11) should not be undefined");
}

if (typeof new Date(1899, 12) !== "object") {
  throw new Test262Error("#2.1: typeof new Date(1899, 12) should be 'object'");
}

if (new Date(1899, 12) === undefined) {
  throw new Test262Error("#2.2: new Date(1899, 12) should not be undefined");
}

var x23 = new Date(1899, 12);
if (typeof x23 !== "object") {
  throw new Test262Error("#2.3: typeof new Date(1899, 12) should be 'object'");
}

var x24 = new Date(1899, 12);
if (x24 === undefined) {
  throw new Test262Error("#2.4: new Date(1899, 12) should not be undefined");
}

if (typeof new Date(1900, 0) !== "object") {
  throw new Test262Error("#3.1: typeof new Date(1900, 0) should be 'object'");
}

if (new Date(1900, 0) === undefined) {
  throw new Test262Error("#3.2: new Date(1900, 0) should not be undefined");
}

var x33 = new Date(1900, 0);
if (typeof x33 !== "object") {
  throw new Test262Error("#3.3: typeof new Date(1900, 0) should be 'object'");
}

var x34 = new Date(1900, 0);
if (x34 === undefined) {
  throw new Test262Error("#3.4: new Date(1900, 0) should not be undefined");
}

if (typeof new Date(1969, 11) !== "object") {
  throw new Test262Error("#4.1: typeof new Date(1969, 11) should be 'object'");
}

if (new Date(1969, 11) === undefined) {
  throw new Test262Error("#4.2: new Date(1969, 11) should not be undefined");
}

var x43 = new Date(1969, 11);
if (typeof x43 !== "object") {
  throw new Test262Error("#4.3: typeof new Date(1969, 11) should be 'object'");
}

var x44 = new Date(1969, 11);
if (x44 === undefined) {
  throw new Test262Error("#4.4: new Date(1969, 11) should not be undefined");
}

if (typeof new Date(1969, 12) !== "object") {
  throw new Test262Error("#5.1: typeof new Date(1969, 12) should be 'object'");
}

if (new Date(1969, 12) === undefined) {
  throw new Test262Error("#5.2: new Date(1969, 12) should not be undefined");
}

var x53 = new Date(1969, 12);
if (typeof x53 !== "object") {
  throw new Test262Error("#5.3: typeof new Date(1969, 12) should be 'object'");
}

var x54 = new Date(1969, 12);
if (x54 === undefined) {
  throw new Test262Error("#5.4: new Date(1969, 12) should not be undefined");
}

if (typeof new Date(1970, 0) !== "object") {
  throw new Test262Error("#6.1: typeof new Date(1970, 0) should be 'object'");
}

if (new Date(1970, 0) === undefined) {
  throw new Test262Error("#6.2: new Date(1970, 0) should not be undefined");
}

var x63 = new Date(1970, 0);
if (typeof x63 !== "object") {
  throw new Test262Error("#6.3: typeof new Date(1970, 0) should be 'object'");
}

var x64 = new Date(1970, 0);
if (x64 === undefined) {
  throw new Test262Error("#6.4: new Date(1970, 0) should not be undefined");
}

if (typeof new Date(1999, 11) !== "object") {
  throw new Test262Error("#7.1: typeof new Date(1999, 11) should be 'object'");
}

if (new Date(1999, 11) === undefined) {
  throw new Test262Error("#7.2: new Date(1999, 11) should not be undefined");
}

var x73 = new Date(1999, 11);
if (typeof x73 !== "object") {
  throw new Test262Error("#7.3: typeof new Date(1999, 11) should be 'object'");
}

var x74 = new Date(1999, 11);
if (x74 === undefined) {
  throw new Test262Error("#7.4: new Date(1999, 11) should not be undefined");
}

if (typeof new Date(1999, 12) !== "object") {
  throw new Test262Error("#8.1: typeof new Date(1999, 12) should be 'object'");
}

if (new Date(1999, 12) === undefined) {
  throw new Test262Error("#8.2: new Date(1999, 12) should not be undefined");
}

var x83 = new Date(1999, 12);
if (typeof x83 !== "object") {
  throw new Test262Error("#8.3: typeof new Date(1999, 12) should be 'object'");
}

var x84 = new Date(1999, 12);
if (x84 === undefined) {
  throw new Test262Error("#8.4: new Date(1999, 12) should not be undefined");
}

if (typeof new Date(2000, 0) !== "object") {
  throw new Test262Error("#9.1: typeof new Date(2000, 0) should be 'object'");
}

if (new Date(2000, 0) === undefined) {
  throw new Test262Error("#9.2: new Date(2000, 0) should not be undefined");
}

var x93 = new Date(2000, 0);
if (typeof x93 !== "object") {
  throw new Test262Error("#9.3: typeof new Date(2000, 0) should be 'object'");
}

var x94 = new Date(2000, 0);
if (x94 === undefined) {
  throw new Test262Error("#9.4: new Date(2000, 0) should not be undefined");
}

if (typeof new Date(2099, 11) !== "object") {
  throw new Test262Error("#10.1: typeof new Date(2099, 11) should be 'object'");
}

if (new Date(2099, 11) === undefined) {
  throw new Test262Error("#10.2: new Date(2099, 11) should not be undefined");
}

var x103 = new Date(2099, 11);
if (typeof x103 !== "object") {
  throw new Test262Error("#10.3: typeof new Date(2099, 11) should be 'object'");
}

var x104 = new Date(2099, 11);
if (x104 === undefined) {
  throw new Test262Error("#10.4: new Date(2099, 11) should not be undefined");
}

if (typeof new Date(2099, 12) !== "object") {
  throw new Test262Error("#11.1: typeof new Date(2099, 12) should be 'object'");
}

if (new Date(2099, 12) === undefined) {
  throw new Test262Error("#11.2: new Date(2099, 12) should not be undefined");
}

var x113 = new Date(2099, 12);
if (typeof x113 !== "object") {
  throw new Test262Error("#11.3: typeof new Date(2099, 12) should be 'object'");
}

var x114 = new Date(2099, 12);
if (x114 === undefined) {
  throw new Test262Error("#11.4: new Date(2099, 12) should not be undefined");
}

if (typeof new Date(2100, 0) !== "object") {
  throw new Test262Error("#12.1: typeof new Date(2100, 0) should be 'object'");
}

if (new Date(2100, 0) === undefined) {
  throw new Test262Error("#12.2: new Date(2100, 0) should not be undefined");
}

var x123 = new Date(2100, 0);
if (typeof x123 !== "object") {
  throw new Test262Error("#12.3: typeof new Date(2100, 0) should be 'object'");
}

var x124 = new Date(2100, 0);
if (x124 === undefined) {
  throw new Test262Error("#12.4: new Date(2100, 0) should not be undefined");
}
