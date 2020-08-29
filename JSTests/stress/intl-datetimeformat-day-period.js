//@ runDefault("--useIntlDateTimeFormatDayPeriod=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

{
    let dtf = new Intl.DateTimeFormat("en", {hour: "numeric", dayPeriod: "short"});
    shouldBe(dtf.format(new Date("2019-05-20T07:00:00")), "7 in the morning");

    {
        let expected = [
            {type: "hour", value: "7"},
            {type: "literal", value: " "},
            {type: "dayPeriod", value: "in the morning"}
        ];
        let actual = dtf.formatToParts(new Date("2019-05-20T07:00:00"));
        shouldBe(actual.length, expected.length);
        for (let index = 0; index < expected.length; ++index) {
            shouldBe(actual[index].type, expected[index].type);
            shouldBe(actual[index].value, expected[index].value);
        }
    }

    shouldBe(dtf.format(new Date("2019-05-20T11:59:00")), "11 in the morning");
    shouldBe(dtf.format(new Date("2019-05-20T12:00:00")), "12 noon");
    shouldBe(dtf.format(new Date("2019-05-20T13:00:00")), "1 in the afternoon");
    shouldBe(dtf.format(new Date("2019-05-20T18:00:00")), "6 in the evening");
    shouldBe(dtf.format(new Date("2019-05-20T22:00:00")), "10 at night");

    dtf = new Intl.DateTimeFormat("zh-Hant", {hour: "numeric", dayPeriod: "short"});
    shouldBe(dtf.format(new Date("2019-05-20T07:00:00")), "清晨7時");
    {
        let expected = [
            {type: "dayPeriod", value: "清晨"},
            {type: "hour", value: "7"},
            {type: "literal", value: "時"}
        ];
        let actual = dtf.formatToParts(new Date("2019-05-20T07:00:00"));
        shouldBe(actual.length, expected.length);
        for (let index = 0; index < expected.length; ++index) {
            shouldBe(actual[index].type, expected[index].type);
            shouldBe(actual[index].value, expected[index].value);
        }
    }
    shouldBe(dtf.format(new Date("2019-05-20T11:59:00")), "上午11時");
    shouldBe(dtf.format(new Date("2019-05-20T12:00:00")), "中午12時");
    shouldBe(dtf.format(new Date("2019-05-20T13:00:00")), "下午1時");
    shouldBe(dtf.format(new Date("2019-05-20T18:00:00")), "下午6時");
    shouldBe(dtf.format(new Date("2019-05-20T22:00:00")), "晚上10時");
}
