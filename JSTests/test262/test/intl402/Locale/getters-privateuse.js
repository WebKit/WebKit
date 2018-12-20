// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies getters with privateuse tags.
info: |
    get Intl.Locale.prototype.baseName
    4. If locale does not match the langtag production, return locale.

    get Intl.Locale.prototype.language
    4. If locale matches the privateuse or the grandfathered production, return locale.

    get Intl.Locale.prototype.script
    4. If locale matches the privateuse or the grandfathered production, return undefined.

    get Intl.Locale.prototype.region
    4. If locale matches the privateuse or the grandfathered production, return undefined.
features: [Intl.Locale]
---*/

// Privateuse only language tag.
var loc = new Intl.Locale("x-private");
assert.sameValue(loc.baseName, "x-private");
assert.sameValue(loc.language, "x-private");
assert.sameValue(loc.script, undefined);
assert.sameValue(loc.region, undefined);
