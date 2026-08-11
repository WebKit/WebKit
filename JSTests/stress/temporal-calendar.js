//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

// Stage 4: Temporal.Calendar constructor is removed
shouldThrow(() => { new Temporal.Calendar("iso8601"); }, TypeError);
shouldThrow(() => { Temporal.Calendar.from("iso8601"); }, TypeError);

// Calendar is accessed via calendarId string on Temporal types
{
    let pd = Temporal.PlainDate.from("2024-03-15");
    shouldBe(pd.calendarId, "iso8601");
}
{
    let pd = Temporal.PlainDate.from("2024-03-15[u-ca=gregory]");
    shouldBe(pd.calendarId, "gregory");
}

// Calendar-aware arithmetic via PlainDate
{
    let date = Temporal.PlainDate.from("2020-02-28");
    shouldBe(date.add({ days: 1 }).toString(), "2020-02-29");
    shouldBe(date.add({ months: 1 }).toString(), "2020-03-28");
    shouldBe(date.add({ years: 1 }).toString(), "2021-02-28");
}

// dateUntil via PlainDate.until
{
    let d1 = Temporal.PlainDate.from("2020-02-28");
    let d2 = Temporal.PlainDate.from("2021-02-28");
    shouldBe(d1.until(d2).toString(), "P366D");
    shouldBe(d1.until(d2, { largestUnit: "year" }).toString(), "P1Y");
}
