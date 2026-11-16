//@ requireOptions("--useTemporal=1")

// Adding/subtracting a huge number of days must throw RangeError instead of
// crashing (Debug ASSERT: day >= 1 && day <= 31 in ExactTime::fromISOPartsAndOffset).

function shouldThrow(func, errorType) {
    let threw = false;
    try { func(); } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected ${errorType.name} but no exception was thrown`);
}

shouldThrow(() => new Temporal.PlainDate(2024, 1, 1).subtract({ days: 149645246 }), RangeError);
shouldThrow(() => new Temporal.PlainDate(2024, 1, 1).subtract({ days: 2000000000 }), RangeError);
shouldThrow(() => new Temporal.PlainDate(2024, 1, 1).add({ days: 149645246 }), RangeError);
shouldThrow(() => new Temporal.PlainDate(2024, 1, 1).add({ days: 2000000000 }), RangeError);

shouldThrow(() => {
    Temporal.Duration.from({ hours: -4294967295 }).round({ smallestUnit: "years", roundingMode: "halfEven", relativeTo: "2020-02-29" });
}, RangeError);
