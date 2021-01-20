// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Checks handling of the options argument to the DateTimeFormat constructor.
info: |
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )

    ...
    30. Let timeStyle be ? GetOption(options, "timeStyle", "string", « "full", "long", "medium", "short" », undefined).
    31. If timeStyle is not undefined, set dateTimeFormat.[[TimeStyle]] to timeStyle.
features: [Intl.DateTimeFormat-datetimestyle]
---*/

const validOptions = [
  [undefined, undefined],
  ["full", "full"],
  ["long", "long"],
  ["medium", "medium"],
  ["short", "short"],
  [{ toString() { return "full"; } }, "full"],
  [{ valueOf() { return "long"; }, toString: undefined }, "long"],
];
for (const [timeStyle, expected] of validOptions) {
  const dtf = new Intl.DateTimeFormat("en", { timeStyle });
  const options = dtf.resolvedOptions();
  assert.sameValue(options.timeStyle, expected);
  const propdesc = Object.getOwnPropertyDescriptor(options, "timeStyle");
  if (expected === undefined) {
    assert.sameValue(propdesc, undefined);
  } else {
    assert.sameValue(propdesc.value, expected);
  }
}
