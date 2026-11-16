//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30[America/New_York]");
    const pd = original.toPlainDate();
    shouldBe(pd.year, 2024, "ZDT→PD year");
    shouldBe(pd.month, 6, "ZDT→PD month");
    shouldBe(pd.day, 15, "ZDT→PD day");
    const back = pd.toZonedDateTime("America/New_York");
    shouldBe(back.year, 2024, "PD→ZDT year");
    shouldBe(back.timeZoneId, "America/New_York", "PD→ZDT tz");
}

{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30:45.123456789[America/New_York]");
    const pdt = original.toPlainDateTime();
    shouldBe(pdt.hour, 14, "ZDT→PDT hour");
    shouldBe(pdt.nanosecond, 789, "ZDT→PDT nanosecond");
    const back = pdt.toZonedDateTime("America/New_York");
    shouldBe(back.epochNanoseconds, original.epochNanoseconds, "PDT→ZDT round-trip ns");
}

{
    const original = Temporal.ZonedDateTime.from("2024-06-15T14:30[America/New_York]");
    const inst = original.toInstant();
    shouldBe(inst.epochNanoseconds, original.epochNanoseconds, "ZDT→Instant epochNs");
    const back = inst.toZonedDateTimeISO("America/New_York");
    shouldBe(back.epochNanoseconds, original.epochNanoseconds, "Instant→ZDT round-trip");
}

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

{
    const date = Temporal.PlainDate.from("2024-03-15");
    const ym = date.toPlainYearMonth();
    shouldBe(ym.year, 2024, "ym year");
    shouldBe(ym.month, 3, "ym month");

    const backDate = ym.toPlainDate({ day: 15 });
    shouldBe(backDate.equals(date), true, "YM round-trip with day");
}

{
    const date = Temporal.PlainDate.from("2024-03-15");
    const md = date.toPlainMonthDay();
    shouldBe(md.monthCode, "M03", "md monthCode");
    shouldBe(md.day, 15, "md day");

    const backDate = md.toPlainDate({ year: 2024 });
    shouldBe(backDate.equals(date), true, "MD round-trip with year");
}

{
    const before = Temporal.Now.instant();
    const zdtISO = Temporal.Now.zonedDateTimeISO();
    const pdtISO = zdtISO.toPlainDateTime();
    const pdISO = zdtISO.toPlainDate();
    const ptISO = zdtISO.toPlainTime();
    const after = Temporal.Now.instant();

    shouldBe(Temporal.Instant.compare(before, after) <= 0, true, "before <= after");
    shouldBe(pdISO.year, pdtISO.year, "Now: PD and PDT year match");
    shouldBe(pdISO.month, pdtISO.month, "Now: PD and PDT month match");
    shouldBe(zdtISO.timeZoneId, Temporal.Now.timeZoneId(), "Now: ZDT and timeZoneId match");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-06-15T14:30:00+09:00[Asia/Tokyo]");
    const zdtParsed = Temporal.ZonedDateTime.from(zdt.toString());
    shouldBe(zdtParsed.epochNanoseconds, zdt.epochNanoseconds, "ZDT string round-trip");

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
