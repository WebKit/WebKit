//@ requireOptions("--useTemporal=1")

function shouldNotThrow(fn, desc) {
    try { fn(); } catch(e) { throw new Error(desc + " threw: " + e.constructor.name + ": " + e.message); }
}
function shouldBe(a, e) { if (a !== e) throw new Error("expected " + JSON.stringify(e) + " got " + JSON.stringify(a)); }

// Antarctica/Casey had a backward DST transition on 2010-03-05: clocks went from +11 to +08.
// This skipped midnight — the transition crossed midnight, causing some instants that have a
// local date of 2010-03-04 to have epochNs > GetStartOfDay(2010-03-05 in +11).
// ZonedDateTime.round({ smallestUnit: "day" }) must not throw for any rounding mode.
// Polyfill fix: https://github.com/tc39/proposal-temporal/issues/3312
{
    const zdt = Temporal.ZonedDateTime.from("2010-03-04T23:10:00+08:00[Antarctica/Casey]");
    for (const roundingMode of ["ceil", "floor", "trunc", "expand", "halfExpand", "halfTrunc", "halfCeil", "halfFloor", "halfEven"]) {
        shouldNotThrow(() => {
            const r = zdt.round({ smallestUnit: "day", roundingMode });
            shouldBe(r.timeZoneId, "Antarctica/Casey");
            // Result must be at a day boundary (time = 00:00:00 local).
            shouldBe(r.hour, 0);
            shouldBe(r.minute, 0);
            shouldBe(r.second, 0);
        }, `round(${roundingMode})`);
    }

    // floor/trunc round down → 2010-03-04 start.
    const floor = zdt.round({ smallestUnit: "day", roundingMode: "floor" });
    shouldBe(floor.toPlainDate().toString(), "2010-03-04");

    // ceil/expand round up → 2010-03-05 start.
    const ceil = zdt.round({ smallestUnit: "day", roundingMode: "ceil" });
    shouldBe(ceil.toPlainDate().toString(), "2010-03-05");
}
