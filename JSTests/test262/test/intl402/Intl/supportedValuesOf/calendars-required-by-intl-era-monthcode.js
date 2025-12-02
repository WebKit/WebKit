// Copyright (C) 2025 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.supportedvaluesof
description: >
  Verifies that all calendars required by Intl.Era-monthcode are supported. See:
  https://tc39.es/proposal-intl-era-monthcode/#table-calendar-types
info: |
  Intl.supportedValuesOf ( key )
    1  Let key be ? ToString(key).
    2. If key is "calendar", then
     a. Let list be a new empty List.
     b. For each element identifier of AvailableCalendars(), do
        i. Let canonical be CanonicalizeUValue("ca", identifier).
        ii. If identifier is canonical, then
          1. Append identifier to list.
          ...
    9. Return CreateArrayFromList( list ).

  AvailableCalendars ( )
    The implementation-defined abstract operation AvailableCalendars takes no arguments and returns a List of calendar types. The returned List is sorted according to lexicographic code unit order, and contains unique calendar types in canonical form (6.9) identifying the calendars for which the implementation provides the functionality of Intl.DateTimeFormat objects, including their aliases (e.g., both of "islamicc" and "islamic-civil"). The List must include the Calendar Type value of every row of Table 1, except the header row.
locale: [en]
features: [Intl-enumeration, Intl.Era-monthcode]
---*/

const requiredCalendars = [
  "buddhist",
  "chinese",
  "coptic",
  "dangi",
  "ethioaa",
  "ethiopic",
  "gregory",
  "hebrew",
  "indian",
  "islamic-civil",
  "islamic-tbla",
  "islamic-umalqura",
  "iso8601",
  "japanese",
  "persian",
  "roc"
]

const supportedCalendars = Intl.supportedValuesOf("calendar");
for (const calendar of requiredCalendars) {
  assert(supportedCalendars.includes(calendar), "Required calendar: " + calendar + " must be supported");
}
