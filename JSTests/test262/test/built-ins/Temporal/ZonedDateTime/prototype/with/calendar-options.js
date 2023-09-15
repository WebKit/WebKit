// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: >
  The options argument is copied and the copy is passed to
  Calendar#dateFromFields.
features: [Temporal]
---*/

const options = {
  extra: "property",
};
let calledDateFromFields = 0;
class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateFromFields(fields, optionsArg) {
    ++calledDateFromFields;
    assert.notSameValue(optionsArg, options, "should pass copied options object");
    assert.sameValue(optionsArg.extra, "property", "should copy all properties from options object");
    assert.sameValue(Object.getPrototypeOf(optionsArg), null, "Copy has null prototype");
    return super.dateFromFields(fields, optionsArg);
  }
};
const calendar = new Calendar();
const datetime = new Temporal.ZonedDateTime(0n, "UTC", calendar);
const result = datetime.with({ year: 1971 }, options);
assert.sameValue(result.epochNanoseconds, 365n * 86400_000_000_000n, "year changed from 1970 to 1971")
assert.sameValue(calledDateFromFields, 1, "should have called overridden dateFromFields once");
