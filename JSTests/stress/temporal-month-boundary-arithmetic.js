//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

{
    const jan31 = Temporal.PlainDate.from("2024-01-31");
    const result = jan31.add({ months: 1 });
    shouldBe(result.month, 2, "Jan 31 + 1M = Feb");
    shouldBe(result.day, 29, "Jan 31 + 1M = Feb 29 (2024 leap)");
}

{
    const jan31 = Temporal.PlainDate.from("2023-01-31");
    const result = jan31.add({ months: 1 });
    shouldBe(result.month, 2, "Jan 31 + 1M = Feb (non-leap)");
    shouldBe(result.day, 28, "Jan 31 + 1M = Feb 28 (2023)");
}

{
    const feb29 = Temporal.PlainDate.from("2024-02-29");
    const result = feb29.add({ years: 1 });
    shouldBe(result.year, 2025, "Feb 29 + 1Y = 2025");
    shouldBe(result.day, 28, "Feb 29 + 1Y = Feb 28 (constrain)");
}

{
    const jan31 = Temporal.PlainDate.from("2024-01-31");
    const feb = jan31.add({ months: 1 }); // Feb 29
    const mar = feb.add({ months: 1 });   // Mar 29 (not 31!)
    shouldBe(mar.month, 3, "chain month");
    shouldBe(mar.day, 29, "chain day preserves constrained day");
}

{
    const jan1 = Temporal.PlainDate.from("2024-01-01");
    const result = jan1.subtract({ days: 1 });
    shouldBe(result.year, 2023, "Jan 1 - 1d = Dec 31 prev year");
    shouldBe(result.month, 12, "Dec");
    shouldBe(result.day, 31, "31st");
}

{
    const nov15 = Temporal.PlainDate.from("2024-11-15");
    const result = nov15.add({ months: 3 });
    shouldBe(result.year, 2025, "Nov + 3M = Feb next year");
    shouldBe(result.month, 2, "February");
    shouldBe(result.day, 15, "day preserved");
}

{
    const jan31 = Temporal.PlainDate.from("2024-01-31");
    const mar31 = Temporal.PlainDate.from("2024-03-31");
    const dur = jan31.until(mar31, { largestUnit: "months" });
    shouldBe(dur.months, 2, "Jan 31 to Mar 31 = 2 months");
    shouldBe(dur.days, 0, "exact month boundary");
}

{
    const jan31 = Temporal.PlainDate.from("2024-01-31");
    const feb29 = Temporal.PlainDate.from("2024-02-29");
    const dur = jan31.until(feb29, { largestUnit: "months" });
    shouldBe(dur.months, 0, "Jan 31 to Feb 29 = 0 months + days");
    shouldBe(dur.days, 29, "29 days");
}

{
    const pdt = Temporal.PlainDateTime.from("2024-01-31T23:30:00");
    const result = pdt.add({ months: 1, hours: 1 });
    shouldBe(result.month, 3, "Jan 31 23:30 + 1M1H = Mar 1 00:30");
    shouldBe(result.day, 1, "day rolled to 1st");
    shouldBe(result.hour, 0, "hour rolled from 23+1=24→0");
}

{
    const mon = Temporal.PlainDate.from("2024-01-01"); // Monday
    const result = mon.add({ weeks: 1 });
    shouldBe(result.day, 8, "+1 week = +7 days");
    shouldBe(result.dayOfWeek, 1, "same day of week (Monday)");
}

{
    const mar15 = Temporal.PlainDate.from("2024-03-15");
    const result = mar15.subtract({ days: 45 });
    shouldBe(result.year, 2024, "year");
    shouldBe(result.month, 1, "Jan (skipped Feb)");
    shouldBe(result.day, 30, "Jan 30");
}
