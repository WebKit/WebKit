function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

let timeZoneNames = ["short", "long", "shortOffset", "longOffset", "shortGeneric", "longGeneric"];
let date = new Date(1625939282389);

function timeZoneTest(date, locale, expectedSet, timeZone = "America/Los_Angeles")
{
    timeZoneNames.forEach(function(timeZoneName) {
        let formatter = new Intl.DateTimeFormat(locale, {
            year: "numeric",
            month: "numeric",
            day: "numeric",
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit",
            timeZone,
            timeZoneName
        });
        let actual = formatter.format(date);
        // print(`${timeZoneName}: "${actual}",`);
        let expected = expectedSet[timeZoneName];
        if (Array.isArray(expected))
            shouldBe(expected.includes(actual), true);
        else
            shouldBe(actual, expected);
    });
}

timeZoneTest(date, "en", {
    short: "7/10/2021, 10:48:02 AM PDT",
    long: "7/10/2021, 10:48:02 AM Pacific Daylight Time",
    shortOffset: "7/10/2021, 10:48:02 AM GMT-7",
    longOffset: "7/10/2021, 10:48:02 AM GMT-07:00",
    shortGeneric: "7/10/2021, 10:48:02 AM PT",
    longGeneric: "7/10/2021, 10:48:02 AM Pacific Time",
});

timeZoneTest(date, "en", {
    short: "7/10/2021, 11:18:02 PM GMT+5:30",
    long: "7/10/2021, 11:18:02 PM India Standard Time",
    shortOffset: "7/10/2021, 11:18:02 PM GMT+5:30",
    longOffset: "7/10/2021, 11:18:02 PM GMT+05:30",
    shortGeneric: "7/10/2021, 11:18:02 PM India Time",
    longGeneric: "7/10/2021, 11:18:02 PM India Standard Time",
}, "Asia/Calcutta");

timeZoneTest(date, "zh-Hant", {
    short: ["2021/7/10 PDT 上午10:48:02", "2021/7/10 上午10:48:02 [PDT]"],
    long: ["2021/7/10 太平洋夏令時間 上午10:48:02", "2021/7/10 上午10:48:02 [太平洋夏令時間]"],
    shortOffset: ["2021/7/10 GMT-7 上午10:48:02", "2021/7/10 上午10:48:02 [GMT-7]"],
    longOffset: ["2021/7/10 GMT-07:00 上午10:48:02", "2021/7/10 上午10:48:02 [GMT-07:00]"],
    shortGeneric: ["2021/7/10 PT 上午10:48:02", "2021/7/10 上午10:48:02 [PT]"],
    longGeneric: ["2021/7/10 太平洋時間 上午10:48:02", "2021/7/10 上午10:48:02 [太平洋時間]"],
});

timeZoneTest(date, "ja-JP", {
    short: "2021/7/10 10:48:02 GMT-7",
    long: "2021/7/10 10時48分02秒 アメリカ太平洋夏時間",
    shortOffset: "2021/7/10 10時48分02秒 GMT-7",
    longOffset: "2021/7/10 10時48分02秒 GMT-07:00",
    shortGeneric: "2021/7/10 10:48:02 ロサンゼルス時間",
    longGeneric: "2021/7/10 10:48:02 アメリカ太平洋時間",
});

timeZoneTest(date, "ja-JP", {
    short: "2021/7/11 02:48:02 JST",
    long: "2021/7/11 02時48分02秒 日本標準時",
    shortOffset: "2021/7/11 02時48分02秒 GMT+9",
    longOffset: "2021/7/11 02時48分02秒 GMT+09:00",
    shortGeneric: "2021/7/11 02:48:02 JST",
    longGeneric: "2021/7/11 02:48:02 日本標準時",
}, "Asia/Tokyo");
