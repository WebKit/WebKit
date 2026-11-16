//@ skip
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=318362
// Skipped: exposes a known, deferred bug, the language-change flavor of
// intl-datetimeformat-default-formatter-stale-across-tz-change.js. The
// per-global default formatters (JSGlobalObject::m_defaultDateTimeFormat /
// m_defaultDateFormat / m_defaultTimeFormat) backing the no-argument
// Date.prototype.toLocale*String paths are initialized once and never
// invalidated, so after a user-preferred-language change they keep the old
// locale while every other construction shape re-resolves.
//
// Remove the //@ skip (and add the runDefault("--useDollarVM=1") directive the
// sibling language-change tests use) once the default formatters are
// invalidated on language change.

if (typeof $vm === "undefined" || typeof $vm.setUserPreferredLanguages !== "function")
    quit();

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

$vm.setUserPreferredLanguages(["en-US"]);

setTimeout(() => {
    const d = new Date(Date.UTC(2024, 5, 15, 0, 0));

    // Initialize all three per-global default formatters under en-US. Each
    // no-argument call must agree with its explicit-empty-options shape, which
    // builds a fresh formatter with the same required/defaults.
    expect("en-US toLocaleString agrees with slow path",
        d.toLocaleString(), d.toLocaleString(undefined, {}));
    expect("en-US toLocaleDateString agrees with slow path",
        d.toLocaleDateString(), d.toLocaleDateString(undefined, {}));
    expect("en-US toLocaleTimeString agrees with slow path",
        d.toLocaleTimeString(), d.toLocaleTimeString(undefined, {}));

    $vm.setUserPreferredLanguages(["ja-JP"]);

    setTimeout(() => {
        expect("post-change locale moved",
            new Intl.DateTimeFormat(undefined, {}).resolvedOptions().locale.startsWith("ja"), true);

        expect("ja-JP toLocaleString agrees with slow path",
            d.toLocaleString(), d.toLocaleString(undefined, {}));
        expect("ja-JP toLocaleDateString agrees with slow path",
            d.toLocaleDateString(), d.toLocaleDateString(undefined, {}));
        expect("ja-JP toLocaleTimeString agrees with slow path",
            d.toLocaleTimeString(), d.toLocaleTimeString(undefined, {}));
    }, 0);
}, 0);
