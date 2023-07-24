// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
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
const mergeFieldsOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "mergeFields");
Object.defineProperty(Temporal.Calendar.prototype, "mergeFields", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("mergeFields should not be looked up");
  },
});
const dateFromFieldsOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dateFromFields");
Object.defineProperty(Temporal.Calendar.prototype, "dateFromFields", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dateFromFields should not be looked up");
  },
});

const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, "iso8601");
instance.with({ year: 2001 });

Object.defineProperty(Temporal.Calendar.prototype, "fields", fieldsOriginal);
Object.defineProperty(Temporal.Calendar.prototype, "mergeFields", mergeFieldsOriginal);
Object.defineProperty(Temporal.Calendar.prototype, "dateFromFields", dateFromFieldsOriginal);
