//@ requireOptions("--useTemporal=1")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined)
        shouldBe(String(error), message);
}

shouldBe(Temporal.Calendar instanceof Function, true);
shouldBe(Temporal.Calendar.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Calendar, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Calendar, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Calendar, 'prototype').configurable, false);
shouldBe(Temporal.Calendar.prototype.constructor, Temporal.Calendar);

const isoCalendar = new Temporal.Calendar("iso8601");
shouldBe(isoCalendar.id, `iso8601`);
shouldBe(String(isoCalendar), `iso8601`);

{
    let calendar = new Temporal.Calendar("gregory");
    shouldBe(calendar.id, `gregory`);
    shouldBe(String(calendar), `gregory`);
}

shouldThrow(() => {
    new Temporal.Calendar("discordian");
}, RangeError, `RangeError: invalid calendar ID`);

{
    let input = ["monthCode", "day"];
    let fields = Temporal.Calendar.from("iso8601").fields(input);
    shouldBe(input !== fields, true);
    shouldBe(JSON.stringify(fields), `["monthCode","day"]`);
}
{
    let input = ["monthCode", "day"];
    let fields = Temporal.Calendar.from("gregory").fields(input);
    shouldBe(input !== fields, true);
    shouldBe(JSON.stringify(fields), `["monthCode","day"]`);
}
{
    let input = ["monthCode", "day", "year"];
    let fields = Temporal.Calendar.from("iso8601").fields(input);
    shouldBe(input !== fields, true);
    shouldBe(JSON.stringify(fields), `["monthCode","day","year"]`);
}

{
    let input = ["monthCode", "day", "year"];
    let fields = Temporal.Calendar.from("gregory").fields(input);
    shouldBe(input !== fields, true);
    shouldBe(JSON.stringify(fields), `["monthCode","day","year","era","eraYear"]`);
}
{
    let merged = Temporal.Calendar.from('iso8601').mergeFields(
      { year: 2006, month: 7, day: 31 },
      { monthCode: 'M08' }
    );
    shouldBe(JSON.stringify(merged), `{"year":2006,"day":31,"monthCode":"M08"}`);
}
{
    let merged = Temporal.Calendar.from('gregory').mergeFields(
      { year: 2006, month: 7, day: 31 },
      { monthCode: 'M08' }
    );
    shouldBe(JSON.stringify(merged), `{"year":2006,"day":31,"monthCode":"M08"}`);
}

{
    const user = {
        id: 775,
        username: 'robotcat',
        password: 'hunter2', // Note: Don't really store passwords like that
        userCalendar: Temporal.Calendar.from('gregory')
    };
    let string = JSON.stringify(user);
    shouldBe(string, `{"id":775,"username":"robotcat","password":"hunter2","userCalendar":"gregory"}`);
    // To rebuild from the string:
    function reviver(key, value) {
        if (key.endsWith('Calendar'))
            return Temporal.Calendar.from(value);
        return value;
    }
    shouldBe(JSON.parse(string, reviver).userCalendar instanceof Temporal.Calendar, true);
}

shouldBe(isoCalendar.toString(), isoCalendar.toJSON());
shouldThrow(() => { Temporal.Calendar.prototype.toString.call({}); }, TypeError);
shouldThrow(() => { Temporal.Calendar.prototype.toJSON.call({}); }, TypeError);

shouldBe(Temporal.Calendar.prototype.dateFromFields.length, 1);
shouldBe(isoCalendar.dateFromFields({ year: 2007, month: 1, day: 9 }).toString(), '2007-01-09');
shouldBe(isoCalendar.dateFromFields({ year: 2007, monthCode: 'M01', day: 9 }).toString(), '2007-01-09');
shouldBe(isoCalendar.dateFromFields({ year: 2007, month: 20, day: 40 }).toString(), '2007-12-31');
shouldBe(isoCalendar.dateFromFields({ year: 2007, month: 20, day: 40 }, { overflow: 'constrain' }).toString(), '2007-12-31');

