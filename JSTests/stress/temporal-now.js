//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${String(expected)}, got ${String(actual)}`);
}
function shouldThrow(fn, ctor, label) {
    let caught = null;
    try { fn(); } catch (e) { caught = e; }
    if (!caught) throw new Error(`${label}: expected ${ctor.name}, got no throw`);
    if (!(caught instanceof ctor))
        throw new Error(`${label}: expected ${ctor.name}, got ${caught.constructor.name}`);
}

// Types + calendar defaults.
shouldBe(Temporal.Now.instant() instanceof Temporal.Instant, true, "instant is Instant");
shouldBe(Temporal.Now.plainDateISO().calendarId, "iso8601", "plainDateISO calendar");
shouldBe(Temporal.Now.plainDateTimeISO().calendarId, "iso8601", "plainDateTimeISO calendar");
shouldBe(Temporal.Now.zonedDateTimeISO().calendarId, "iso8601", "zonedDateTimeISO calendar");
shouldBe(typeof Temporal.Now.timeZoneId(), "string", "timeZoneId returns string");

// Explicit tz string.
shouldBe(Temporal.Now.zonedDateTimeISO("UTC").timeZoneId, "UTC", "explicit tz UTC");
shouldBe(Temporal.Now.zonedDateTimeISO("Asia/Tokyo").timeZoneId, "Asia/Tokyo", "explicit tz IANA");
shouldBe(Temporal.Now.zonedDateTimeISO("+05:30").timeZoneId, "+05:30", "explicit tz offset");

// ZDT arg forwards its [[TimeZone]], not its getter.
{
    class SpyZDT extends Temporal.ZonedDateTime {
        get timeZoneId() { throw new Error("getter must not fire"); }
    }
    const z = new SpyZDT(0n, "America/New_York");
    shouldBe(Temporal.Now.zonedDateTimeISO(z).timeZoneId, "America/New_York", "ZDT arg forwards [[TimeZone]]");
}

// Arg validation.
shouldThrow(() => Temporal.Now.plainDateISO("bogus"), RangeError, "invalid tz string");
shouldThrow(() => Temporal.Now.plainDateISO(42), TypeError, "number tz");
shouldThrow(() => Temporal.Now.plainDateISO({}), TypeError, "object tz");
shouldThrow(() => Temporal.Now.plainDateISO(Symbol()), TypeError, "symbol tz");
shouldThrow(() => Temporal.Now.plainDateTimeISO("+05:30:15"), RangeError, "sub-minute offset rejected");
shouldThrow(() => Temporal.Now.plainDateTimeISO(""), RangeError, "empty tz string");

// arg.toString must NOT be called (spec forbids object coercion for tz).
{
    let called = 0;
    try { Temporal.Now.plainDateISO({ toString() { called++; return "UTC"; } }); } catch (e) {}
    shouldBe(called, 0, "no arg.toString coercion");
}

// timeZoneId ignores extra args.
shouldBe(Temporal.Now.timeZoneId("extra"), Temporal.Now.timeZoneId(), "timeZoneId ignores args");

// Freshness: successive calls produce non-decreasing epoch.
{
    let prev = 0n;
    for (let i = 0; i < 20; i++) {
        const n = Temporal.Now.instant().epochNanoseconds;
        if (n < prev) throw new Error("instant not monotonic");
        prev = n;
    }
}

// Cross-fn consistency: date derived via plainDateISO() must match date part of
// plainDateTimeISO() and zonedDateTimeISO(). We compare across three near-simultaneous
// calls; they can disagree only if a day boundary crosses between them, so retry.
for (let retry = 0; retry < 3; retry++) {
    const a = Temporal.Now.plainDateISO().toString();
    const b = Temporal.Now.plainDateTimeISO().toString().slice(0, 10);
    const c = Temporal.Now.zonedDateTimeISO().toPlainDate().toString();
    if (a === b && b === c) break;
    if (retry === 2) throw new Error(`day-boundary consistency: ${a} vs ${b} vs ${c}`);
}

// Cross-fn consistency: default tz result must match explicit system-tz result.
{
    const tz = Temporal.Now.timeZoneId();
    shouldBe(Temporal.Now.plainDateISO().equals(Temporal.Now.plainDateISO(tz)), true, "default tz date == explicit tz date");
}

// Calendar override post-creation works (Now.* always returns iso8601, then user can convert).
shouldBe(Temporal.Now.plainDateISO().withCalendar("japanese").calendarId, "japanese", "withCalendar japanese");
shouldBe(Temporal.Now.zonedDateTimeISO("UTC").withCalendar("gregory").calendarId, "gregory", "withCalendar gregory on ZDT");

// undefined arg == no arg.
shouldBe(Temporal.Now.plainDateISO().equals(Temporal.Now.plainDateISO(undefined)), true, "undefined vs no arg (date)");
