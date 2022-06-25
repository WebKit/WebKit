// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.with
description: TypeError thrown when the offset field of the receiver is broken
info: |
    10. Let _fieldNames_ be ? CalendarFields(_calendar_, « *"day"*, *"hour"*, *"microsecond"*, *"millisecond"*, *"minute"*, *"month"*, *"monthCode"*, *"nanosecond"*, *"second"*, *"year"* »).
    11. Append *"offset"* to _fieldNames_.
    12. Let _partialZonedDateTime_ be ? PreparePartialTemporalFields(_temporalZonedDateTimeLike_, _fieldNames_).
    ...
    17. Append *"timeZone"* to _fieldNames_.
    18. Let _fields_ be ? PrepareTemporalFields(_zonedDateTime_, _fieldNames_, « *"timeZone"*, *"offset"* »).
    19. Set _fields_ to ? CalendarMergeFields(_calendar_, _fields_, _partialZonedDateTime_).
    20. Set _fields_ to ? PrepareTemporalFields(_fields_, _fieldNames_, « *"timeZone"*, *"offset"* »).
    21. Let _offsetString_ be ! Get(_fields_, *"offset"*).
    22. Assert: Type(_offsetString_) is String.
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

// Test throw in step 12

assert.throws(TypeError, () => dateTime.with({ offset: Symbol("can't convert to string") }), "conversion failure on ZonedDateTime-like");
assert.sameValue(calendar.mergeFieldsCalled, 0, "calendar.mergeFields should not be called");

calendar.resetCalls();

// Test throw in step 20 (before sabotaging the ZonedDateTime instance)

assert.throws(TypeError, () => dateTime.with({ year: 2002 }), "conversion failure on sabotaged return value from mergeFields");
assert.sameValue(calendar.mergeFieldsCalled, 1, "calendar.mergeFields was called once");

calendar.resetCalls();

// Test throw in step 18

Object.defineProperty(dateTime, "offset", { value: Symbol("can't convert to string"), configurable: true });

assert.throws(TypeError, () => dateTime.with({ year: 2002 }), "conversion failure on sabotaged offset field of receiver");
assert.sameValue(calendar.mergeFieldsCalled, 0, "calendar.mergeFields should not be called");

calendar.resetCalls();

// Test offset being required in step 18

Object.defineProperty(dateTime, "offset", { value: undefined });

assert.throws(TypeError, () => dateTime.with({ year: 2002 }), "offset property is required on receiver");
assert.sameValue(calendar.mergeFieldsCalled, 0, "calendar.mergeFields should not be called");

calendar.resetCalls();
