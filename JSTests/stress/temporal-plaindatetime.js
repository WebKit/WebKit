//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
    func();
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

shouldBe(Temporal.PlainDateTime instanceof Function, true);
shouldBe(Temporal.PlainDateTime.length, 3);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDateTime, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDateTime, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainDateTime, 'prototype').configurable, false);
shouldThrow(() => Temporal.PlainDateTime(), TypeError);
shouldBe(Temporal.PlainDateTime.prototype.constructor, Temporal.PlainDateTime);

const pdt = new Temporal.PlainDateTime(1,2,3,4,5,6,7,8,9);
shouldBe(pdt instanceof Temporal.PlainDateTime, true);
{
    class DerivedPlainDateTime extends Temporal.PlainDateTime {}

    const dd = new DerivedPlainDateTime(0, 1, 1);
    shouldBe(dd instanceof DerivedPlainDateTime, true);
    shouldBe(dd instanceof Temporal.PlainDateTime, true);
    shouldBe(dd.negated, Temporal.PlainDateTime.prototype.negated);
    shouldBe(Object.getPrototypeOf(dd), DerivedPlainDateTime.prototype);
    shouldBe(Object.getPrototypeOf(DerivedPlainDateTime.prototype), Temporal.PlainDateTime.prototype);
}

shouldThrow(() => new Temporal.PlainDateTime(), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(Infinity, 1, 1), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, Infinity, 1), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, 0, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, 0, 0, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, 0, 0, 0, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, 0, 0, 0, 0, Infinity), RangeError);
shouldThrow(() => new Temporal.PlainDateTime(0, 1, 1, 0, 0, 0, 0, 0, Infinity), RangeError);

const fields = ['year', 'month', 'day', 'hour', 'minute', 'second', 'millisecond', 'microsecond', 'nanosecond'];
fields.forEach((field, i) => {
    shouldThrow(() => Temporal.PlainDateTime.prototype[field], TypeError);
    shouldBe(pdt[field], i + 1);
});

shouldBe(pdt.calendar instanceof Temporal.Calendar, true);
shouldBe(pdt.calendar.toString(), 'iso8601');
{
    const dateGetters = ['monthCode', 'dayOfWeek', 'dayOfYear', 'weekOfYear', 'daysInWeek', 'daysInMonth', 'daysInYear', 'monthsInYear', 'inLeapYear'];
    const results = ['M02', 6, 34, 5, 7, 28, 365, 12, false];
    dateGetters.forEach((property, i) => {
        shouldThrow(() => Temporal.PlainDateTime.prototype[property], TypeError);
        shouldBe(pdt[property], results[i]);
    });
}

shouldBe(Temporal.PlainDateTime.prototype.getISOFields.length, 0);
shouldBe(JSON.stringify(pdt.getISOFields()), `{"calendar":"iso8601","isoDay":3,"isoHour":4,"isoMicrosecond":8,"isoMillisecond":7,"isoMinute":5,"isoMonth":2,"isoNanosecond":9,"isoSecond":6,"isoYear":1}`);

