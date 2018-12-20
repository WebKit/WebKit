// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies getters with grandfathered tags.
info: |
    get Intl.Locale.prototype.baseName
    4. If locale does not match the langtag production, return locale.
    5. Return the substring of locale corresponding to the
       language ["-" script] ["-" region] *("-" variant)
       subsequence of the langtag grammar.

    get Intl.Locale.prototype.language
    4. If locale matches the privateuse or the grandfathered production, return locale.

    get Intl.Locale.prototype.script
    4. If locale matches the privateuse or the grandfathered production, return undefined.

    get Intl.Locale.prototype.region
    4. If locale matches the privateuse or the grandfathered production, return undefined.
features: [Intl.Locale]
---*/

// Irregular grandfathered language tag.
var loc = new Intl.Locale("i-default");
assert.sameValue(loc.baseName, "i-default"); // Step 4.
assert.sameValue(loc.language, "i-default");
assert.sameValue(loc.script, undefined);
assert.sameValue(loc.region, undefined);

// Regular grandfathered language tag.
var loc = new Intl.Locale("cel-gaulish");
assert.sameValue(loc.baseName, "cel-gaulish"); // Step 5.
assert.sameValue(loc.language, "cel-gaulish");
assert.sameValue(loc.script, undefined);
assert.sameValue(loc.region, undefined);

// Regular grandfathered language tag.
var loc = new Intl.Locale("zh-min");
assert.sameValue(loc.baseName, "zh-min"); // Step 5.
assert.sameValue(loc.language, "zh-min");
assert.sameValue(loc.script, undefined);
assert.sameValue(loc.region, undefined);
