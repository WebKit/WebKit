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
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[u-ca=japanese]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09 03:24:30+01:00[Europe/Brussels][u-ca=japanese]')), `2007-01-09`);
shouldBe(String(Temporal.PlainDate.from('2007-01-09[u-ca=japanese]')), `2007-01-09`);
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
shouldThrow(() => { new Temporal.PlainDate(0x43530, 9, 14); }, RangeError);
shouldBe(String(new Temporal.PlainDate(0x43530, 9, 13)), `275760-09-13`);
shouldThrow(() => { new Temporal.PlainDate(-0x425cd, 4, 19); }, RangeError);
shouldBe(String(new Temporal.PlainDate(-0x425cd, 4, 20)), `-271821-04-20`);
shouldThrow(() => { new Temporal.PlainDate(0x80000000, 1, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(-0x80000000, 1, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(0x7fffffff, 1, 1); }, RangeError);
shouldThrow(() => { new Temporal.PlainDate(-0x7fffffff, 1, 1); }, RangeError);

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
    "2007-01-09T03:24:30Z", // UTCDesignator
    "2007-01-09 03:24:30[u-ca=japanese][Europe/Brussels]",
    "2007-01-09 03:24:30+01:00[u-ca=japanese][Europe/Brussels]",
];

for (let text of failures) {
    shouldThrow(() => {
        print(String(Temporal.PlainDate.from(text)));
    }, RangeError);
}

{
    let one = Temporal.PlainDate.from('1001-01-01');
    let two = Temporal.PlainDate.from('1002-01-01');
    let three = Temporal.PlainDate.from('1000-02-02');
    let four = Temporal.PlainDate.from('1001-01-02');
    let five = Temporal.PlainDate.from('1001-02-01');
    let sorted = [one, two, three, four, five].sort(Temporal.PlainDate.compare);
    shouldBe(sorted.join(' '), `1000-02-02 1001-01-01 1001-01-02 1001-02-01 1002-01-01`);
}

{
    for (let i = 0; i < 12; ++i) {
        let dt = new Temporal.PlainDate(1995, 1 + i, 11 + i);
        shouldBe(dt.monthCode, `M${String(1 + i).padStart(2, '0')}`);
    }
}

{
    let week = ['MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT', 'SUN'];
    for (let i = 0; i < 7; ++i) {
        let dt = new Temporal.PlainDate(1995, 12, 11 + i);
        shouldBe(week[dt.dayOfWeek - 1], week[i]);
    }
}
{
    shouldBe(Temporal.PlainDate.from('1995-12-07').dayOfWeek, 4);
    shouldBe(Temporal.PlainDate.from('1995-12-08').dayOfWeek, 5);
    shouldBe(Temporal.PlainDate.from('1995-12-09').dayOfWeek, 6);
    shouldBe(Temporal.PlainDate.from('1995-12-10').dayOfWeek, 7);
    shouldBe(Temporal.PlainDate.from('1995-12-11').dayOfWeek, 1);
    shouldBe(Temporal.PlainDate.from('1995-12-12').dayOfWeek, 2);
    shouldBe(Temporal.PlainDate.from('1995-12-13').dayOfWeek, 3);
    shouldBe(Temporal.PlainDate.from('1995-12-14').dayOfWeek, 4);
}

{
    let tests = [
        [ '1995-01-01', 1 ],
        [ '1995-12-07', 341 ],
        [ '1995-12-31', 365 ],
        [ '2000-01-01', 1 ],
        [ '2000-12-07', 342 ],
        [ '2000-12-31', 366 ],
        [ '2004-01-01', 1 ],
        [ '2004-12-07', 342 ],
        [ '2004-12-31', 366 ],
        [ '2100-01-01', 1 ],
        [ '2100-12-07', 341 ],
        [ '2100-12-31', 365 ],
    ];
    for (let test of tests) {
        let dt = Temporal.PlainDate.from(test[0]);
        shouldBe(dt.dayOfYear, test[1]);
    }
}

{
    shouldBe(Temporal.PlainDate.from('1996-12-31').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1997-12-31').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1998-12-27').weekOfYear, 52);
    shouldBe(Temporal.PlainDate.from('1998-12-31').weekOfYear, 53);
    shouldBe(Temporal.PlainDate.from('1999-12-31').weekOfYear, 52);
    shouldBe(Temporal.PlainDate.from('2000-12-31').weekOfYear, 52);

    shouldBe(Temporal.PlainDate.from('1995-12-07').weekOfYear, 49);
    shouldBe(Temporal.PlainDate.from('1995-12-08').weekOfYear, 49);
    shouldBe(Temporal.PlainDate.from('1995-12-09').weekOfYear, 49);
    shouldBe(Temporal.PlainDate.from('1995-12-10').weekOfYear, 49);
    shouldBe(Temporal.PlainDate.from('1995-12-11').weekOfYear, 50);
    shouldBe(Temporal.PlainDate.from('1995-12-31').weekOfYear, 52);
    shouldBe(Temporal.PlainDate.from('1995-01-20').weekOfYear, 3);

    shouldBe(Temporal.PlainDate.from('1995-01-02').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-03').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-04').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-05').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-06').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-07').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-08').weekOfYear, 1);
    shouldBe(Temporal.PlainDate.from('1995-01-09').weekOfYear, 2);

    shouldBe(Temporal.PlainDate.from('1994-12-25').weekOfYear, 51); // Thursday
    shouldBe(Temporal.PlainDate.from('1994-12-26').weekOfYear, 52); // Friday
    shouldBe(Temporal.PlainDate.from('1994-12-27').weekOfYear, 52); // Saturday
    shouldBe(Temporal.PlainDate.from('1994-12-28').weekOfYear, 52); // Sunday
    shouldBe(Temporal.PlainDate.from('1994-12-29').weekOfYear, 52); // Monday
    shouldBe(Temporal.PlainDate.from('1994-12-30').weekOfYear, 52); // Tuesday
    shouldBe(Temporal.PlainDate.from('1994-12-31').weekOfYear, 52); // Wednesday
    shouldBe(Temporal.PlainDate.from('1995-01-01').weekOfYear, 52); // Thursday
}

