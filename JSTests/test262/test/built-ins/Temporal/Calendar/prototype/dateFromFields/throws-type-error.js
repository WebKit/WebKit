// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Temporal.Calendar.prototype.dateFromFields should throw TypeError with wrong type.
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISODateFromFields(fields, options).
  7. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).
features: [BigInt, Symbol, Temporal, arrow-function]
---*/
// Check throw for first arg
let cal = new Temporal.Calendar('iso8601');
assert.throws(TypeError, () => cal.dateFromFields(), 'cal.dateFromFields() throws a TypeError exception');

[undefined, true, false, 123, 456n, Symbol(), 'string'].forEach(function(fields) {
  assert.throws(
    TypeError,
    () => cal.dateFromFields(fields),
    'cal.dateFromFields(fields) throws a TypeError exception'
  );

  assert.throws(
    TypeError,
    () => cal.dateFromFields(fields, undefined),
    'cal.dateFromFields(fields, undefined) throws a TypeError exception'
  );

  assert.throws(TypeError, () => cal.dateFromFields(fields, {
    overflow: 'constrain'
  }), 'cal.dateFromFields(fields, {overflow: "constrain"}) throws a TypeError exception');

  assert.throws(TypeError, () => cal.dateFromFields(fields, {
    overflow: 'reject'
  }), 'cal.dateFromFields(fields, {overflow: "reject"}) throws a TypeError exception');
});

assert.throws(TypeError, () => cal.dateFromFields({
  month: 1,
  day: 17
}), 'cal.dateFromFields({month: 1, day: 17}) throws a TypeError exception');

assert.throws(TypeError, () => cal.dateFromFields({
  year: 2021,
  day: 17
}), 'cal.dateFromFields({year: 2021, day: 17}) throws a TypeError exception');

assert.throws(TypeError, () => cal.dateFromFields({
  year: 2021,
  month: 12
}), 'cal.dateFromFields({year: 2021, month: 12}) throws a TypeError exception');
