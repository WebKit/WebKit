//@ requireOptions("--useTemporal=1")

// Temporal.ZonedDateTime.prototype.toLocaleString must format in each
// ZonedDateTime's own time zone, even when (locales, options) are identical
// across calls, and must agree with its own explicit-options slow path.

function expect(label, got, want)
{
    if (got !== want)
        throw new Error(`${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
}

const instant = Temporal.Instant.from("2024-01-15T12:00:00Z");
const newYork = instant.toZonedDateTimeISO("America/New_York");
const tokyo = instant.toZonedDateTimeISO("Asia/Tokyo");

// Same instant, different zones: the rendered wall-clock time must differ.
{
    const newYorkString = newYork.toLocaleString("en-US");
    const tokyoString = tokyo.toLocaleString("en-US");
    if (newYorkString === tokyoString)
        throw new Error(`different time zones rendered identically: "${newYorkString}"`);
}

// Same shape with undefined locales.
{
    const newYorkString = newYork.toLocaleString();
    const tokyoString = tokyo.toLocaleString();
    if (newYorkString === tokyoString)
        throw new Error(`different time zones rendered identically (undefined locales): "${newYorkString}"`);
}

// Each must agree with the explicit-empty-options construction shape.
expect("newYork toLocaleString agrees with empty-options shape",
    newYork.toLocaleString("en-US"), newYork.toLocaleString("en-US", {}));
expect("tokyo toLocaleString agrees with empty-options shape",
    tokyo.toLocaleString("en-US"), tokyo.toLocaleString("en-US", {}));

// Interleaving zones must not leak one zone's rendering into the other.
for (let i = 0; i < 4; ++i) {
    expect(`interleaved newYork (round ${i})`,
        newYork.toLocaleString("en-US"), newYork.toLocaleString("en-US", {}));
    expect(`interleaved tokyo (round ${i})`,
        tokyo.toLocaleString("en-US"), tokyo.toLocaleString("en-US", {}));
}