shouldThrow(() => { isoCalendar.dateFromFields(null); }, TypeError);
shouldThrow(() => { isoCalendar.dateFromFields({ month: 1, day: 9 }); }, TypeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, day: 9 }); }, TypeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, month: 1 }); }, TypeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: Infinity, month: 1, day: 9 }); }, RangeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, month: 0, day: 9 }); }, RangeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, monthCode: 'M00', day: 9 }); }, RangeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, month: 1, day: 0 }); }, RangeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, month: 1, monthCode: 'M02', day: 9 }); }, RangeError);
shouldThrow(() => { isoCalendar.dateFromFields({ year: 2007, month: 20, day: 40 }, { overflow: 'reject' }); }, RangeError);

shouldBe(Temporal.Calendar.prototype.dateAdd.length, 2);
shouldBe(isoCalendar.dateAdd('2020-02-28', new Temporal.Duration()).toString(), '2020-02-28');
shouldBe(isoCalendar.dateAdd('2020-02-28', { years: 1, months: 1 }).toString(), '2021-03-28');
shouldBe(isoCalendar.dateAdd('2020-02-28', 'P1W1D').toString(), '2020-03-07');
shouldBe(isoCalendar.dateAdd('2020-02-28', { hours: 24 }).toString(), '2020-02-29');
shouldBe(isoCalendar.dateAdd('2020-02-28', { days: 36500000 }).toString(), '+101953-10-07');
shouldBe(isoCalendar.dateAdd('2020-02-28', new Temporal.Duration(-1, -1, 0, -1)).toString(), '2019-01-27');
shouldBe(isoCalendar.dateAdd('2020-01-30', { months: 1 }).toString(), '2020-02-29');
shouldThrow(() => { isoCalendar.dateAdd('2020-01-30', { months: 1 }, { overflow: 'reject' }); }, RangeError);
shouldThrow(() => { isoCalendar.dateAdd('2020-02-28', { years: 300000 }); }, RangeError);

shouldBe(isoCalendar.dateAdd('2020-02-28', { years: -1, months: -1 }).toString(), '2019-01-28');
shouldBe(isoCalendar.dateAdd('2020-02-28', '-P1W1D').toString(), '2020-02-20');
shouldBe(isoCalendar.dateAdd('2020-02-28', { hours: -24 }).toString(), '2020-02-27');
shouldBe(isoCalendar.dateAdd('2020-02-28', { days: -36500000 }).toString(), '-097914-07-21');
shouldBe(isoCalendar.dateAdd('2020-02-28', new Temporal.Duration(1, 1, 0, 1)).toString(), '2021-03-29');
shouldBe(isoCalendar.dateAdd('2020-03-30', { months: -1 }).toString(), '2020-02-29');
shouldThrow(() => { isoCalendar.dateAdd('2020-03-30', { months: -1 }, { overflow: 'reject' }); }, RangeError);
shouldThrow(() => { isoCalendar.dateAdd('2020-02-28', { years: -300000 }); }, RangeError);

shouldBe(Temporal.Calendar.prototype.dateUntil.length, 2);
shouldBe(isoCalendar.dateUntil('2020-02-28', '2019-02-28').toString(), '-P365D');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2019-02-28', { largestUnit: 'year' }).toString(), '-P1Y');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2021-02-28').toString(), 'P366D');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2021-02-28', { largestUnit: 'year' }).toString(), 'P1Y');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2020-04-28', { largestUnit: 'month' }).toString(), 'P2M');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2019-12-28', { largestUnit: 'month' }).toString(), '-P2M');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2020-03-15', { largestUnit: 'week' }).toString(), 'P2W2D');
shouldBe(isoCalendar.dateUntil('2020-02-28', '2020-02-12', { largestUnit: 'week' }).toString(), '-P2W2D');
shouldThrow(() => { isoCalendar.dateUntil('2020-02-28', '2019-02-28', { largestUnit: 'hour' }); }, RangeError);
