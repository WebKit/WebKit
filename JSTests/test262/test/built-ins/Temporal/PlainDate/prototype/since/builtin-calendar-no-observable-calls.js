// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dateUntilOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dateUntil");
Object.defineProperty(Temporal.Calendar.prototype, "dateUntil", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dateUntil should not be looked up");
  },
});

const instance = new Temporal.PlainDate(2000, 5, 2, "iso8601");
instance.since(new Temporal.PlainDate(1999, 4, 1));

Object.defineProperty(Temporal.Calendar.prototype, "dateUntil", dateUntilOriginal);
