// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.resolvedOptions
description: >
  Intl.DateTimeFormat.prototype.resolvedOptions properly
  reflect hourCycle settings.
info: |
  12.4.5 Intl.DateTimeFormat.prototype.resolvedOptions()

  12.1.1 InitializeDateTimeFormat ( dateTimeFormat, locales, options )
   29. If dateTimeFormat.[[Hour]] is not undefined, then
     a. Let hcDefault be dataLocaleData.[[hourCycle]].
     b. Let hc be dateTimeFormat.[[HourCycle]].
     c. If hc is null, then
        i. Set hc to hcDefault.
     d. If hour12 is not undefined, then
        i. If hour12 is true, then
           1. If hcDefault is "h11" or "h23", then
              a. Set hc to "h11".
           2. Else,
              a. Set hc to "h12".
        ii. Else,
           1. Assert: hour12 is false.
           2. If hcDefault is "h11" or "h23", then
              a. Set hc to "h23".
           3. Else,
              a. Set hc to "h24".
     e. Set dateTimeFormat.[[HourCycle]] to hc.
     
locale: [en, fr, it, ja, zh, ko, ar, hi]
---*/

let locales = ["en", "fr", "it", "ja", "zh", "ko", "ar", "hi"];

locales.forEach(function(locale) {
  let hcDefault = (new Intl.DateTimeFormat(locale, {hour: "numeric"}))
      .resolvedOptions().hourCycle;
  if (hcDefault == "h11" || hcDefault == "h23") {
    assert.sameValue("h11",
        (new Intl.DateTimeFormat(locale, {hour: "numeric", hour12: true}))
        .resolvedOptions().hourCycle);
    assert.sameValue("h23",
        (new Intl.DateTimeFormat(locale, {hour: "numeric", hour12: false}))
        .resolvedOptions().hourCycle);
  } else {
    assert.sameValue(true, hcDefault == "h12" || hcDefault == "h24")
    assert.sameValue("h12",
        (new Intl.DateTimeFormat(locale, {hour: "numeric", hour12: true}))
        .resolvedOptions().hourCycle);
    assert.sameValue("h24",
        (new Intl.DateTimeFormat(locale, {hour: "numeric", hour12: false}))
        .resolvedOptions().hourCycle);
  }
});
