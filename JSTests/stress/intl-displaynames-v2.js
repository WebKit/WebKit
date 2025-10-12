function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
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

{
    let displayNames = new Intl.DisplayNames(undefined, {type: 'calendar'});
    shouldThrow(() => {
        displayNames.of('');
    }, `RangeError: argument is not a calendar code`);
    shouldThrow(() => {
        displayNames.of('-');
    }, `RangeError: argument is not a calendar code`);
    shouldThrow(() => {
        displayNames.of('aaaaa-');
    }, `RangeError: argument is not a calendar code`);
    shouldThrow(() => {
        displayNames.of('_');
    }, `RangeError: argument is not a calendar code`);
    shouldThrow(() => {
        displayNames.of('aaaaa_');
    }, `RangeError: argument is not a calendar code`); }
{
    let dn = new Intl.DisplayNames("en", {type: "calendar"})
    shouldBe(dn.of("roc"), "Minguo Calendar");
    shouldBe(dn.of("persian"), "Persian Calendar");
    shouldBe(dn.of("gregory"), "Gregorian Calendar");
    shouldBe(dn.of("ethioaa"), "Ethiopic Amete Alem Calendar");
    shouldBe(dn.of("japanese"), "Japanese Calendar");
    shouldBeOneOf(dn.of("dangi"), ["Dangi Calendar", "Korean Calendar"]);
    shouldBe(dn.of("chinese"), "Chinese Calendar");
}
{
    let dn = new Intl.DisplayNames("zh", {type: "calendar"})
    shouldBe(dn.of("roc"), "民国纪年");
    shouldBe(dn.of("persian"), "波斯历");
    shouldBe(dn.of("gregory"), "公历");
    shouldBe(dn.of("ethioaa"), "埃塞俄比亚阿米特阿莱姆日历");
    shouldBe(dn.of("japanese"), "和历");
    shouldBeOneOf(dn.of("dangi"), ["檀纪历", "韩国历"]);
    shouldBe(dn.of("chinese"), "农历");
}
{
    let dn = new Intl.DisplayNames("zh", {type: "dateTimeField"})
    shouldBe(dn.of("era"), "纪元");
    shouldBe(dn.of("year"), "年");
    shouldBe(dn.of("month"), "月");
    shouldBe(dn.of("quarter"), "季度");
    shouldBe(dn.of("weekOfYear"), "周");
    shouldBeOneOf(dn.of("weekday"), ["工作日", "星期"]);
    shouldBe(dn.of("dayPeriod"), "上午/下午");
    shouldBe(dn.of("day"), "日");
    shouldBe(dn.of("hour"), "小时");
    shouldBe(dn.of("minute"), "分钟");
    shouldBe(dn.of("second"), "秒");
}
{
    let dn = new Intl.DisplayNames("es", {type: "dateTimeField"})
    shouldBe(dn.of("era"), "era");
    shouldBe(dn.of("year"), "año");
    shouldBe(dn.of("month"), "mes");
    shouldBe(dn.of("quarter"), "trimestre");
    shouldBe(dn.of("weekOfYear"), "semana");
    shouldBe(dn.of("weekday"), "día de la semana");
    let dayPeriod = dn.of("dayPeriod");
    shouldBe(dayPeriod === "a. m./p. m." || dayPeriod === "a. m./p. m.", true);
    shouldBe(dn.of("day"), "día");
    shouldBe(dn.of("hour"), "hora");
    shouldBe(dn.of("minute"), "minuto");
    shouldBe(dn.of("second"), "segundo");
}
{
    let dn1 = new Intl.DisplayNames("en", {type: "language"})
    shouldBe(dn1.of("en"), "English");
    shouldBe(dn1.of("en-GB"), "British English");
    shouldBe(dn1.of("en-US"), "American English");
    shouldBe(dn1.of("en-AU"), "Australian English");
    shouldBe(dn1.of("en-CA"), "Canadian English");
    shouldBe(dn1.of("zh"), "Chinese");
    shouldBe(dn1.of("zh-Hant") === "Traditional Chinese" || dn1.of("zh-Hant") === "Chinese, Traditional", true);
    shouldBe(dn1.of("zh-Hans") === "Simplified Chinese" || dn1.of("zh-Hans") === "Chinese, Simplified", true);
}
{
    let dn2 = new Intl.DisplayNames("en", {type: "language", languageDisplay: "dialect"})
    shouldBe(dn2.of("en"), "English");
    shouldBe(dn2.of("en-GB"), "British English");
    shouldBe(dn2.of("en-US"), "American English");
    shouldBe(dn2.of("en-AU"), "Australian English");
    shouldBe(dn2.of("en-CA"), "Canadian English");
    shouldBe(dn2.of("zh"), "Chinese");
    shouldBe(dn2.of("zh-Hant") === "Traditional Chinese" || dn2.of("zh-Hant") === "Chinese, Traditional", true);
    shouldBe(dn2.of("zh-Hans") === "Simplified Chinese" || dn2.of("zh-Hans") === "Chinese, Simplified", true);
}
{
    let dn3 = new Intl.DisplayNames("en", {type: "language", languageDisplay: "standard"})
    shouldBe(dn3.of("en"), "English");
    shouldBe(dn3.of("en-GB") === "English (United Kingdom)" || dn3.of("en-GB") === "English (UK)", true);
    shouldBe(dn3.of("en-AU"), "English (Australia)");
    shouldBe(dn3.of("en-CA"), "English (Canada)");
    shouldBe(dn3.of("en-US") === "English (United States)" || dn3.of("en-US") === "English (US)", true);
    shouldBe(dn3.of("zh"), "Chinese");
    shouldBe(dn3.of("zh-Hant") === "Chinese (Traditional)" || dn3.of("zh-Hant") === "Chinese, Traditional", true);
    shouldBe(dn3.of("zh-Hans") === "Chinese (Simplified)" || dn3.of("zh-Hans") === "Chinese, Simplified", true);
}
