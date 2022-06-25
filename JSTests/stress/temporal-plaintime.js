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

shouldBe(Temporal.PlainTime instanceof Function, true);
shouldBe(Temporal.PlainTime.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainTime, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainTime, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainTime, 'prototype').configurable, false);
shouldBe(Temporal.PlainTime.prototype.constructor, Temporal.PlainTime);

{
    let time = new Temporal.PlainTime(0, 1, 2, 3, 4, 5, 6, 7);
    shouldBe(String(time), `00:01:02.003004005`);
    shouldBe(time.hour, 0);
    shouldBe(time.minute, 1);
    shouldBe(time.second, 2);
    shouldBe(time.millisecond, 3);
    shouldBe(time.microsecond, 4);
    shouldBe(time.nanosecond, 5);
}

{
    let time = new Temporal.PlainTime(0, 0, 0, 0, 0, 0);
    shouldBe(String(time), `00:00:00`);
    shouldBe(time.hour, 0);
    shouldBe(time.minute, 0);
    shouldBe(time.second, 0);
    shouldBe(time.millisecond, 0);
    shouldBe(time.microsecond, 0);
    shouldBe(time.nanosecond, 0);
}

{
    let time = new Temporal.PlainTime(23, 59, 59, 999, 999, 999);
    shouldBe(String(time), `23:59:59.999999999`);
    shouldBe(time.hour, 23);
    shouldBe(time.minute, 59);
    shouldBe(time.second, 59);
    shouldBe(time.millisecond, 999);
    shouldBe(time.microsecond, 999);
    shouldBe(time.nanosecond, 999);
}

{
    let time = new Temporal.PlainTime(23, 59, 59, 999, 999, 999);
    shouldBe(String(time), `23:59:59.999999999`);
    shouldBe(time.hour, 23);
    shouldBe(time.minute, 59);
    shouldBe(time.second, 59);
    shouldBe(time.millisecond, 999);
    shouldBe(time.microsecond, 999);
    shouldBe(time.nanosecond, 999);
}

{
    // We accept 60 seconds, but interpret it as 59 seconds.
    // https://tc39.es/proposal-temporal/#sec-temporal-parseisodatetime step 13
    let time = Temporal.PlainTime.from("23:59:60.999999999");
    shouldBe(String(time), `23:59:59.999999999`);
    shouldBe(time.hour, 23);
    shouldBe(time.minute, 59);
    shouldBe(time.second, 59);
    shouldBe(time.millisecond, 999);
    shouldBe(time.microsecond, 999);
    shouldBe(time.nanosecond, 999);
}

{
    let time = Temporal.PlainTime.from("2021-09-04T23:14:26.100200300");
    shouldBe(String(time), `23:14:26.1002003`);
    shouldBe(time.hour, 23);
    shouldBe(time.minute, 14);
    shouldBe(time.second, 26);
    shouldBe(time.millisecond, 100);
    shouldBe(time.microsecond, 200);
    shouldBe(time.nanosecond, 300);
}

shouldBe(String(Temporal.PlainTime.from('03')), `03:00:00`);
shouldBe(String(Temporal.PlainTime.from('T0314')), `03:14:00`);
shouldThrow(() => Temporal.PlainTime.from('0314'), RangeError);
shouldBe(String(Temporal.PlainTime.from('031415')), `03:14:15`);
shouldBe(String(Temporal.PlainTime.from('03:14')), `03:14:00`);
shouldBe(String(Temporal.PlainTime.from('03:14:15')), `03:14:15`);
shouldBe(String(Temporal.PlainTime.from('03:24:30')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('T2020-01')), `20:20:00`); // -01 UTC offset
shouldThrow(() => Temporal.PlainTime.from('2020-01'), RangeError);
shouldBe(String(Temporal.PlainTime.from('T01-01')), `01:00:00`); // -01 UTC offset
shouldThrow(() => Temporal.PlainTime.from('01-01'), RangeError);
shouldBe(String(Temporal.PlainTime.from('03:24:30[u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('03:24:30+01:00[Europe/Brussels][u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('03:24:30+01:00[u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('T03:24:30+01:00[Europe/Brussels][u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('T03:24:30+01:00[u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07t03:24:30')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+20:20:59')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30-20:20:59')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30\u221220:20:59')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+10')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+1020')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+102030')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+10:20:30.05')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+10:20:30.123456789')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[Europe/Brussels]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[Europe/Brussels]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[UNKNOWN]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[.hey]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_-]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_/_]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_-./_-.]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_../_..]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[_./_.]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[Etc/GMT+20]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[Etc/GMT-20]')), `03:24:30`); // TimeZone error should be ignored.
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00:00]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00:00.123]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00:00.12345]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00:00.12345678]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[+01:00:00.123456789]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[-01:00]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('1995-12-07 03:24:30+01:00[\u221201:00]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('2007-01-09 03:24:30+01:00[u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('2007-01-09 03:24:30+01:00[Europe/Brussels][u-ca=japanese]')), `03:24:30`);
shouldBe(String(Temporal.PlainTime.from('2007-01-09 03:24:30[u-ca=japanese]')), `03:24:30`);
{
    let time = Temporal.PlainTime.from('1995-12-07T03:24:30+01:00[Europe/Brussels]')
    shouldBe(time === Temporal.PlainTime.from(time), false);
}
{
    let time = Temporal.PlainTime.from({
      hour: 19,
      minute: 39,
      second: 9,
      millisecond: 68,
      microsecond: 346,
      nanosecond: 205
    });
    shouldBe(String(time), `19:39:09.068346205`);
}
{
    // Different overflow modes
    shouldBe(String(Temporal.PlainTime.from({ hour: 15, minute: 60 }, { overflow: 'constrain' })), `15:59:00`);
    shouldBe(String(Temporal.PlainTime.from({ hour: 15, minute: -1 }, { overflow: 'constrain' })), `15:00:00`);
}
shouldThrow(() => {
    Temporal.PlainTime.from({ hour: 15, minute: 60 }, { overflow: 'reject' });
}, RangeError);
shouldThrow(() => {
    Temporal.PlainTime.from({ hour: 15, minute: -1 }, { overflow: 'reject' });
}, RangeError);

