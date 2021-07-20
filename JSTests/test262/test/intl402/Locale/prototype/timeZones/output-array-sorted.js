// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale.prototype.timeZones
description: >
    Checks that the return value of Intl.Locale.prototype.timeZones is a sorted
    Array.
info: |
  TimeZonesOfLocale ( loc )
  ...
  4. Let list be a List of 1 or more unique time zone identifiers, which must be
  String values indicating a Zone or Link name of the IANA Time Zone Database,
  ordered as if an Array of the same values had been sorted using
  %Array.prototype.sort% using undefined as comparefn, of those in common use
  in region.
features: [Intl.Locale,Intl.Locale-info]
includes: [compareArray.js]
---*/

const output = new Intl.Locale('en-US').timeZones;
assert.compareArray(output, [...output].sort());
