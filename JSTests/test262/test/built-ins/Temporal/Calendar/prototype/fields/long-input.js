// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: >
  Temporal.Calendar.prototype.fields will take iterable of any size and any string
  and return Array of the same content.
info: |
  ## 12.4.21 Temporal.Calendar.prototype.fields ( fields )
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. Let fieldNames be ? IterableToListOfType(fields, « String »).
  5. Return ! CreateArrayFromList(fieldNames).
features: [Symbol, Symbol.iterator, Temporal, computed-property-names, generators]
includes: [compareArray.js]
---*/
let cal = new Temporal.Calendar("iso8601")
const fields = {
  *[Symbol.iterator]() {
      let i = 0;
      while (i++ < 1000001) {
        yield "garbage " + i;
      }
  }
}
assert(
  compareArray(cal.fields(fields), Array.from(fields)),
  'compareArray(cal.fields(fields), Array.from(fields)) must return true'
);