shouldBe(Temporal.PlainDateTime.from.length, 1);
shouldThrow(() => Temporal.PlainDateTime.from(), TypeError);
shouldBe(Temporal.PlainDateTime.from('0001-02-03T04:05:06.007008009').toString(), '0001-02-03T04:05:06.007008009');
{
    const dateTime = Temporal.PlainDateTime.from('2007-01-09T03:24:30+01:00[Europe/Brussels]');
    shouldBe(dateTime === Temporal.PlainDateTime.from(dateTime), false);

    const date = Temporal.PlainDate.from('2007-01-09T03:24:30+01:00[Europe/Brussels]');
    shouldBe(Temporal.PlainDateTime.from(date).toString(), '2007-01-09T00:00:00');

    shouldBe(Temporal.PlainDateTime.from({ year: 1, month: 2, day: 3, hour: 4, minute: 5, second: 6, millisecond: 7, microsecond: 8, nanosecond: 9 }).toString(), '0001-02-03T04:05:06.007008009');
    shouldBe(Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M01', day: 9 }).toString(), '2007-01-09T00:00:00');

    shouldBe(Temporal.PlainDateTime.from({ year: 2007, month: 20, day: 40, hour: 30, minute: 80, second: 80, millisecond: 2000, microsecond: 2000, nanosecond: 2000 }).toString(), '2007-12-31T23:59:59.999999999');
    shouldBe(Temporal.PlainDateTime.from({ year: 2007, month: 20, day: 40, hour: 30, minute: 80, second: 80, millisecond: 2000, microsecond: 2000, nanosecond: 2000 }, { overflow: 'constrain' }).toString(), '2007-12-31T23:59:59.999999999');
}

shouldThrow(() => Temporal.PlainDateTime.from(pdt, { overflow: 'bogus' }), RangeError);
shouldBe(Temporal.PlainDateTime.from(pdt, { overflow: 'constrain' }).toString(), Temporal.PlainDateTime.from(pdt).toString());shouldThrow(() => { Temporal.PlainDate.from({ month: 1, day: 9 }); }, TypeError);

shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, day: 9 }); }, TypeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, month: 1 }); }, TypeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: Infinity, month: 1, day: 9 }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, month: 0, day: 9 }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9 }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, hour: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, minute: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, second: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, millisecond: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, microsecond: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, monthCode: 'M00', day: 9, nanosecond: Infinity }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, month: 1, day: 0 }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, month: 1, monthCode: 'M02', day: 9 }); }, RangeError);
shouldThrow(() => { Temporal.PlainDateTime.from({ year: 2007, month: 20, day: 40, hour: 30, minute: 80, second: 80, millisecond: 2000, microsecond: 2000, nanosecond: 2000 }, { overflow: 'reject' }); }, RangeError);

const goodStrings = [
    '2007-01-09',
    '2007-01-09T03:24:30',
    '2007-01-09t03:24:30',
    '2007-01-09 03:24:30',
    '2007-01-09T03:24:30+20:20:59',
    '2007-01-09T03:24:30-20:20:59',
    '2007-01-09T03:24:30+10',
    '2007-01-09T03:24:30+1020',
    '2007-01-09T03:24:30+102030',
    '2007-01-09T03:24:30+10:20:30.05',
    '2007-01-09T03:24:30+10:20:30.123456789',
    '2007-01-09T03:24:30+01:00[Europe/Brussels]',
    '2007-01-09 03:24:30+01:00[Europe/Brussels]',
    '2007-01-09T03:24:30+01:00[UNKNOWN]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[.hey]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_-]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_/_]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_-./_-.]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_../_..]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[_./_.]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[Etc/GMT+20]', // TimeZone error should be ignored.
    '2007-01-09T03:24:30+01:00[Etc/GMT-20]', // TimeZone error should be ignored.
    '2007-01-09 03:24:30+01:00[+01]',
    '2007-01-09 03:24:30+01:00[+01:00]',
    '2007-01-09 03:24:30+01:00[-01:00]',
    '2007-01-09 03:24:30+01:00[u-ca=japanese]',
    '2007-01-09 03:24:30+01:00[Europe/Brussels][u-ca=japanese]',
    '2007-01-09[u-ca=japanese]',
];
for (let s of goodStrings)
    shouldNotThrow(() => Temporal.PlainDateTime.from(s));

const badStrings = [
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
    "2007-01-09T03:24:30\u221220:20:59",
    "2007-01-09 03:24:30+01:00[\u221201:00]",
    "2007-01-09Z",
    "2007-01-09+01:00",
    '2006-01-09 03:24:30+01:00[+01:00:00]',
    '2006-01-09 03:24:30+01:00[+01:00:00.123]',
    '2006-01-09 03:24:30+01:00[+01:00:00.12345]',
    '2006-01-09 03:24:30+01:00[+01:00:00.12345678]',
    '2006-01-09 03:24:30+01:00[+01:00:00.123456789]',
];
for (let s of badStrings)
    shouldThrow(() => Temporal.PlainDateTime.from(s), RangeError);

shouldBe(Temporal.PlainDateTime.compare.length, 2);
shouldBe(Temporal.PlainDateTime.compare('2007-01-09T03:24', '2007-01-09'), 1);
shouldBe(Temporal.PlainDateTime.compare('2007-01-09T03:24', '2007-01-09T03:24:00.007008009'), -1);
shouldBe(Temporal.PlainDateTime.compare('2007-01-09T00:00', '2007-01-09'), 0);

// At present, toLocaleString has the same behavior as toJSON or argumentless toString.
for (const method of ['toString', 'toJSON', 'toLocaleString']) {    
    shouldBe(Temporal.PlainDateTime.prototype[method].length, 0);
    shouldThrow(() => Temporal.PlainDateTime.prototype[method].call({}), TypeError);

    shouldBe(pdt[method](), '0001-02-03T04:05:06.007008009');
}
shouldBe(pdt.toString({}), pdt.toString());

