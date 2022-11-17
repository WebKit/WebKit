// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: >
  When calendar.fields is undefined, since() doesn't perform an
  observable array iteration to convert the property bag to ZonedDateTime
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
calendar.fields = undefined;

const timeZone = new class extends Temporal.TimeZone {
  getPossibleInstantsFor(dt) {
    const instants = super.getPossibleInstantsFor.call(this, dt);
    // Return an iterable object that doesn't call Array iterator
    return {
      *[Symbol.iterator]() {
        for (let index = 0; index < instants.length; index++) {
          yield instants[index];
        }
      }
    }
  }
}("UTC");

const instance = new Temporal.ZonedDateTime(0n, timeZone);

// Detect observable array iteration:
const oldIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () {
  throw new Test262Error(`array shouldn't be iterated: ${new Error().stack}`);
};

const arg = { year: 1981, month: 12, day: 15, timeZone, calendar };

instance.since(arg);

Array.prototype[Symbol.iterator] = oldIterator;
