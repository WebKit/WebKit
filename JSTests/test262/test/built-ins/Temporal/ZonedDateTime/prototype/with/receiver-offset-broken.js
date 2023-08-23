// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.with
description: >
  TypeError thrown when the offset field of the argument or the object returned
  from mergeFields is broken
info: |
    7. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"month"*, *"monthCode"*, *"year"* »).
    8. Append *"hour"*, *"microsecond"*, *"millisecond"*, *"minute"*, *"nanosecond"*, *"offset"*, and *"second"* to _fieldNames_.
    9. Let _fields_ be ? PrepareTemporalFields(_zonedDateTime_, _fieldNames_, « *"offset"* »).
    10. Let _partialZonedDateTime_ be ? PrepareTemporalFields(_temporalZonedDateTimeLike_, _fieldNames_, ~partial~).
    11. Set _fields_ to ? CalendarMergeFields(_calendar_, _fields_, _partialZonedDateTime_).
    12. Set _fields_ to ? PrepareTemporalFields(_fields_, _fieldNames_, « *"offset"* »).
features: [Temporal]
---*/

class ObservedCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
    this.resetCalls();
  }

  toString() {
    return "observed-calendar";
  }

  mergeFields(original, additional) {
    this.mergeFieldsCalled++;
    const result = super.mergeFields(original, additional);
    result.offset = Symbol("can't convert to string");
    return result;
  }

  resetCalls() {
    this.mergeFieldsCalled = 0;
  }
}

const calendar = new ObservedCalendar();
const dateTime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);

// Test throw in step 10

assert.throws(TypeError, () => dateTime.with({ offset: Symbol("can't convert to string") }), "conversion failure on ZonedDateTime-like");
assert.sameValue(calendar.mergeFieldsCalled, 0, "calendar.mergeFields should not be called");

calendar.resetCalls();

// Test throw in step 12 (before sabotaging the ZonedDateTime instance)

assert.throws(TypeError, () => dateTime.with({ year: 2002 }), "conversion failure on sabotaged return value from mergeFields");
assert.sameValue(calendar.mergeFieldsCalled, 1, "calendar.mergeFields was called once");

calendar.resetCalls();
