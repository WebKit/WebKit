// Copyright 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-createdatetimeformat
description: Tests that the time zone names "Etc/UTC", "Etc/GMT", and "GMT" all resolve to "UTC".
info: |
  CreateDateTimeFormat ( dateTimeFormat, locales, options, required, default )

  29. If IsTimeZoneOffsetString(timeZone) is true, then
  ...
  30. Else,
    a. Let timeZoneIdentifierRecord be GetAvailableNamedTimeZoneIdentifier(timeZone).

  GetAvailableNamedTimeZoneIdentifier ( timeZoneIdentifier )

  ...
  5. For each element identifier of identifiers, do
  ...
    c. If primary is one of "Etc/UTC", "Etc/GMT", or "GMT", set primary to "UTC".
---*/

const utcIdentifiers = ["Etc/GMT", "Etc/UTC", "GMT"];

for (const timeZone of utcIdentifiers) {
  assert.sameValue(new Intl.DateTimeFormat([], {timeZone}).resolvedOptions().timeZone, "UTC", "Time zone name " + timeZone + " not canonicalized to 'UTC'.");
}
