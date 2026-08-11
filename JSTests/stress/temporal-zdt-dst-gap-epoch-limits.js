//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type, msg) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`${msg}: expected ${type.name} but got ${err}`);
}

function shouldBe(actual, expected, msg) {
    if (String(actual) !== String(expected))
        throw new Error(`${msg}: expected ${JSON.stringify(String(expected))} but got ${JSON.stringify(String(actual))}`);
}

// Sydney springs forward 02:00 -> 03:00 on the first Sunday in October, so 02:30 does not exist.
// +275760-10-05 is such a Sunday and is past the maximum representable epoch, so resolving the gap
// must be rejected rather than silently produce an out-of-range instant.
shouldThrow(() => Temporal.ZonedDateTime.from("+275760-10-05T02:30[Australia/Sydney]"),
    RangeError, "gap shift past max epoch");

// Every disambiguation that resolves a gap goes through the same re-entrant conversion.
for (const disambiguation of ["compatible", "earlier", "later"]) {
    shouldThrow(() => Temporal.ZonedDateTime.from("+275760-10-05T02:30[Australia/Sydney]", { disambiguation }),
        RangeError, `gap shift past max epoch (${disambiguation})`);
}

// ~reject~ throws for being a gap at all, before any range question arises.
shouldThrow(() => Temporal.ZonedDateTime.from("+275760-10-05T02:30[Australia/Sydney]", { disambiguation: "reject" }),
    RangeError, "gap with disambiguation reject");

// The same wall clock reached through the other entry point that funnels into
// GetEpochNanosecondsFor must reject too.
shouldThrow(() => Temporal.PlainDateTime.from("+275760-10-05T02:30").toZonedDateTime("Australia/Sydney"),
    RangeError, "PlainDateTime.toZonedDateTime into an out-of-range gap");

// A fold below the minimum epoch — Sydney falls back in April, so this covers
// getPossibleEpochNanosecondsFor's candidate range check, not the gap branch. (Sydney's gap is in
// October, which is inside the range at this year, so min-side gap coverage is not included here.)
shouldThrow(() => Temporal.ZonedDateTime.from("-271821-04-11T02:30[Australia/Sydney]"),
    RangeError, "fold below min epoch");

// --- Regression guards: in-range gaps and folds must still resolve exactly as before. ---

shouldBe(Temporal.ZonedDateTime.from("2023-10-01T02:30[Australia/Sydney]").toString(),
    "2023-10-01T03:30:00+11:00[Australia/Sydney]", "in-range Sydney gap, compatible");
shouldBe(Temporal.ZonedDateTime.from("2023-10-01T02:30[Australia/Sydney]", { disambiguation: "earlier" }).toString(),
    "2023-10-01T01:30:00+10:00[Australia/Sydney]", "in-range Sydney gap, earlier");
shouldBe(Temporal.ZonedDateTime.from("2023-10-01T02:30[Australia/Sydney]", { disambiguation: "later" }).toString(),
    "2023-10-01T03:30:00+11:00[Australia/Sydney]", "in-range Sydney gap, later");
shouldBe(Temporal.ZonedDateTime.from("2023-09-03T00:30[America/Santiago]").toString(),
    "2023-09-03T01:30:00-03:00[America/Santiago]", "in-range Santiago gap");

shouldBe(Temporal.ZonedDateTime.from("2023-04-02T02:30[Australia/Sydney]").toString(),
    "2023-04-02T02:30:00+11:00[Australia/Sydney]", "in-range Sydney fold, compatible");
shouldBe(Temporal.ZonedDateTime.from("2023-04-02T02:30[Australia/Sydney]", { disambiguation: "later" }).toString(),
    "2023-04-02T02:30:00+10:00[Australia/Sydney]", "in-range Sydney fold, later");

// The boundary stays reachable — the largest representable Sydney local time.
shouldBe(Temporal.ZonedDateTime.from("+275760-09-13T03:30[Australia/Sydney]").toString(),
    "+275760-09-13T03:30:00+10:00[Australia/Sydney]", "max in-range Sydney local time");