shouldThrow(() => { new Temporal.PlainTime(-1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(25); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(24); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(30); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(-Infinity); }, RangeError);

shouldThrow(() => { new Temporal.PlainTime(0, -1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 60); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 61); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 77); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 100); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, -Infinity); }, RangeError);

shouldThrow(() => { new Temporal.PlainTime(0, 0, -1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 60); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 61); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 77); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 100); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, -Infinity); }, RangeError);

shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, -1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 1000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 1001); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 50000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 9999999999); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, -Infinity); }, RangeError);

shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, -1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 1000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 1001); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 50000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 9999999999); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, -Infinity); }, RangeError);

shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, -1); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, 1000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, 1001); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, 50000); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, 9999999999); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, Infinity); }, RangeError);
shouldThrow(() => { new Temporal.PlainTime(0, 0, 0, 0, 0, -Infinity); }, RangeError);

let failures = [
    "",
    "2020-01-01",
    "23:59:61.999999999",
    "1995-1207T03:24:30",
    "199512-07T03:24:30",
    "1995-12-07T03:24:30+20:20:60",
    "1995-12-07T03:24:30+20:2050",
    "1995-12-07T03:24:30+:",
    "1995-12-07T03:24:30+2:50",
    "1995-12-07T03:24:30+01:00[/]",
    "1995-12-07T03:24:30+01:00[///]",
    "1995-12-07T03:24:30+01:00[Hey/Hello",
    "1995-12-07T03:24:30+01:00[]",
    "1995-12-07T03:24:30+01:00[Hey/]",
    "1995-12-07T03:24:30+01:00[..]",
    "1995-12-07T03:24:30+01:00[.]",
    "1995-12-07T03:24:30+01:00[./.]",
    "1995-12-07T03:24:30+01:00[../..]",
    "1995-12-07T03:24:30+01:00[-Hey/Hello]",
    "1995-12-07T03:24:30+01:00[-]",
    "1995-12-07T03:24:30+01:00[-/_]",
    "1995-12-07T03:24:30+01:00[_/-]",
    "1995-12-07T03:24:30+01:00[CocoaCappuccinoMatcha]",
    "1995-12-07T03:24:30+01:00[Etc/GMT+50]",
    "1995-12-07T03:24:30+01:00[Etc/GMT+0]",
    "1995-12-07T03:24:30+01:00[Etc/GMT0]",
    "1995-12-07T03:24:30+10:20:30.0123456789",
    "1995-12-07 03:24:30+01:00[Etc/GMT\u221201]",
    "1995-12-07 03:24:30+01:00[+02:00:00.0123456789]",
    "1995-12-07 03:24:30+01:00[02:00:00.123456789]",
    "1995-12-07 03:24:30+01:00[02:0000.123456789]",
    "1995-12-07 03:24:30+01:00[0200:00.123456789]",
    "1995-12-07 03:24:30+01:00[02:00:60.123456789]",
    "1995-12-07T03:24:30Z", // UTCDesignator
    "2007-01-09 03:24:30[u-ca=japanese][Europe/Brussels]",
    "2007-01-09 03:24:30+01:00[u-ca=japanese][Europe/Brussels]",
];

for (let text of failures) {
    shouldThrow(() => {
        print(String(Temporal.PlainTime.from(text)));
    }, RangeError);
}

{
    let one = Temporal.PlainTime.from('03:24');
    let two = Temporal.PlainTime.from('01:24');
    let three = Temporal.PlainTime.from('01:24:05');
    let sorted = [one, two, three].sort(Temporal.PlainTime.compare);
    shouldBe(sorted.join(' '), `01:24:00 01:24:05 03:24:00`);
}

shouldBe(String(Temporal.PlainTime.from("20:34").calendar), `iso8601`);
shouldBe(Temporal.PlainTime.from("20:34").calendar instanceof Temporal.Calendar, true);
{
    let time = Temporal.PlainTime.from("20:34");
    shouldBe(time.calendar, time.calendar);
}

