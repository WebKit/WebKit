function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function test() {
    let range = " – "; // This is not usual space unfortuantely in ICU 66.
    if ($vm.icuVersion() > 66)
        range = " – ";

    let date1 = new Date(Date.UTC(2007, 0, 10, 10, 0, 0));
    let date2 = new Date(Date.UTC(2007, 0, 10, 11, 0, 0));
    let date3 = new Date(Date.UTC(2007, 0, 20, 10, 0, 0));
    let date4 = new Date(Date.UTC(2010, 0, 20, 10, 0, 0));

    let date5 = new Date(Date.UTC(2007, 0, 10, 12, 0, 0));
    let date6 = new Date(Date.UTC(2007, 0, 10, 14, 0, 0));
    let date7 = new Date(Date.UTC(2007, 0, 10, 23, 0, 0));
    let date8 = new Date(Date.UTC(2007, 0, 11, 0, 0, 0));

    // Test three relavant extensions.
    // "nu" NumberingSystem
    let fmt1 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'America/Los_Angeles',
        numberingSystem: 'hanidec',
    });
    shouldBe(fmt1.format(date1), `一/一〇/〇七, 二:〇〇 AM`);
    shouldBe(fmt1.formatRange(date1, date2), `一/一〇/〇七, 二:〇〇${range}三:〇〇 AM`);
    shouldBe(fmt1.formatRange(date1, date3), `一/一〇/〇七, 二:〇〇 AM${range}一/二〇/〇七, 二:〇〇 AM`);

    // "ca" Calendar
    let fmt2 = new Intl.DateTimeFormat("en", {
        year: 'numeric',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'America/Los_Angeles',
        calendar: 'chinese'
    });
    shouldBe(fmt2.format(date1), `11/22/2006, 2:00 AM`);
    shouldBe(fmt2.formatRange(date1, date2), `11/22/2006, 2:00${range}3:00 AM`);
    shouldBe(fmt2.formatRange(date1, date3), `11/22/2006, 2:00 AM${range}12/2/2006, 2:00 AM`);

    let fmt3 = new Intl.DateTimeFormat("en", {
        year: 'numeric',
        timeZone: 'America/Los_Angeles',
        calendar: 'chinese'
    });
    shouldBe(fmt3.format(date1), `2006(bing-xu)`);
    shouldBe(fmt3.formatRange(date1, date2), `2006(bing-xu)`);
    shouldBe(fmt3.formatRange(date1, date3), `2006(bing-xu)`);
    shouldBe(fmt3.formatRange(date1, date4), `2006(bing-xu)${range}2009(ji-chou)`);

    // Calendar-sensitive format
    let fmt4 = new Intl.DateTimeFormat('en-u-ca-buddhist', {
        year: 'numeric',
        timeZone: 'America/Los_Angeles',
    });
    shouldBe(fmt4.format(date1), `2550 BE`);
    shouldBe(fmt4.formatRange(date1, date2), `2550 BE`);
    shouldBe(fmt4.formatRange(date1, date3), `2550 BE`);
    shouldBe(fmt4.formatRange(date1, date4), `2550${range}2553 BE`);

    // "hc" HourCycle
    let fmt5 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
    });
    shouldBe(fmt5.format(date1), `1/10/07, 10:00`);
    shouldBe(fmt5.format(date8), `1/11/07, 24:00`);
    shouldBe(fmt5.formatRange(date1, date2), `1/10/07, 10:00${range}11:00`);
    shouldBe(fmt5.formatRange(date1, date3), `1/10/07, 10:00${range}1/20/07, 10:00`);
    shouldBe(fmt5.formatRange(date1, date5), `1/10/07, 10:00${range}12:00`);
    shouldBe(fmt5.formatRange(date1, date6), `1/10/07, 10:00${range}14:00`);
    shouldBe(fmt5.formatRange(date1, date7), `1/10/07, 10:00${range}23:00`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt5.formatRange(date1, date8), `1/10/07, 10:00${range}1/11/07, 24:00`);

    let fmt6 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
    });
    shouldBe(fmt6.format(date1), `1/10/07, 10:00`);
    shouldBe(fmt6.format(date8), `1/11/07, 00:00`);
    shouldBe(fmt6.formatRange(date1, date2), `1/10/07, 10:00${range}11:00`);
    shouldBe(fmt6.formatRange(date1, date3), `1/10/07, 10:00${range}1/20/07, 10:00`);
    shouldBe(fmt6.formatRange(date1, date5), `1/10/07, 10:00${range}12:00`);
    shouldBe(fmt6.formatRange(date1, date6), `1/10/07, 10:00${range}14:00`);
    shouldBe(fmt6.formatRange(date1, date7), `1/10/07, 10:00${range}23:00`);
    shouldBe(fmt6.formatRange(date1, date8), `1/10/07, 10:00${range}1/11/07, 00:00`);

    let fmt7 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
    });
    shouldBe(fmt7.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt7.format(date8), `1/11/07, 0:00 AM`);
    shouldBe(fmt7.formatRange(date1, date2), `1/10/07, 10:00 AM${range}11:00 AM`);
    shouldBe(fmt7.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt7.formatRange(date1, date5), `1/10/07, 10:00 AM${range}0:00 PM`);
    shouldBe(fmt7.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt7.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt7.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 0:00 AM`);

    let fmt8 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
    });
    shouldBe(fmt8.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt8.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt8.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt8.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt8.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt8.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt8.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt8.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);

    // "hc" + hour 2-digit
    let fmt9 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
    });
    shouldBe(fmt9.format(date1), `1/10/07, 10:00`);
    shouldBe(fmt9.format(date8), `1/11/07, 24:00`);
    shouldBe(fmt9.formatRange(date1, date2), `1/10/07, 10:00${range}11:00`);
    shouldBe(fmt9.formatRange(date1, date3), `1/10/07, 10:00${range}1/20/07, 10:00`);
    shouldBe(fmt9.formatRange(date1, date5), `1/10/07, 10:00${range}12:00`);
    shouldBe(fmt9.formatRange(date1, date6), `1/10/07, 10:00${range}14:00`);
    shouldBe(fmt9.formatRange(date1, date7), `1/10/07, 10:00${range}23:00`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt9.formatRange(date1, date8), `1/10/07, 10:00${range}1/11/07, 24:00`);

    let fmt10 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
    });
    shouldBe(fmt10.format(date1), `1/10/07, 10:00`);
    shouldBe(fmt10.format(date8), `1/11/07, 00:00`);
    shouldBe(fmt10.formatRange(date1, date2), `1/10/07, 10:00${range}11:00`);
    shouldBe(fmt10.formatRange(date1, date3), `1/10/07, 10:00${range}1/20/07, 10:00`);
    shouldBe(fmt10.formatRange(date1, date5), `1/10/07, 10:00${range}12:00`);
    shouldBe(fmt10.formatRange(date1, date6), `1/10/07, 10:00${range}14:00`);
    shouldBe(fmt10.formatRange(date1, date7), `1/10/07, 10:00${range}23:00`);
    shouldBe(fmt10.formatRange(date1, date8), `1/10/07, 10:00${range}1/11/07, 00:00`);

    let fmt11 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
    });
    shouldBe(fmt11.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt11.format(date8), `1/11/07, 00:00 AM`);
    shouldBe(fmt11.formatRange(date1, date2), `1/10/07, 10:00 AM${range}11:00 AM`);
    shouldBe(fmt11.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt11.formatRange(date1, date5), `1/10/07, 10:00 AM${range}0:00 PM`);
    shouldBe(fmt11.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt11.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt11.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 0:00 AM`);

    let fmt12 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
    });
    shouldBe(fmt12.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt12.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt12.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt12.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt12.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt12.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt12.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt12.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);

    // "hc" + hour12.
    let fmt13 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
        hour12: true,
    });
    shouldBe(fmt13.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt13.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt13.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt13.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt13.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt13.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt13.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt13.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);

    let fmt14 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
        hour12: true,
    });
    shouldBe(fmt14.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt14.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt14.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt14.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt14.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt14.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt14.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt14.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);

    let fmt15 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
        hour12: true,
    });
    shouldBe(fmt15.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt15.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt15.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt15.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt15.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt15.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt15.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt15.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);

    let fmt16 = new Intl.DateTimeFormat("en", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
        hour12: true,
    });
    shouldBe(fmt16.format(date1), `1/10/07, 10:00 AM`);
    shouldBe(fmt16.format(date8), `1/11/07, 12:00 AM`);
    shouldBe(fmt16.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
    shouldBe(fmt16.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    shouldBe(fmt16.formatRange(date1, date5), `1/10/07, 10:00 AM${range}12:00 PM`);
    shouldBe(fmt16.formatRange(date1, date6), `1/10/07, 10:00 AM${range}2:00 PM`);
    shouldBe(fmt16.formatRange(date1, date7), `1/10/07, 10:00 AM${range}11:00 PM`);
    shouldBe(fmt16.formatRange(date1, date8), `1/10/07, 10:00 AM${range}1/11/07, 12:00 AM`);
}

if ($vm.icuVersion() >= 64)
    test();
