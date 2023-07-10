// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dateAddOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dateAdd");
Object.defineProperty(Temporal.Calendar.prototype, "dateAdd", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dateAdd should not be looked up");
  },
});

const instance = new Temporal.PlainDate(2000, 5, 2, "iso8601");
instance.subtract(new Temporal.Duration(1));

Object.defineProperty(Temporal.Calendar.prototype, "dateAdd", dateAddOriginal);