shouldThrow(() => pdt.toString({ smallestUnit: 'bogus' }), RangeError);
for (const unit of ['year', 'month', 'week', 'day', 'hour']) {
    shouldThrow(() => pdt.toString({ smallestUnit: unit }), RangeError);
    shouldThrow(() => pdt.toString({ smallestUnit: `${unit}s` }), RangeError);
}
shouldBe(pdt.toString({ smallestUnit: 'minute' }), '0001-02-03T04:05');
shouldBe(pdt.toString({ smallestUnit: 'second' }), '0001-02-03T04:05:06');
shouldBe(pdt.toString({ smallestUnit: 'millisecond' }), '0001-02-03T04:05:06.007');
shouldBe(pdt.toString({ smallestUnit: 'microsecond' }), '0001-02-03T04:05:06.007008');
shouldBe(pdt.toString({ smallestUnit: 'nanosecond' }), '0001-02-03T04:05:06.007008009');
for (const unit of ['minute', 'second', 'millisecond', 'microsecond', 'nanosecond'])
    shouldBe(pdt.toString({ smallestUnit: unit }), pdt.toString({ smallestUnit: `${unit}s` }));

shouldThrow(() => pdt.toString({ fractionalSecondDigits: -1 }), RangeError);
shouldThrow(() => pdt.toString({ fractionalSecondDigits: 10 }), RangeError);
shouldThrow(() => pdt.toString({ fractionalSecondDigits: 'bogus' }), RangeError);
shouldBe(pdt.toString({ fractionalSecondDigits: 0 }), '0001-02-03T04:05:06');
const decimalPart = '007008009';
for (let i = 1; i < 10; i++)
    shouldBe(pdt.toString({ fractionalSecondDigits: i }), `0001-02-03T04:05:06.${decimalPart.slice(0,i)}`);
shouldBe(pdt.toString({ fractionalSecondDigits: 'auto' }), pdt.toString());

shouldThrow(() => pdt.toString({ roundingMode: 'bogus' }), RangeError);
shouldBe(pdt.toString({ roundingMode: 'trunc' }), pdt.toString());
shouldBe(pdt.toString({ fractionalSecondDigits: 2, roundingMode: 'ceil' }), '0001-02-03T04:05:06.01');
shouldBe(pdt.toString({ fractionalSecondDigits: 2, roundingMode: 'floor' }), '0001-02-03T04:05:06.00');
shouldBe(new Temporal.PlainDateTime(1999,12,31,23,59,59,999,999,999).toString({ smallestUnit: 'microsecond', roundingMode: 'halfExpand' }), '2000-01-01T00:00:00.000000');

shouldBe(Temporal.PlainDateTime.prototype.toPlainDate.length, 0);
shouldBe(pdt.toPlainDate().toString(), '0001-02-03');
shouldBe(Temporal.PlainDateTime.prototype.toPlainTime.length, 0);
shouldBe(pdt.toPlainTime().toString(), '04:05:06.007008009');

shouldBe(Temporal.PlainDateTime.prototype.valueOf.length, 0);
shouldThrow(() => pdt.valueOf(), TypeError);

shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,5,6,7,8,9)), true);
shouldBe(pdt.equals(new Temporal.PlainDateTime(2,2,3,4,5,6,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,3,3,4,5,6,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,4,4,5,6,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,5,5,6,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,6,6,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,5,7,7,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,5,6,8,8,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,5,6,7,9,9)), false);
shouldBe(pdt.equals(new Temporal.PlainDateTime(1,2,3,4,5,6,7,8,1)), false);

