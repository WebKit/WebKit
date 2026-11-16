//@ runDefault("--useDollarVM=1")

// IntlDateTimeFormat resolution depends on the user-preferred languages list;
// when it changes, subsequent constructions must reflect the new list rather
// than serving a stale resolution.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected "${want}", got "${got}"`);
}

function startsWith(s, prefix)
{
    return typeof s === "string" && s.indexOf(prefix) === 0;
}

const cycles = [
    { langs: ["fr-FR", "en-US"], prefix: "fr" },
    { langs: ["ja-JP", "en-US"], prefix: "ja" },
    { langs: ["de-DE", "en-US"], prefix: "de" },
    { langs: ["en-US"], prefix: "en" },
];

let firstResolved = null;

function runCycle(i)
{
    if (i >= cycles.length) {
        afterCycles();
        return;
    }
    const { langs, prefix } = cycles[i];
    $vm.setUserPreferredLanguages(langs);
    setTimeout(() => {
        const undef = new Intl.DateTimeFormat();
        const undefAgain = new Intl.DateTimeFormat();

        if (!startsWith(undef.resolvedOptions().locale, prefix))
            throw new Error(`cycle ${i} [${langs}] expected locale to start with "${prefix}", got "${undef.resolvedOptions().locale}"`);

        expect(`cycle ${i} back-to-back constructions agree`,
            undefAgain.resolvedOptions().locale, undef.resolvedOptions().locale);

        // At least one cycle must move the resolution off the first one we saw.
        if (firstResolved === null)
            firstResolved = undef.resolvedOptions().locale;
        else if (i === cycles.length - 1) {
            if (firstResolved === undef.resolvedOptions().locale)
                throw new Error(`cache leaked across language changes — same resolved locale "${firstResolved}" everywhere`);
        }

        runCycle(i + 1);
    }, 0);
}

function afterCycles()
{
    // format() output must also move with the language, not just resolvedOptions.
    $vm.setUserPreferredLanguages(["ja-JP"]);
    setTimeout(() => {
        const ja = new Intl.DateTimeFormat().format(0);

        $vm.setUserPreferredLanguages(["en-US"]);
        setTimeout(() => {
            const en = new Intl.DateTimeFormat().format(0);
            if (ja === en)
                throw new Error(`format() did not change with language: ja="${ja}" en="${en}"`);
        }, 0);
    }, 0);
}

runCycle(0);
