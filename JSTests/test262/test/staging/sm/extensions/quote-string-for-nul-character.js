// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Ensure we properly quote strings which can contain the NUL character before
// returning them to the user to avoid cutting off any trailing characters.

function assertStringIncludes(actual, expected) {
    assert.sameValue(actual.includes(expected), true, `"${actual}" includes "${expected}"`);
}

assertThrownErrorContains(() => "foo\0bar" in "asdf\0qwertz", "bar");
assertThrownErrorContains(() => "foo\0bar" in "asdf\0qwertz", "qwertz");

if (this.Intl) {
    assertThrownErrorContains(() => Intl.getCanonicalLocales("de\0Latn"), "Latn");

    assertThrownErrorContains(() => Intl.Collator.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.Collator("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.Collator("en", {usage: "sort\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.Collator("en", {caseFirst: "upper\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.Collator("en", {sensitivity: "base\0cookie"}), "cookie");

    assertThrownErrorContains(() => Intl.DateTimeFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {hourCycle: "h24\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {weekday: "narrow\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {era: "narrow\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {year: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {month: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {day: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {hour: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {minute: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {second: "2-digit\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {formatMatcher: "basic\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.DateTimeFormat("en", {timeZone: "UTC\0cookie"}), "cookie");

    assertThrownErrorContains(() => Intl.NumberFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.NumberFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.NumberFormat("en", {style: "decimal\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.NumberFormat("en", {currency: "USD\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.NumberFormat("en", {currencyDisplay: "code\0cookie"}), "cookie");

    assertThrownErrorContains(() => Intl.PluralRules.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.PluralRules("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.PluralRules("en", {type: "cardinal\0cookie"}), "cookie");

    assertThrownErrorContains(() => Intl.RelativeTimeFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.RelativeTimeFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.RelativeTimeFormat("en", {style: "long\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.RelativeTimeFormat("en", {numeric: "auto\0cookie"}), "cookie");
    assertThrownErrorContains(() => new Intl.RelativeTimeFormat().format(1, "day\0cookie"), "cookie");

    assertThrownErrorContains(() => new Intl.Locale("de\0keks"), "keks");
    assertThrownErrorContains(() => new Intl.Locale("de", {language: "it\0biscotto"}), "biscotto");
    assertThrownErrorContains(() => new Intl.Locale("th", {script: "Thai\0คุกกี้"}), "\\u0E04\\u0E38\\u0E01\\u0E01\\u0E35\\u0E49");
    assertThrownErrorContains(() => new Intl.Locale("en", {region: "GB\0biscuit"}), "biscuit");

    assertThrownErrorContains(() => new Intl.Locale("und", {calendar: "gregory\0F1"}), "F1");
    assertThrownErrorContains(() => new Intl.Locale("und", {collation: "phonebk\0F2"}), "F2");
    assertThrownErrorContains(() => new Intl.Locale("und", {hourCycle: "h24\0F3"}), "F3");
    assertThrownErrorContains(() => new Intl.Locale("und", {caseFirst: "false\0F4"}), "F4");
    assertThrownErrorContains(() => new Intl.Locale("und", {numberingSystem: "arab\0F5"}), "F5");
}

// Shell and helper functions.

assertThrownErrorContains(() => assert.sameValue(true, false, "foo\0bar"), "bar");

if (this.disassemble) {
    assertStringIncludes(disassemble(Function("var re = /foo\0bar/")), "bar");
}

if (this.getBacktrace) {
    const k = "asdf\0asdf";
    const o = {[k](a) { "use strict"; return getBacktrace({locals: true, args: true}); }};
    const r = o[k].call("foo\0bar", "baz\0faz");
    assertStringIncludes(r, "bar");
    assertStringIncludes(r, "faz");
}

// js/src/tests/browser.js provides its own |options| function, make sure we don't test that one.
if (this.options && typeof document === "undefined") {
    assertThrownErrorContains(() => options("foo\0bar"), "bar");
}

