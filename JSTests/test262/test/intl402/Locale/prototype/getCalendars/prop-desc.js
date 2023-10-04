// Copyright 2023 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Locale.prototype.getCalendars
description: Checks the "getCalendars" property of the Locale prototype object.
info: |
    Intl.Locale.prototype.getCalendars ()
    Unless specified otherwise in this document, the objects, functions, and constructors described in this standard are subject to the generic requirements and restrictions specified for standard built-in ECMAScript objects in the ECMAScript 2019 Language Specification, 10th edition, clause 17, or successor.
    Every other data property described in clauses 18 through 26 and in Annex B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [Intl.Locale,Intl.Locale-info]
---*/

assert.sameValue(
  typeof Intl.Locale.prototype.getCalendars,
  "function",
  "typeof Intl.Locale.prototype.getCalendars is function"
);

verifyProperty(Intl.Locale.prototype, "getCalendars", {
  writable: true,
  enumerable: false,
  configurable: true,
});
