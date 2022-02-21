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

shouldBe(Temporal.PlainDate instanceof Function, true);
shouldBe(Temporal.PlainDate.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDate, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDate, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDate, 'prototype').configurable, false);
shouldBe(Temporal.PlainDate.prototype.constructor, Temporal.PlainDate);

{
    let date = new Temporal.PlainDate(1, 2, 3);
    shouldBe(String(date), `0001-02-03`);
    shouldBe(date.year, 1);
    shouldBe(date.month, 2);
    shouldBe(date.day, 3);
}

{
    let date = new Temporal.PlainDate(2007, 1, 9);
    shouldBe(String(date), `2007-01-09`);
    shouldBe(date.year, 2007);
    shouldBe(date.month, 1);
    shouldBe(date.day, 9);
}


shouldBe(String(Temporal.PlainDate.from('2007-01-09')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09t03:24:30')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30Z')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+20:20:59')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30-20:20:59')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30\u221220:20:59')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+10')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+1020')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+102030')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+10:20:30.05')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+10:20:30.123456789')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[Europe/Brussels]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[Europe/Brussels]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[UNKNOWN]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[.hey]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_-]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_/_]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_-./_-.]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_../_..]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[_./_.]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[Etc/GMT+20]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[Etc/GMT-20]')), `2007-01-09`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00:00]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00:00.123]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00:00.12345]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00:00.12345678]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[+01:00:00.123456789]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[-01:00]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[\u221201:00]')), `2007-01-09`);
{
    let date = Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[Europe/Brussels]')
    shouldBe(date === Temporal.PlainDate.from(date), false);
}


// FIXME: from(object) is note yet implemented.
// {
//     let date = Temporal.PlainDate.from({
//       year: 2007,
//       month: 1,
//       day: 9
//     });
//     shouldBe(String(date), `2007-01-09`);
// }
shouldThrow(() => { Temporal.PlainDate.from({ year: 2007, month: 1, day: 9 }); }, RangeError);


shouldThrow(() => { new Temporal.PlainDate(-1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, 13, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, 2, 30); }, RangeError);

shouldThrow(() => { new Temporal.PlainDate(Infinity, 1, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(-Infinity, 1, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, Infinity, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, -Infinity, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, 1, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(2007, 1, -Infinity); }, RangeError);

let failures = [
    "",
    "23:59:61.999999999",
    "1995-1207T03:24:30",
    "199512-07T03:24:30",
    "2007-01-09T03:24:30+20:20:60",
    "2007-01-09T03:24:30+20:2050",
    "2007-01-09T03:24:30+:",
    "2007-01-09T03:24:30+2:50",
    "2007-01-09T03:24:30+01:00[/]",
    "2007-01-09T03:24:30+01:00[///]",
    "2007-01-09T03:24:30+01:00[Hey/Hello",
    "2007-01-09T03:24:30+01:00[]",
    "2007-01-09T03:24:30+01:00[Hey/]",
    "2007-01-09T03:24:30+01:00[..]",
    "2007-01-09T03:24:30+01:00[.]",
    "2007-01-09T03:24:30+01:00[./.]",
    "2007-01-09T03:24:30+01:00[../..]",
    "2007-01-09T03:24:30+01:00[-Hey/Hello]",
    "2007-01-09T03:24:30+01:00[-]",
    "2007-01-09T03:24:30+01:00[-/_]",
    "2007-01-09T03:24:30+01:00[_/-]",
    "2007-01-09T03:24:30+01:00[CocoaCappuccinoMatcha]",
    "2007-01-09T03:24:30+01:00[Etc/GMT+50]",
    "2007-01-09T03:24:30+01:00[Etc/GMT+0]",
    "2007-01-09T03:24:30+01:00[Etc/GMT0]",
    "2007-01-09T03:24:30+10:20:30.0123456789",
    "2007-01-09 03:24:30+01:00[Etc/GMT\u221201]",
    "2007-01-09 03:24:30+01:00[+02:00:00.0123456789]",
    "2007-01-09 03:24:30+01:00[02:00:00.123456789]",
    "2007-01-09 03:24:30+01:00[02:0000.123456789]",
    "2007-01-09 03:24:30+01:00[0200:00.123456789]",
    "2007-01-09 03:24:30+01:00[02:00:60.123456789]",
];

for (let text of failures) {
    shouldThrow(() => {
        print(String(Temporal.PlainDate.from(text)));
    }, RangeError);
}

// FIXME: This relies on Temporal.PlainDate.from(object).
// {
//     let one = Temporal.PlainDate.from('1001-01-01');
//     let two = Temporal.PlainDate.from('1002-01-01');
//     let three = Temporal.PlainDate.from('1000-02-02');
//     let four = Temporal.PlainDate.from('1001-01-02');
//     let five = Temporal.PlainDate.from('1001-02-01');
//     let sorted = [one, two, three, four, five].sort(Temporal.PlainDate.compare);
//     shouldBe(sorted.join(' '), `1000-02-02 1001-01-01 1001-01-02 1001-02-01 1002-01-01`);
// }
