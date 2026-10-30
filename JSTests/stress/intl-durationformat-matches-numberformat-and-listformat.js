function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

const units = ["years", "months", "weeks", "days", "hours", "minutes", "seconds", "milliseconds", "microseconds", "nanoseconds"];

const durations = [
    { seconds: 0 },
    { minutes: 5 },
    { nanoseconds: 1 },
    { hours: 1, minutes: 30, seconds: 15 },
    { years: 1, months: 2, weeks: 3, days: 4, hours: 5, minutes: 6, seconds: 7, milliseconds: 8, microseconds: 9, nanoseconds: 10 },
    { years: 4294967295, months: 4294967295, weeks: 4294967295, days: 1234567, hours: 1234567, minutes: 1234567, seconds: 1234567, milliseconds: 1234567, microseconds: 1234567, nanoseconds: 1234567 },
    { seconds: 2 ** 51 - 1 },
    { seconds: 2 ** 51 },
    { seconds: 2 ** 53 - 1 },
    { days: 1, milliseconds: 2 ** 53 },
    { days: 1, microseconds: 2 ** 63 },
    { days: 1, nanoseconds: 2 ** 80 },
];

for (const [locale, style] of [["en", "long"], ["en", "short"], ["en", "narrow"], ["fr", "narrow"], ["ja", "long"], ["ar", "short"]]) {
    const durationFormat = new Intl.DurationFormat(locale, { style });
    const listFormat = new Intl.ListFormat(locale, { type: "unit", style });
    const numberFormats = { };
    for (const unit of units)
        numberFormats[unit] = new Intl.NumberFormat(locale, { style: "unit", unit: unit.slice(0, -1), unitDisplay: style });

    for (const duration of durations) {
        const list = [];
        for (const unit of units) {
            if (duration[unit])
                list.push(numberFormats[unit].format(duration[unit]));
        }
        const expected = listFormat.format(list);
        shouldBe(durationFormat.format(duration), expected);
        shouldBe(durationFormat.formatToParts(duration).map((part) => part.value).join(""), expected);
        shouldBe(durationFormat.format(duration), expected);
    }
}

{
    const durationFormat = new Intl.DurationFormat("en", { style: "short", hoursDisplay: "always" });
    shouldBe(durationFormat.format({ minutes: -1 }), "-0 hr, 1 min");
    shouldBe(durationFormat.format({ minutes: 1 }), "0 hr, 1 min");
    shouldBe(durationFormat.format({ hours: -1, minutes: -1 }), "-1 hr, 1 min");
    shouldBe(JSON.stringify(durationFormat.formatToParts({ minutes: -1 })), `[{"type":"minusSign","value":"-","unit":"hour"},{"type":"integer","value":"0","unit":"hour"},{"type":"literal","value":" ","unit":"hour"},{"type":"unit","value":"hr","unit":"hour"},{"type":"literal","value":", "},{"type":"integer","value":"1","unit":"minute"},{"type":"literal","value":" ","unit":"minute"},{"type":"unit","value":"min","unit":"minute"}]`);
}

{
    const durationFormat = new Intl.DurationFormat("en", { style: "digital" });
    shouldBe(durationFormat.format({ hours: 1, minutes: 2, seconds: 3 }), "1:02:03");
    shouldBe(durationFormat.format({ minutes: -2 }), "-0:02:00");
    shouldBe(durationFormat.format({ days: 1, hours: 1, minutes: 2, seconds: 3 }), "1 day, 1:02:03");
    shouldBe(durationFormat.format({ seconds: 2 ** 53 - 1 }), "0:00:9007199254740991");
    shouldBe(durationFormat.formatToParts({ days: 1, hours: 1, minutes: 2, seconds: 3 }).map((part) => part.value).join("|"), "1| |day|, |1|:|02|:|03");
}

for (const locale of ["da", "fi", "en-u-nu-hanidec"]) {
    const durationFormat = new Intl.DurationFormat(locale, { style: "digital" });
    const duration = { days: 1, hours: 1, minutes: 2, seconds: 3 };
    shouldBe(durationFormat.formatToParts(duration).map((part) => part.value).join(""), durationFormat.format(duration));
}

shouldBe(new Intl.DurationFormat("en").format({ seconds: 0 }), "");
shouldBe(new Intl.DurationFormat("en").formatToParts({ seconds: 0 }).length, 0);