shouldBe(Temporal.PlainDateTime.prototype.add.length, 1);
shouldBe(pdt.add(new Temporal.Duration()).toString(), '0001-02-03T04:05:06.007008009');
shouldBe(pdt.add(new Temporal.Duration(1,1,1,1,1,1,1,1,1,1)).toString(), '0002-03-11T05:06:07.00800901');
shouldBe(pdt.add('-P1Y1M1DT1H1M1.001001001S').toString(), '0000-01-02T03:04:05.006007008');
shouldBe(pdt.add({ hours: 24 }).toString(), '0001-02-04T04:05:06.007008009');
shouldBe(pdt.add(new Temporal.Duration(0,10,3,7,19,54,53,992,991,991)).toString(), '0002-01-01T00:00:00');
shouldBe(Temporal.PlainDateTime.from('2020-01-30').add({ months: 1 }).toString(), '2020-02-29T00:00:00');
shouldThrow(() => { Temporal.PlainDateTime.from('2020-01-30').add({ months: 1 }, { overflow: 'reject' }); }, RangeError);
shouldThrow(() => { pdt.add({ years: 300000 }); }, RangeError);
const pdt1 = Temporal.PlainDateTime.from("2020-02-29T00:57:27.747612578");
shouldThrow(() => pdt1.add(Temporal.Duration.from({seconds: Number.MAX_SAFE_INTEGER})), RangeError);
shouldThrow(() => pdt1.add(Temporal.Duration.from({milliseconds: Number.MAX_SAFE_INTEGER})), RangeError);
shouldBe(pdt1.add(Temporal.Duration.from({microseconds: Number.MAX_SAFE_INTEGER})).toString(), '2305-08-04T00:45:02.488603578');
shouldBe(pdt1.add(Temporal.Duration.from({nanoseconds: Number.MAX_SAFE_INTEGER})).toString(), '2020-06-12T06:57:27.002353569');

shouldBe(Temporal.PlainDateTime.prototype.subtract.length, 1);
shouldBe(pdt.subtract(new Temporal.Duration()).toString(), '0001-02-03T04:05:06.007008009');
shouldBe(pdt.subtract(new Temporal.Duration(1,1,0,1,1,1,1,1,1,1)).toString(), '0000-01-02T03:04:05.006007008');
shouldBe(pdt.subtract('-P1Y1M1W1DT1H1M1.001001001S').toString(), '0002-03-11T05:06:07.00800901');
shouldBe(pdt.subtract({ hours: 24 }).toString(), '0001-02-02T04:05:06.007008009');
shouldBe(pdt.subtract(new Temporal.Duration(0,1,5,3,4,5,6,7,8,10)).toString(), '0000-11-25T23:59:59.999999999');
shouldBe(Temporal.PlainDateTime.from('2020-03-30').subtract({ months: 1 }).toString(), '2020-02-29T00:00:00');
shouldThrow(() => { Temporal.PlainDateTime.from('2020-03-30').subtract({ months: 1 }, { overflow: 'reject' }); }, RangeError);
shouldThrow(() => { pdt.subtract({ years: 300000 }); }, RangeError);

shouldBe(Temporal.PlainDateTime.prototype.with.length, 1);
shouldBe(pdt.with({ year: 2000, month: 10, hour: 1, minute: 3, millisecond: 0, microsecond: 0, nanosecond: 0 }).toString(), '2000-10-03T01:03:06');
shouldBe(pdt.with({ second: 15 }).toString(), '0001-02-03T04:05:15.007008009');
shouldBe(pdt.with({ day: 30 }).toString(), '0001-02-28T04:05:06.007008009');
shouldThrow(() => { pdt.with({ day: 30 }, { overflow: 'reject' }); }, RangeError);

shouldBe(Temporal.PlainDateTime.prototype.withPlainTime.length, 0);
shouldBe(pdt.withPlainTime().toString(), '0001-02-03T00:00:00');
shouldBe(pdt.withPlainTime({ hour: 1, minute: 2, second: 3 }).toString(), '0001-02-03T01:02:03');

shouldBe(Temporal.PlainDateTime.prototype.round.length, 1);
shouldBe(pdt.round('hour').toString(), '0001-02-03T04:00:00');
shouldBe(pdt.round({ smallestUnit: 'hour' }).toString(), '0001-02-03T04:00:00');
shouldBe(pdt.round({ smallestUnit: 'minute', roundingIncrement: 2 }).toString(), '0001-02-03T04:06:00');
shouldBe(pdt.round({ smallestUnit: 'minute', roundingIncrement: 2, roundingMode: 'floor' }).toString(), '0001-02-03T04:04:00');
shouldThrow(() => { pdt.round({}); }, RangeError);
shouldThrow(() => { pdt.round({ smallestUnit: 'bogus' }); }, RangeError);
shouldThrow(() => { pdt.round({ smallestUnit: 'minute', roundingIncrement: 24 }); }, RangeError);
shouldThrow(() => { pdt.round({ smallestUnit: 'minute', roundingMode: 'bogus' }); }, RangeError);
