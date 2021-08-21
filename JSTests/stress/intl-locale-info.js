function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

{
    let he = new Intl.Locale("he")
    shouldBe(JSON.stringify(he.weekInfo), `{"firstDay":7,"weekendStart":5,"weekendEnd":6,"minimalDays":1}`);
    let af = new Intl.Locale("af")
    shouldBe(JSON.stringify(af.weekInfo), `{"firstDay":7,"weekendStart":6,"weekendEnd":7,"minimalDays":1}`);
    let enGB = new Intl.Locale("en-GB")
    shouldBe(JSON.stringify(enGB.weekInfo), `{"firstDay":1,"weekendStart":6,"weekendEnd":7,"minimalDays":4}`);
}
{
    let l = new Intl.Locale("ar")
    shouldBe(JSON.stringify(l.textInfo), `{"direction":"rtl"}`);
}
{
    let locale = new Intl.Locale("ar")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","coptic","islamic","islamicc","islamic-tbla"]`);
    shouldBe(JSON.stringify(locale.collations), `["compat","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    let ns = JSON.stringify(locale.numberingSystems);
    shouldBe(ns === `["arab"]` || ns === `["latn"]`, true);
    shouldBe(locale.timeZones, undefined);
}
{
    let locale = new Intl.Locale("ar-EG")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","coptic","islamic","islamicc","islamic-tbla"]`);
    shouldBe(JSON.stringify(locale.collations), `["compat","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["arab"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Africa/Cairo"]`);
}
{
    let locale = new Intl.Locale("ar-SA")
    shouldBe(JSON.stringify(locale.calendars), `["islamic-umalqura","islamic-rgsa","islamic","gregory"]`);
    shouldBe(JSON.stringify(locale.collations), `["compat","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["arab"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Asia/Riyadh"]`);
}
{
    let locale = new Intl.Locale("ja")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","japanese"]`);
    shouldBe(JSON.stringify(locale.collations), `["unihan","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h23"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(locale.timeZones, undefined);
}
{
    let locale = new Intl.Locale("ja-JP")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","japanese"]`);
    shouldBe(JSON.stringify(locale.collations), `["unihan","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h23"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Asia/Tokyo"]`);
}
{
    let locale = new Intl.Locale("en-US")
    shouldBe(JSON.stringify(locale.calendars), `["gregory"]`);
    shouldBe(JSON.stringify(locale.collations), `["emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["America/Adak","America/Anchorage","America/Boise","America/Chicago","America/Denver","America/Detroit","America/Indiana/Knox","America/Indiana/Marengo","America/Indiana/Petersburg","America/Indiana/Tell_City","America/Indiana/Vevay","America/Indiana/Vincennes","America/Indiana/Winamac","America/Indianapolis","America/Juneau","America/Kentucky/Monticello","America/Los_Angeles","America/Louisville","America/Menominee","America/Metlakatla","America/New_York","America/Nome","America/North_Dakota/Beulah","America/North_Dakota/Center","America/North_Dakota/New_Salem","America/Phoenix","America/Sitka","America/Yakutat","Pacific/Honolulu"]`);
}
{
    let locale = new Intl.Locale("en-NZ")
    shouldBe(JSON.stringify(locale.calendars), `["gregory"]`);
    shouldBe(JSON.stringify(locale.collations), `["emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Pacific/Auckland","Pacific/Chatham"]`);
}
{
    let locale = new Intl.Locale("zh")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","chinese"]`);
    shouldBe(JSON.stringify(locale.collations), `["pinyin","big5han","gb2312","stroke","unihan","zhuyin","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(locale.timeZones, undefined);
}
{
    let locale = new Intl.Locale("zh-TW")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","roc","chinese"]`);
    shouldBe(JSON.stringify(locale.collations), `["stroke","big5han","gb2312","pinyin","unihan","zhuyin","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Asia/Taipei"]`);
}
{
    let locale = new Intl.Locale("zh-HK")
    shouldBe(JSON.stringify(locale.calendars), `["gregory","chinese"]`);
    shouldBe(JSON.stringify(locale.collations), `["stroke","big5han","gb2312","pinyin","unihan","zhuyin","emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h12"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["latn"]`);
    shouldBe(JSON.stringify(locale.timeZones), `["Asia/Hong_Kong"]`);
}
{
    let locale = new Intl.Locale("fa")
    shouldBe(JSON.stringify(locale.calendars), `["persian","gregory","islamic","islamicc","islamic-tbla"]`);
    shouldBe(JSON.stringify(locale.collations), `["emoji","eor"]`);
    shouldBe(JSON.stringify(locale.hourCycles), `["h23"]`);
    shouldBe(JSON.stringify(locale.numberingSystems), `["arabext"]`);
    shouldBe(locale.timeZones, undefined);
}
