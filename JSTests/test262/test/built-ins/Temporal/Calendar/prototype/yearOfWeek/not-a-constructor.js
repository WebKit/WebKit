// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: >
  Temporal.Calendar.prototype.yearOfWeek does not implement [[Construct]], is not new-able
info: |
    Built-in function objects that are not identified as constructors do not implement the
    [[Construct]] internal method unless otherwise specified in the description of a particular
    function.
includes: [isConstructor.js]
features: [Reflect.construct, Temporal]
---*/

assert.throws(TypeError, () => {
  new Temporal.Calendar.prototype.yearOfWeek();
}, "Calling as constructor");

assert.sameValue(isConstructor(Temporal.Calendar.prototype.yearOfWeek), false,
  "isConstructor(Temporal.Calendar.prototype.yearOfWeek)");
