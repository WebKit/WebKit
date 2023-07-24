// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainmonthday
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable lookups or calls to calendar methods.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const fieldsOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "fields");
Object.defineProperty(Temporal.Calendar.prototype, "fields", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("fields should not be looked up");
  },
});
const monthDayFromFieldsOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "monthDayFromFields");
Object.defineProperty(Temporal.Calendar.prototype, "monthDayFromFields", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("monthDayFromFields should not be looked up");
  },
});

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", "iso8601");
instance.toPlainMonthDay();

Object.defineProperty(Temporal.Calendar.prototype, "fields", fieldsOriginal);
Object.defineProperty(Temporal.Calendar.prototype, "monthDayFromFields", monthDayFromFieldsOriginal);