{
    shouldBe(Temporal.PlainDate.from('1995-12-07').daysInWeek, 7);
    shouldBe(Temporal.PlainDate.from('2001-12-07').daysInWeek, 7);
    shouldBe(Temporal.PlainDate.from('2000-12-07').daysInWeek, 7);
    shouldBe(Temporal.PlainDate.from('2004-12-07').daysInWeek, 7);
    shouldBe(Temporal.PlainDate.from('2100-12-07').daysInWeek, 7);
}

{
    let days = [
        [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ],
        [ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ],
    ];
    for (let i = 0; i < 12; ++i) {
        let dt = new Temporal.PlainDate(1995, 1 + i, 11 + i);
        shouldBe(dt.daysInMonth, days[0][i]);
    }
    for (let i = 0; i < 12; ++i) {
        let dt = new Temporal.PlainDate(2004, 1 + i, 11 + i);
        shouldBe(dt.daysInMonth, days[1][i]);
    }
}

{
    shouldBe(Temporal.PlainDate.from('1995-12-07').daysInYear, 365);
    shouldBe(Temporal.PlainDate.from('2001-12-07').daysInYear, 365);
    shouldBe(Temporal.PlainDate.from('2000-12-07').daysInYear, 366);
    shouldBe(Temporal.PlainDate.from('2004-12-07').daysInYear, 366);
    shouldBe(Temporal.PlainDate.from('2100-12-07').daysInYear, 365);
}

{
    shouldBe(Temporal.PlainDate.from('1995-12-07').monthsInYear, 12);
    shouldBe(Temporal.PlainDate.from('2001-12-07').monthsInYear, 12);
    shouldBe(Temporal.PlainDate.from('2000-12-07').monthsInYear, 12);
    shouldBe(Temporal.PlainDate.from('2004-12-07').monthsInYear, 12);
    shouldBe(Temporal.PlainDate.from('2100-12-07').monthsInYear, 12);
}

{
    shouldBe(Temporal.PlainDate.from('1995-12-07').inLeapYear, false);
    shouldBe(Temporal.PlainDate.from('2001-12-07').inLeapYear, false);
    shouldBe(Temporal.PlainDate.from('2000-12-07').inLeapYear, true);
    shouldBe(Temporal.PlainDate.from('2004-12-07').inLeapYear, true);
    shouldBe(Temporal.PlainDate.from('2100-12-07').inLeapYear, false);
}

{
    let getterNames = [
        "year",
        "month",
        "monthCode",
        "day",
        "dayOfWeek",
        "dayOfYear",
        "weekOfYear",
        "daysInWeek",
        "daysInMonth",
        "daysInYear",
        "monthsInYear",
        "inLeapYear",
    ];
    for (let getterName of getterNames) {
        let getter = Reflect.getOwnPropertyDescriptor(Temporal.PlainDate.prototype, getterName).get;
        shouldBe(typeof getter, 'function');
        shouldThrow(() => {
            getter.call({});
        }, TypeError);
    }
}
