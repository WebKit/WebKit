// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Prototype]] property of the newly constructed object
    is set to the original Date prototype object, the one that is the
    initial value of Date.prototype
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 4 arguments, (year, month, date, hours)
---*/

var x11 = new Date(1899, 11, 31, 23);
if (typeof x11.constructor.prototype !== "object") {
  throw new Test262Error("#1.1: typeof x11.constructor.prototype === 'object'");
}

var x12 = new Date(1899, 11, 31, 23);
if (!Date.prototype.isPrototypeOf(x12)) {
  throw new Test262Error('#1.2: Date.prototype.isPrototypeOf(x12)');
}

var x13 = new Date(1899, 11, 31, 23);
if (Date.prototype !== x13.constructor.prototype) {
  throw new Test262Error("#1.3: Date.prototype === x13.constructor.prototype");
}

var x21 = new Date(1899, 12, 1, 0);
if (typeof x21.constructor.prototype !== "object") {
  throw new Test262Error("#2.1: typeof x21.constructor.prototype === 'object'");
}

var x22 = new Date(1899, 12, 1, 0);
if (!Date.prototype.isPrototypeOf(x22)) {
  throw new Test262Error('#2.2: Date.prototype.isPrototypeOf(x22)');
}

var x23 = new Date(1899, 12, 1, 0);
if (Date.prototype !== x23.constructor.prototype) {
  throw new Test262Error("#2.3: Date.prototype === x23.constructor.prototype");
}

var x31 = new Date(1900, 0, 1, 0);
if (typeof x31.constructor.prototype !== "object") {
  throw new Test262Error("#3.1: typeof x31.constructor.prototype === 'object'");
}

var x32 = new Date(1900, 0, 1, 0);
if (!Date.prototype.isPrototypeOf(x32)) {
  throw new Test262Error('#3.2: Date.prototype.isPrototypeOf(x32)');
}

var x33 = new Date(1900, 0, 1, 0);
if (Date.prototype !== x33.constructor.prototype) {
  throw new Test262Error("#3.3: Date.prototype === x33.constructor.prototype");
}

var x41 = new Date(1969, 11, 31, 23);
if (typeof x41.constructor.prototype !== "object") {
  throw new Test262Error("#4.1: typeof x41.constructor.prototype === 'object'");
}

var x42 = new Date(1969, 11, 31, 23);
if (!Date.prototype.isPrototypeOf(x42)) {
  throw new Test262Error('#4.2: Date.prototype.isPrototypeOf(x42)');
}

var x43 = new Date(1969, 11, 31, 23);
if (Date.prototype !== x43.constructor.prototype) {
  throw new Test262Error("#4.3: Date.prototype === x43.constructor.prototype");
}

var x51 = new Date(1969, 12, 1, 0);
if (typeof x51.constructor.prototype !== "object") {
  throw new Test262Error("#5.1: typeof x51.constructor.prototype === 'object'");
}

var x52 = new Date(1969, 12, 1, 0);
if (!Date.prototype.isPrototypeOf(x52)) {
  throw new Test262Error('#5.2: Date.prototype.isPrototypeOf(x52)');
}

var x53 = new Date(1969, 12, 1, 0);
if (Date.prototype !== x53.constructor.prototype) {
  throw new Test262Error("#5.3: Date.prototype === x53.constructor.prototype");
}

var x61 = new Date(1970, 0, 1, 0);
if (typeof x61.constructor.prototype !== "object") {
  throw new Test262Error("#6.1: typeof x61.constructor.prototype === 'object'");
}

var x62 = new Date(1970, 0, 1, 0);
if (!Date.prototype.isPrototypeOf(x62)) {
  throw new Test262Error('#6.2: Date.prototype.isPrototypeOf(x62)');
}

var x63 = new Date(1970, 0, 1, 0);
if (Date.prototype !== x63.constructor.prototype) {
  throw new Test262Error("#6.3: Date.prototype === x63.constructor.prototype");
}

var x71 = new Date(1999, 11, 31, 23);
if (typeof x71.constructor.prototype !== "object") {
  throw new Test262Error("#7.1: typeof x71.constructor.prototype === 'object'");
}

var x72 = new Date(1999, 11, 31, 23);
if (!Date.prototype.isPrototypeOf(x72)) {
  throw new Test262Error('#7.2: Date.prototype.isPrototypeOf(x72)');
}

var x73 = new Date(1999, 11, 31, 23);
if (Date.prototype !== x73.constructor.prototype) {
  throw new Test262Error("#7.3: Date.prototype === x73.constructor.prototype");
}

var x81 = new Date(1999, 12, 1, 0);
if (typeof x81.constructor.prototype !== "object") {
  throw new Test262Error("#8.1: typeof x81.constructor.prototype === 'object'");
}

var x82 = new Date(1999, 12, 1, 0);
if (!Date.prototype.isPrototypeOf(x82)) {
  throw new Test262Error('#8.2: Date.prototype.isPrototypeOf(x82)');
}

var x83 = new Date(1999, 12, 1, 0);
if (Date.prototype !== x83.constructor.prototype) {
  throw new Test262Error("#8.3: Date.prototype === x83.constructor.prototype");
}

var x91 = new Date(2000, 0, 1, 0);
if (typeof x91.constructor.prototype !== "object") {
  throw new Test262Error("#9.1: typeof x91.constructor.prototype === 'object'");
}

var x92 = new Date(2000, 0, 1, 0);
if (!Date.prototype.isPrototypeOf(x92)) {
  throw new Test262Error('#9.2: Date.prototype.isPrototypeOf(x92)');
}

var x93 = new Date(2000, 0, 1, 0);
if (Date.prototype !== x93.constructor.prototype) {
  throw new Test262Error("#9.3: Date.prototype === x93.constructor.prototype");
}

var x101 = new Date(2099, 11, 31, 23);
if (typeof x101.constructor.prototype !== "object") {
  throw new Test262Error("#10.1: typeof x101.constructor.prototype === 'object'");
}

var x102 = new Date(2099, 11, 31, 23);
if (!Date.prototype.isPrototypeOf(x102)) {
  throw new Test262Error('#10.2: Date.prototype.isPrototypeOf(x102)');
}

var x103 = new Date(2099, 11, 31, 23);
if (Date.prototype !== x103.constructor.prototype) {
  throw new Test262Error("#10.3: Date.prototype === x103.constructor.prototype");
}

var x111 = new Date(2099, 12, 1, 0);
if (typeof x111.constructor.prototype !== "object") {
  throw new Test262Error("#11.1: typeof x111.constructor.prototype === 'object'");
}

var x112 = new Date(2099, 12, 1, 0);
if (!Date.prototype.isPrototypeOf(x112)) {
  throw new Test262Error('#11.2: Date.prototype.isPrototypeOf(x112)');
}

var x113 = new Date(2099, 12, 1, 0);
if (Date.prototype !== x113.constructor.prototype) {
  throw new Test262Error("#11.3: Date.prototype === x113.constructor.prototype");
}

var x121 = new Date(2100, 0, 1, 0);
if (typeof x121.constructor.prototype !== "object") {
  throw new Test262Error("#12.1: typeof x121.constructor.prototype === 'object'");
}

var x122 = new Date(2100, 0, 1, 0);
if (!Date.prototype.isPrototypeOf(x122)) {
  throw new Test262Error('#12.2: Date.prototype.isPrototypeOf(x122)');
}

var x123 = new Date(2100, 0, 1, 0);
if (Date.prototype !== x123.constructor.prototype) {
  throw new Test262Error("#12.3: Date.prototype === x123.constructor.prototype");
}
