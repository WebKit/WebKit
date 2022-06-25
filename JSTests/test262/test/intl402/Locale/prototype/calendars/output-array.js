// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale.prototype.calendars
description: >
    Checks that the return value of Intl.Locale.prototype.calendars is an Array.
info: |
  CalendarsOfLocale ( loc )
  ...
  5. Return ! CreateArrayFromListAndPreferred( list, preferred ).
features: [Intl.Locale,Intl.Locale-info]
---*/

assert(Array.isArray(new Intl.Locale('en').calendars));
assert(new Intl.Locale('en').calendars.length > 0, 'array has at least one element');
