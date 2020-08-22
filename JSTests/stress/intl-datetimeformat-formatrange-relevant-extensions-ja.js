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

let date1 = new Date(Date.UTC(2007, 0, 10, 10, 0, 0));
let date2 = new Date(Date.UTC(2007, 0, 10, 11, 0, 0));
let date3 = new Date(Date.UTC(2007, 0, 20, 10, 0, 0));
let date4 = new Date(Date.UTC(2010, 0, 20, 10, 0, 0));

let date5 = new Date(Date.UTC(2007, 0, 10, 12, 0, 0));
let date6 = new Date(Date.UTC(2007, 0, 10, 14, 0, 0));
let date7 = new Date(Date.UTC(2007, 0, 10, 23, 0, 0));
let date8 = new Date(Date.UTC(2007, 0, 11, 0, 0, 0));

if ($vm.icuVersion() >= 64) {
    // Test three relavant extensions.
    // "nu" NumberingSystem
    let fmt1 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'America/Los_Angeles',
        numberingSystem: 'hanidec',
    });
    shouldBe(fmt1.format(date1), `〇七/一/一〇 二:〇〇`);
    shouldBe(fmt1.formatRange(date1, date2), `〇七/一/一〇 二時〇〇分～三時〇〇分`);
    shouldBe(fmt1.formatRange(date1, date3), `〇七/一/一〇 二:〇〇～〇七/一/二〇 二:〇〇`);

    // "ca" Calendar
    let fmt2 = new Intl.DateTimeFormat("ja", {
        year: 'numeric',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'America/Los_Angeles',
        calendar: 'chinese'
    });
    shouldBe(fmt2.format(date1), `丙戌年11月22日 2:00`);
    shouldBe(fmt2.formatRange(date1, date2), `丙戌年11月22日 2時00分～3時00分`);
    shouldBe(fmt2.formatRange(date1, date3), `丙戌年11月22日 2:00～丙戌年12月2日 2:00`);

    let fmt3 = new Intl.DateTimeFormat("ja", {
        year: 'numeric',
        timeZone: 'America/Los_Angeles',
        calendar: 'chinese'
    });
    shouldBe(fmt3.format(date1), `丙戌年`);
    shouldBe(fmt3.formatRange(date1, date2), `丙戌年`);
    shouldBe(fmt3.formatRange(date1, date3), `丙戌年`);
    shouldBe(fmt3.formatRange(date1, date4), `丙戌年～己丑年`);

    // Calendar-sensitive format
    let fmt4 = new Intl.DateTimeFormat('ja-u-ca-buddhist', {
        year: 'numeric',
        timeZone: 'America/Los_Angeles',
    });
    shouldBe(fmt4.format(date1), `仏暦2550年`);
    shouldBe(fmt4.formatRange(date1, date2), `仏暦2550年`);
    shouldBe(fmt4.formatRange(date1, date3), `仏暦2550年`);
    shouldBe(fmt4.formatRange(date1, date4), `仏暦2550年～2553年`);

    // "hc" HourCycle
    let fmt5 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
    });
    shouldBe(fmt5.format(date1), `07/1/10 10:00`);
    shouldBe(fmt5.format(date8), `07/1/11 24:00`);
    shouldBe(fmt5.formatRange(date1, date2), `07/1/10 10:00～11:00`);
    shouldBe(fmt5.formatRange(date1, date3), `07/1/10 10:00～07/1/20 10:00`);
    shouldBe(fmt5.formatRange(date1, date5), `07/1/10 10:00～12:00`);
    shouldBe(fmt5.formatRange(date1, date6), `07/1/10 10:00～14:00`);
    shouldBe(fmt5.formatRange(date1, date7), `07/1/10 10:00～23:00`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt5.formatRange(date1, date8), `07/1/10 10:00～07/1/11 24:00`);

    let fmt6 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
    });
    shouldBe(fmt6.format(date1), `07/1/10 10:00`);
    shouldBe(fmt6.format(date8), `07/1/11 0:00`);
    shouldBe(fmt6.formatRange(date1, date2), `07/1/10 10時00分～11時00分`);
    shouldBe(fmt6.formatRange(date1, date3), `07/1/10 10:00～07/1/20 10:00`);
    shouldBe(fmt6.formatRange(date1, date5), `07/1/10 10時00分～12時00分`);
    shouldBe(fmt6.formatRange(date1, date6), `07/1/10 10時00分～14時00分`);
    shouldBe(fmt6.formatRange(date1, date7), `07/1/10 10時00分～23時00分`);
    shouldBe(fmt6.formatRange(date1, date8), `07/1/10 10:00～07/1/11 0:00`);

    let fmt7 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
    });
    shouldBe(fmt7.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt7.format(date8), `07/1/11 午前0:00`);
    shouldBe(fmt7.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt7.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt7.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt7.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt7.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt7.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);

    let fmt8 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: 'numeric',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
    });
    shouldBe(fmt8.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt8.format(date8), `07/1/11 午前12:00`);
    shouldBe(fmt8.formatRange(date1, date2), `07/1/10 午前10時00分～11時00分`);
    shouldBe(fmt8.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt8.formatRange(date1, date5), `07/1/10 午前10時00分～午後0時00分`);
    shouldBe(fmt8.formatRange(date1, date6), `07/1/10 午前10時00分～午後2時00分`);
    shouldBe(fmt8.formatRange(date1, date7), `07/1/10 午前10時00分～午後11時00分`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt8.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前12:00`);

    // "hc" + hour 2-digit
    let fmt9 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
    });
    shouldBe(fmt9.format(date1), `07/1/10 10:00`);
    shouldBe(fmt9.format(date8), `07/1/11 24:00`);
    shouldBe(fmt9.formatRange(date1, date2), `07/1/10 10:00～11:00`);
    shouldBe(fmt9.formatRange(date1, date3), `07/1/10 10:00～07/1/20 10:00`);
    shouldBe(fmt9.formatRange(date1, date5), `07/1/10 10:00～12:00`);
    shouldBe(fmt9.formatRange(date1, date6), `07/1/10 10:00～14:00`);
    shouldBe(fmt9.formatRange(date1, date7), `07/1/10 10:00～23:00`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt9.formatRange(date1, date8), `07/1/10 10:00～07/1/11 24:00`);

    let fmt10 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
    });
    shouldBe(fmt10.format(date1), `07/1/10 10:00`);
    shouldBe(fmt10.format(date8), `07/1/11 00:00`);
    shouldBe(fmt10.formatRange(date1, date2), `07/1/10 10時00分～11時00分`);
    shouldBe(fmt10.formatRange(date1, date3), `07/1/10 10:00～07/1/20 10:00`);
    shouldBe(fmt10.formatRange(date1, date5), `07/1/10 10時00分～12時00分`);
    shouldBe(fmt10.formatRange(date1, date6), `07/1/10 10時00分～14時00分`);
    shouldBe(fmt10.formatRange(date1, date7), `07/1/10 10時00分～23時00分`);
    shouldBe(fmt10.formatRange(date1, date8), `07/1/10 10:00～07/1/11 0:00`);

    let fmt11 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
    });
    shouldBe(fmt11.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt11.format(date8), `07/1/11 午前00:00`);
    shouldBe(fmt11.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt11.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt11.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt11.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt11.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt11.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);

    let fmt12 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
    });
    shouldBe(fmt12.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt12.format(date8), `07/1/11 午前12:00`);
    shouldBe(fmt12.formatRange(date1, date2), `07/1/10 午前10時00分～11時00分`);
    shouldBe(fmt12.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt12.formatRange(date1, date5), `07/1/10 午前10時00分～午後0時00分`);
    shouldBe(fmt12.formatRange(date1, date6), `07/1/10 午前10時00分～午後2時00分`);
    shouldBe(fmt12.formatRange(date1, date7), `07/1/10 午前10時00分～午後11時00分`);
    if ($vm.icuVersion() > 66)
        shouldBe(fmt12.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前12:00`);

    // "hc" + hour12.
    let fmt13 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h24',
        hour12: true,
    });
    shouldBe(fmt13.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt13.format(date8), `07/1/11 午前00:00`);
    shouldBe(fmt13.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt13.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt13.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt13.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt13.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt13.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);

    let fmt14 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h23',
        hour12: true,
    });
    shouldBe(fmt14.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt14.format(date8), `07/1/11 午前00:00`);
    shouldBe(fmt14.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt14.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt14.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt14.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt14.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt14.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);

    let fmt15 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h11',
        hour12: true,
    });
    shouldBe(fmt15.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt15.format(date8), `07/1/11 午前00:00`);
    shouldBe(fmt15.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt15.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt15.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt15.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt15.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt15.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);

    let fmt16 = new Intl.DateTimeFormat("ja", {
        year: '2-digit',
        month: 'numeric',
        day: 'numeric',
        hour: '2-digit',
        minute: 'numeric',
        timeZone: 'UTC',
        hourCycle: 'h12',
        hour12: true,
    });
    shouldBe(fmt16.format(date1), `07/1/10 午前10:00`);
    shouldBe(fmt16.format(date8), `07/1/11 午前00:00`);
    shouldBe(fmt16.formatRange(date1, date2), `07/1/10 午前10:00～午前11:00`);
    shouldBe(fmt16.formatRange(date1, date3), `07/1/10 午前10:00～07/1/20 午前10:00`);
    shouldBe(fmt16.formatRange(date1, date5), `07/1/10 午前10:00～午後0:00`);
    shouldBe(fmt16.formatRange(date1, date6), `07/1/10 午前10:00～午後2:00`);
    shouldBe(fmt16.formatRange(date1, date7), `07/1/10 午前10:00～午後11:00`);
    shouldBe(fmt16.formatRange(date1, date8), `07/1/10 午前10:00～07/1/11 午前0:00`);
}