{
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.round('hour')), `20:00:00`);
    shouldBe(String(time.round({ smallestUnit: 'hour' })), `20:00:00`);
    shouldBe(String(time.round({ roundingIncrement: 30, smallestUnit: 'minute' })), `19:30:00`);
    shouldBe(String(time.round({ roundingIncrement: 30, smallestUnit: 'minute', roundingMode: 'ceil' })), `20:00:00`);
}
{
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    let other = Temporal.PlainTime.from('20:13:20.971398099');
    shouldBe(time.equals(other), false);
    shouldBe(time.equals(time), true);
}
{
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(time.toString(), `19:39:09.068346205`);

    shouldBe(time.toString({ smallestUnit: 'minute' }), `19:39`);
    shouldBe(time.toString({ fractionalSecondDigits: 0 }), `19:39:09`);
    shouldBe(time.toString({ fractionalSecondDigits: 4 }), `19:39:09.0683`);
    shouldBe(time.toString({ fractionalSecondDigits: 5, roundingMode: 'halfExpand' }), `19:39:09.06835`);
    shouldBe(Temporal.PlainTime.from('19:39:09.000000205').toString({ fractionalSecondDigits: 4 }), `19:39:09.0000`);
}
{
    const workBreak = {
      type: 'mandatory',
      name: 'Lunch',
      startTime: new Temporal.PlainTime(12),
      endTime: new Temporal.PlainTime(13)
    };
    const str = JSON.stringify(workBreak);
    shouldBe(str, `{"type":"mandatory","name":"Lunch","startTime":"12:00:00","endTime":"13:00:00"}`);
    function reviver(key, value) {
      if (key.endsWith('Time')) return Temporal.PlainTime.from(value);
      return value;
    }
    let object = JSON.parse(str, reviver);
    shouldBe(object.startTime instanceof Temporal.PlainTime, true);
    shouldBe(String(object.startTime), `12:00:00`);
    shouldBe(object.endTime instanceof Temporal.PlainTime, true);
    shouldBe(String(object.endTime), `13:00:00`);
}
shouldThrow(() => {
    new Temporal.PlainTime(12).valueOf();
}, TypeError);
{
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(JSON.stringify(time.getISOFields()), `{"calendar":"iso8601","isoHour":19,"isoMicrosecond":346,"isoMillisecond":68,"isoMinute":39,"isoNanosecond":205,"isoSecond":9}`);
}
{
    let max = 1 ** 53;
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.add({ minutes: 5, nanoseconds: 800 })), `19:44:09.068347005`);
    time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.add({ hours: max, minutes: max, seconds: max, milliseconds: max, microseconds: max, nanoseconds: max })), `20:40:10.069347206`);
    shouldBe(String(time.add(new Temporal.Duration(1, 1, 1, 1))), `19:39:09.068346205`);
    shouldBe(String(time.add(new Temporal.Duration(1, 1, 1, 1, 1, 1, 1, 1))), `20:40:10.069346205`);
    shouldBe(String(time.add(new Temporal.Duration(0, 0, 0, 0, 30, 1, 1, 1))), `01:40:10.069346205`);
}
{
    let max = 1 ** 53;
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.subtract({ minutes: 5, nanoseconds: 800 })), `19:34:09.068345405`);
    time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.subtract({ hours: max, minutes: max, seconds: max, milliseconds: max, microseconds: max, nanoseconds: max })), `18:38:08.067345204`);
    shouldBe(String(time.subtract(new Temporal.Duration(1, 1, 1, 1))), `19:39:09.068346205`);
    shouldBe(String(time.subtract(new Temporal.Duration(1, 1, 1, 1, 1, 1, 1, 1))), `18:38:08.067346205`);
    shouldBe(String(time.subtract(new Temporal.Duration(0, 0, 0, 0, 30, 1, 1, 1))), `13:38:08.067346205`);
}
{
    let time = Temporal.PlainTime.from('19:39:09.068346205');
    shouldBe(String(time.add({ hours: 1 }).with({
      minute: 0,
      second: 0,
      millisecond: 0,
      microsecond: 0,
      nanosecond: 0
    })), `20:00:00`);
}
{
    let time = Temporal.PlainTime.from('20:13:20.971398099');
    shouldBe(String(time.until(Temporal.PlainTime.from('22:39:09.068346205'))), `PT2H25M48.096948106S`);
    shouldBe(String(time.until(Temporal.PlainTime.from('19:39:09.068346205'))), `-PT34M11.903051894S`);
    shouldBe(String(time.until(Temporal.PlainTime.from('22:39:09.068346205'), { smallestUnit: 'second' })), `PT2H25M48S`);
}
{
    let time = Temporal.PlainTime.from('20:13:20.971398099');
    shouldBe(String(time.since(Temporal.PlainTime.from('19:39:09.068346205'))), `PT34M11.903051894S`);
    shouldBe(String(time.since(Temporal.PlainTime.from('22:39:09.068346205'))), `-PT2H25M48.096948106S`);
}
