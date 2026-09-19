function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const time = Date.UTC(2026, 0, 1, 10, 5);

function roundTrip(locale, options) {
    let original = new Intl.DateTimeFormat(locale, { timeZone: "UTC", ...options });
    let resolved = original.resolvedOptions();
    let recreated = new Intl.DateTimeFormat(locale, resolved);
    return { original, resolved, recreated };
}

// The AM/PM marker is controlled by hourCycle / hour12, not by dayPeriod.
{
    let { original, resolved, recreated } = roundTrip("en-US", { hour: "numeric", minute: "2-digit" });
    shouldBe(original.format(time), "10:05 AM");
    shouldBe(resolved.hourCycle, "h12");
    shouldBe(resolved.hour12, true);
    shouldBe("dayPeriod" in resolved, false);
    shouldBe(recreated.format(time), "10:05 AM");
    shouldBe(recreated.resolvedOptions().hourCycle, "h12");
    shouldBe("dayPeriod" in recreated.resolvedOptions(), false);
}

{
    let { original, resolved, recreated } = roundTrip("en-US", { hour: "numeric" });
    shouldBe(original.format(time), "10 AM");
    shouldBe("dayPeriod" in resolved, false);
    shouldBe(recreated.format(time), "10 AM");
}

{
    let { original, resolved, recreated } = roundTrip("en-US", { hour: "numeric", hour12: true });
    shouldBe(original.format(time), "10 AM");
    shouldBe("dayPeriod" in resolved, false);
    shouldBe(recreated.format(time), "10 AM");
}

{
    let { original, resolved, recreated } = roundTrip("en", { hour: "numeric", minute: "numeric", second: "numeric", fractionalSecondDigits: 1, timeZone: "America/Los_Angeles" });
    shouldBe("dayPeriod" in resolved, false);
    shouldBe(recreated.format(time), original.format(time));
}

for (let locale of ["en-GB", "ja", "ko", "zh-Hant", "hi", "ar", "th", "de", "ru", "fr"]) {
    for (let hour12 of [true, false]) {
        let { original, resolved, recreated } = roundTrip(locale, { hour: "numeric", minute: "numeric", hour12 });
        shouldBe("dayPeriod" in resolved, false);
        shouldBe(recreated.format(time), original.format(time));
        shouldBe(recreated.resolvedOptions().hourCycle, resolved.hourCycle);
    }
}

// The AM/PM marker is still reported as a dayPeriod part by formatToParts.
{
    let parts = new Intl.DateTimeFormat("en-US", { timeZone: "UTC", hour: "numeric", minute: "2-digit" }).formatToParts(time);
    shouldBe(JSON.stringify(parts.filter((part) => part.type !== "literal")), JSON.stringify([
        { type: "hour", value: "10" },
        { type: "minute", value: "05" },
        { type: "dayPeriod", value: "AM" },
    ]));
}

// timeStyle and dateStyle do not report component fields.
{
    let { original, resolved, recreated } = roundTrip("en-US", { timeStyle: "short" });
    shouldBe(original.format(time), "10:05 AM");
    shouldBe("dayPeriod" in resolved, false);
    shouldBe(recreated.format(time), "10:05 AM");
}

// An explicitly requested dayPeriod is still reported and survives the round trip.
for (let dayPeriod of ["narrow", "short", "long"]) {
    for (let locale of ["en-US", "zh-Hant"]) {
        let { original, resolved, recreated } = roundTrip(locale, { hour: "numeric", dayPeriod });
        shouldBe(resolved.dayPeriod, dayPeriod);
        shouldBe(recreated.resolvedOptions().dayPeriod, dayPeriod);
        shouldBe(recreated.format(time), original.format(time));
    }
}

{
    let { original, resolved } = roundTrip("en-US", { hour: "numeric", minute: "2-digit", dayPeriod: "short" });
    shouldBe(original.format(time), "10:05 in the morning");
    shouldBe(resolved.dayPeriod, "short");
    shouldBe(Object.keys(resolved).join(), "locale,calendar,numberingSystem,timeZone,hourCycle,hour12,dayPeriod,hour,minute");
}
