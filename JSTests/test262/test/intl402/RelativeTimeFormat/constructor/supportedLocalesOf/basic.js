// Copyright 2018 Google Inc., Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.RelativeTimeFormat.supportedLocalesOf
description: >
    Tests that Intl.RelativeTimeFormat has a supportedLocalesOf property,
    and it works as planned.
features: [Intl.RelativeTimeFormat]
---*/

assert.sameValue(typeof Intl.RelativeTimeFormat.supportedLocalesOf, "function",
                 "supportedLocalesOf should be supported.");

const defaultLocale = new Intl.RelativeTimeFormat().resolvedOptions().locale;
const notSupported = 'zxx'; // "no linguistic content"
const requestedLocales = [defaultLocale, notSupported];

const supportedLocales = Intl.RelativeTimeFormat.supportedLocalesOf(requestedLocales);
assert.sameValue(supportedLocales.length, 1, 'The length of supported locales list is not 1.');
assert.sameValue(supportedLocales[0], defaultLocale, 'The default locale is not returned in the supported list.');
