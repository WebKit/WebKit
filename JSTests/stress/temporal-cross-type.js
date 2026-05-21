//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

// --- ZDT → PlainDate → ZDT round-trip ---
{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30[America/New_York]");
    const date = original.toPlainDate();
    shouldBe(date.year, 2024, "extracted year");
    shouldBe(date.month, 6, "extracted month");
    shouldBe(date.day, 15, "extracted day");

    // Round-trip back to ZDT at midnight
    const roundTrip = date.toZonedDateTime("America/New_York");
    shouldBe(roundTrip.year, 2024, "round-trip year");
    shouldBe(roundTrip.hour, 0, "round-trip goes to midnight");
}

// --- ZDT → PlainDateTime → ZDT round-trip ---
{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30:45.123456789[America/New_York]");
    const pdt = original.toPlainDateTime();
    shouldBe(pdt.hour, 14, "pdt hour");
    shouldBe(pdt.nanosecond, 789, "pdt nanosecond preserved");

    const roundTrip = pdt.toZonedDateTime("America/New_York");
    shouldBe(roundTrip.epochNanoseconds, original.epochNanoseconds, "PDT round-trip preserves epoch");
}

// --- ZDT → Instant → ZDT round-trip ---
{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30[America/New_York]");
    const instant = original.toInstant();
    const roundTrip = instant.toZonedDateTimeISO("America/New_York");
    shouldBe(roundTrip.epochNanoseconds, original.epochNanoseconds, "Instant round-trip preserves epoch");
    shouldBe(roundTrip.hour, original.hour, "Instant round-trip preserves hour");
}

// --- PlainDate + PlainTime → PlainDateTime → components ---
{
    const date = Temporal.PlainDate.from("2024-03-15");
    const time = Temporal.PlainTime.from("10:30:00");
    const dt = date.toPlainDateTime(time);
    shouldBe(dt.year, 2024, "combined year");
    shouldBe(dt.hour, 10, "combined hour");
    shouldBe(dt.minute, 30, "combined minute");

    const backDate = dt.toPlainDate();
    const backTime = dt.toPlainTime();
    shouldBe(backDate.equals(date), true, "decomposed date matches");
    shouldBe(Temporal.PlainTime.compare(backTime, time), 0, "decomposed time matches");
}

// --- PlainDate → PlainYearMonth → PlainDate ---
{
    const date = Temporal.PlainDate.from("2024-03-15");
    const ym = date.toPlainYearMonth();
    shouldBe(ym.year, 2024, "ym year");
    shouldBe(ym.month, 3, "ym month");

    const backDate = ym.toPlainDate({ day: 15 });
    shouldBe(backDate.equals(date), true, "YM round-trip with day");
}

// --- PlainDate → PlainMonthDay → PlainDate ---
{
    const date = Temporal.PlainDate.from("2024-03-15");
    const md = date.toPlainMonthDay();
    shouldBe(md.monthCode, "M03", "md monthCode");
    shouldBe(md.day, 15, "md day");

    const backDate = md.toPlainDate({ year: 2024 });
    shouldBe(backDate.equals(date), true, "MD round-trip with year");
}

// --- Temporal.Now methods consistency ---
{
    const before = Temporal.Now.instant();
    const zdtISO = Temporal.Now.zonedDateTimeISO();
    const pdtISO = Temporal.Now.plainDateTimeISO();
    const pdISO = Temporal.Now.plainDateISO();
    const ptISO = Temporal.Now.plainTimeISO();
    const after = Temporal.Now.instant();

    // All should be between before and after (or equal)
    shouldBe(Temporal.Instant.compare(before, after) <= 0, true, "before <= after");

    // ZDT and PDT should agree on date
    shouldBe(zdtISO.year, pdtISO.year, "Now: ZDT and PDT year match");
    shouldBe(zdtISO.month, pdtISO.month, "Now: ZDT and PDT month match");
    shouldBe(zdtISO.day, pdtISO.day, "Now: ZDT and PDT day match");

    // PD should match date
    shouldBe(pdISO.year, pdtISO.year, "Now: PD and PDT year match");
    shouldBe(pdISO.month, pdtISO.month, "Now: PD and PDT month match");
}

// --- toString → from round-trip ---
{
    const zdt = Temporal.ZonedDateTime.from("2024-06-15T14:30:00+09:00[Asia/Tokyo]");
    const str = zdt.toString();
    const parsed = Temporal.ZonedDateTime.from(str);
    shouldBe(parsed.epochNanoseconds, zdt.epochNanoseconds, "ZDT string round-trip");

    const inst = Temporal.Instant.from("2024-06-15T05:30:00Z");
    const instStr = inst.toString();
    const instParsed = Temporal.Instant.from(instStr);
    shouldBe(instParsed.epochNanoseconds, inst.epochNanoseconds, "Instant string round-trip");

    const dur = Temporal.Duration.from("P1Y2M3DT4H5M6S");
    const durStr = dur.toString();
    const durParsed = Temporal.Duration.from(durStr);
    shouldBe(durParsed.years, 1, "Duration string round-trip years");
    shouldBe(durParsed.seconds, 6, "Duration string round-trip seconds");
}
