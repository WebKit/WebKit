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

function shouldNotThrow(func) {
    func();
}

shouldThrow(() => Intl.getCanonicalLocales("en-US-u-ca-u-nu"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-a-ok1-a-ok2"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("de-DE-u-email-co-phonebk-x-linux"));
shouldNotThrow(() => Intl.getCanonicalLocales("de-DE-u-email-co-phonebk"));
shouldThrow(() => Intl.getCanonicalLocales("de-DE-u-"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("de-DE-u"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-u-ca"));
shouldThrow(() => Intl.getCanonicalLocales("en-US-$"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-x"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-x-xxxxxxxxxxxxxxxxxxx"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-x-ok-ok-$"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-x-ok-ok"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-a-ok1"));
shouldThrow(() => Intl.getCanonicalLocales("en-US-a"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-a-xxxxxxxxxxxxxxxxxxx"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-a-ok-ok-$"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-a-ok-ok"));
if ($vm.icuVersion() < 64)
    shouldThrow(() => Intl.getCanonicalLocales("en-US-0-ok1"), RangeError);
else
    shouldNotThrow(() => Intl.getCanonicalLocales("en-US-0-ok1"));
shouldThrow(() => Intl.getCanonicalLocales("en-US-0"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-0-xxxxxxxxxxxxxxxxxxx"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-0-ok-ok-$"), RangeError);
if ($vm.icuVersion() < 64)
    shouldThrow(() => Intl.getCanonicalLocales("en-US-0-ok-ok"), RangeError);
else
    shouldNotThrow(() => Intl.getCanonicalLocales("en-US-0-ok-ok"));
shouldNotThrow(() => Intl.getCanonicalLocales("de"));
shouldNotThrow(() => Intl.getCanonicalLocales("fr"));
shouldNotThrow(() => Intl.getCanonicalLocales("ja"));
shouldThrow(() => Intl.getCanonicalLocales("i-enochian"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("zh-Hant"));
shouldNotThrow(() => Intl.getCanonicalLocales("zh-Hans"));
shouldNotThrow(() => Intl.getCanonicalLocales("sr-Cyrl"));
shouldNotThrow(() => Intl.getCanonicalLocales("sr-Latn"));
shouldThrow(() => Intl.getCanonicalLocales("zh-cmn-Hans-CN"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("cmn-Hans-CN"));
shouldThrow(() => Intl.getCanonicalLocales("zh-yue-HK"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("yue-HK"));
shouldNotThrow(() => Intl.getCanonicalLocales("zh-Hans-CN"));
shouldNotThrow(() => Intl.getCanonicalLocales("sr-Latn-RS"));
shouldNotThrow(() => Intl.getCanonicalLocales("sl-rozaj"));
shouldNotThrow(() => Intl.getCanonicalLocales("sl-rozaj-biske"));
shouldThrow(() => Intl.getCanonicalLocales("sl-rozaj-biske-biske"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("sl-nedis"));
shouldNotThrow(() => Intl.getCanonicalLocales("de-CH-1901"));
shouldNotThrow(() => Intl.getCanonicalLocales("sl-IT-nedis"));
shouldNotThrow(() => Intl.getCanonicalLocales("hy-Latn-IT-arevela"));
shouldNotThrow(() => Intl.getCanonicalLocales("de-DE"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US"));
shouldNotThrow(() => Intl.getCanonicalLocales("es-419"));
shouldNotThrow(() => Intl.getCanonicalLocales("de-CH-x-phonebk"));
shouldNotThrow(() => Intl.getCanonicalLocales("az-Arab-x-AZE-derbend"));
shouldThrow(() => Intl.getCanonicalLocales("x-whatever"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("qaa-Qaaa-QM-x-southern"));
shouldNotThrow(() => Intl.getCanonicalLocales("de-Qaaa"));
shouldNotThrow(() => Intl.getCanonicalLocales("sr-Latn-QM"));
shouldNotThrow(() => Intl.getCanonicalLocales("sr-Qaaa-RS"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-u-islamcal"));
shouldNotThrow(() => Intl.getCanonicalLocales("zh-CN-a-myext-x-private"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-a-myext-b-another"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-u-nu-fullwide"));
shouldThrow(() => Intl.getCanonicalLocales("de-419-DE"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("a-DE"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("aaa-DE"));
shouldThrow(() => Intl.getCanonicalLocales("aaaa-DE"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("aaaaa-DE"));
shouldThrow(() => Intl.getCanonicalLocales("ar-a-aaa-b-bbb-a-ccc"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales(""), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-u-ca"));
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-enen-GB"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-t"), RangeError);
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000-001"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000-a1-000"));
shouldNotThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000-a1-000-001"));
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000-a1-000-001-xxxxxxxxxxxxxxxxx"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-000-a1"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-a0-a1"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-en-GBBBBBBBBBB"), RangeError);
shouldThrow(() => Intl.getCanonicalLocales("en-US-t-en-GB-variant0-variant0"), RangeError);
