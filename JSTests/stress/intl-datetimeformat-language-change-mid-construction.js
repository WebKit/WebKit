//@ runDefault("--useDollarVM=1")

// A worker changes the user language from another thread while the main thread keeps
// constructing across VM entries. Language is latched per entry, so a cacheable no-arg
// formatter can lag the non-cacheable shape within a turn, but a turn boundary must heal
// it: if it stays pinned across many turns while the non-cacheable shape has moved on, an
// invalidation was dropped.

if (typeof $vm === "undefined" || typeof $vm.setUserPreferredLanguages !== "function")
    quit();

const langs = ["fr-FR", "ja-JP", "de-DE", "en-US"];

$.agent.start(`
    const langs = ${JSON.stringify(langs)};
    for (let i = 0; i < 400; ++i) {
        $vm.setUserPreferredLanguages([langs[i % langs.length]]);
        $.agent.sleep(1);
    }
    $.agent.report("done");
    $.agent.leaving();
`);

let pinned = 0;
let lastCached = null;
const distinctCached = new Set();
let turns = 0;

function turn()
{
    // Fresh VM entry: the cache is reset here if the language changed since the last turn.
    const cached = new Intl.DateTimeFormat().resolvedOptions().locale;            // cacheable
    const uncached = new Intl.DateTimeFormat(undefined, {}).resolvedOptions().locale; // not cacheable
    distinctCached.add(cached);

    if (cached === uncached)
        pinned = 0;
    else {
        // A frozen value is a dropped invalidation; one that keeps moving is just a turn behind.
        pinned = (cached === lastCached) ? pinned + 1 : 1;
        if (pinned >= 25)
            throw new Error(`cacheable resolution pinned at "${cached}" across ${pinned} turns while it should be "${uncached}"`);
    }
    lastCached = cached;

    if ($.agent.getReport() !== null) {
        finish();
        return;
    }
    if (++turns > 1000000)
        throw new Error("worker never reported");
    setTimeout(turn, 0);
}

function finish()
{
    // Flips stopped: a final turn must leave the two shapes in agreement.
    setTimeout(() => {
        const c = new Intl.DateTimeFormat().resolvedOptions().locale;
        const u = new Intl.DateTimeFormat(undefined, {}).resolvedOptions().locale;
        if (c !== u)
            throw new Error(`did not heal after flips stopped: cacheable "${c}" vs non-cacheable "${u}"`);
        if (distinctCached.size < 2)
            throw new Error(`main thread never tracked the worker's changes (only saw ${[...distinctCached]})`);
    }, 0);
}

setTimeout(turn, 0);
