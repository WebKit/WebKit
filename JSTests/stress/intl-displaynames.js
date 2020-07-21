//@ runDefault("--useIntlDisplayNames=1")
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

if ($vm.icuVersion() >= 61) {
    shouldBe(Intl.DisplayNames.length, 2);
    shouldThrow(() => new Intl.DisplayNames, `TypeError: undefined is not an object (evaluating 'new Intl.DisplayNames')`);

    // Get display names of region in English
    var regionNames = new Intl.DisplayNames(['en'], {type: 'region'});
    if ($vm.icuVersion() >= 64)
        shouldBe(regionNames.of('419'), "Latin America");
    shouldBe(regionNames.of('BZ'), "Belize");
    shouldBe(regionNames.of('US'), "United States");
    shouldBe(regionNames.of('BA'), "Bosnia & Herzegovina");
    shouldBe(regionNames.of('MM'), "Myanmar (Burma)");

    // Get display names of region in Traditional Chinese
    regionNames = new Intl.DisplayNames(['zh-Hant'], {type: 'region'});
    if ($vm.icuVersion() >= 64)
        shouldBe(regionNames.of('419'), "拉丁美洲");
    shouldBe(regionNames.of('BZ'), "貝里斯");
    shouldBe(regionNames.of('US'), "美國");
    shouldBe(regionNames.of('BA'), "波士尼亞與赫塞哥維納");
    shouldBe(regionNames.of('MM'), "緬甸");

    regionNames = new Intl.DisplayNames(['en'], {type: 'region', style: 'short'});
    if ($vm.icuVersion() >= 64)
        shouldBe(regionNames.of('419'), "Latin America");
    shouldBe(regionNames.of('BZ'), "Belize");
    shouldBe(regionNames.of('US'), "US");
    shouldBe(regionNames.of('BA'), "Bosnia");
    shouldBe(regionNames.of('MM'), "Myanmar");

    regionNames = new Intl.DisplayNames(['en'], {type: 'region', style: 'narrow'});
    if ($vm.icuVersion() >= 64)
        shouldBe(regionNames.of('419'), "Latin America");
    shouldBe(regionNames.of('BZ'), "Belize");
    shouldBe(regionNames.of('US'), "US");
    shouldBe(regionNames.of('BA'), "Bosnia");
    shouldBe(regionNames.of('MM'), "Myanmar");

    // Get display names of language in English
    var languageNames = new Intl.DisplayNames(['en'], {type: 'language'});
    shouldBe(languageNames.of('fr'), "French");
    shouldBe(languageNames.of('de'), "German");
    shouldBe(languageNames.of('fr-CA'), "Canadian French");
    if ($vm.icuVersion() >= 64 && $vm.icuVersion() <= 66)
        shouldBe(languageNames.of('zh-Hant'), "Chinese, Traditional");
    else
        shouldBe(languageNames.of('zh-Hant'), "Traditional Chinese");
    shouldBe(languageNames.of('en-US'), "American English");
    shouldBe(languageNames.of('zh-TW'), "Chinese (Taiwan)");

    // Get display names of language in Traditional Chinese
    languageNames = new Intl.DisplayNames(['zh-Hant'], {type: 'language'});
    shouldBe(languageNames.of('fr'), "法文");
    shouldBe(languageNames.of('zh'), "中文");
    shouldBe(languageNames.of('de'), "德文");

    languageNames = new Intl.DisplayNames(['en'], {type: 'language', style: 'short'});
    shouldBe(languageNames.of('fr'), "French");
    shouldBe(languageNames.of('de'), "German");
    shouldBe(languageNames.of('fr-CA'), "Canadian French");
    if ($vm.icuVersion() >= 64 && $vm.icuVersion() <= 66)
        shouldBe(languageNames.of('zh-Hant'), "Chinese, Traditional");
    else
        shouldBe(languageNames.of('zh-Hant'), "Traditional Chinese");
    shouldBe(languageNames.of('en-US'), "US English");
    shouldBe(languageNames.of('zh-TW'), "Chinese (Taiwan)");

    languageNames = new Intl.DisplayNames(['en'], {type: 'language', style: 'narrow'});
    shouldBe(languageNames.of('fr'), "French");
    shouldBe(languageNames.of('de'), "German");
    shouldBe(languageNames.of('fr-CA'), "Canadian French");
    if ($vm.icuVersion() >= 64 && $vm.icuVersion() <= 66)
        shouldBe(languageNames.of('zh-Hant'), "Chinese, Traditional");
    else
        shouldBe(languageNames.of('zh-Hant'), "Traditional Chinese");
    shouldBe(languageNames.of('en-US'), "US English");
    shouldBe(languageNames.of('zh-TW'), "Chinese (Taiwan)");

    // Get display names of script in English
    var scriptNames = new Intl.DisplayNames(['en'], {type: 'script'});
    // Get script names
    shouldBe(scriptNames.of('Latn'), "Latin");
    shouldBe(scriptNames.of('Arab'), "Arabic");
    shouldBe(scriptNames.of('Kana'), "Katakana");

    // Get display names of script in Traditional Chinese
    scriptNames = new Intl.DisplayNames(['zh-Hant'], {type: 'script'});
    shouldBe(scriptNames.of('Latn'), "拉丁文");
    shouldBe(scriptNames.of('Arab'), "阿拉伯文");
    shouldBe(scriptNames.of('Kana'), "片假名");

    scriptNames = new Intl.DisplayNames(['en'], {type: 'script', style: 'short'});
    shouldBe(scriptNames.of('Latn'), "Latin");
    shouldBe(scriptNames.of('Arab'), "Arabic");
    shouldBe(scriptNames.of('Kana'), "Katakana");

    scriptNames = new Intl.DisplayNames(['en'], {type: 'script', style: 'narrow'});
    shouldBe(scriptNames.of('Latn'), "Latin");
    shouldBe(scriptNames.of('Arab'), "Arabic");
    shouldBe(scriptNames.of('Kana'), "Katakana");

    // Get display names of currency code in English
    var currencyNames = new Intl.DisplayNames(['en'], {type: 'currency'});
    // Get currency names
    shouldBe(currencyNames.of('USD'), "US Dollar");
    shouldBe(currencyNames.of('EUR'), "Euro");
    shouldBe(currencyNames.of('TWD'), "New Taiwan Dollar");
    shouldBe(currencyNames.of('CNY'), "Chinese Yuan");
    shouldBe(currencyNames.of('JPY'), "Japanese Yen");

    // Get display names of currency code in Traditional Chinese
    currencyNames = new Intl.DisplayNames(['zh-Hant'], {type: 'currency'});
    shouldBe(currencyNames.of('USD'), "美元");
    shouldBe(currencyNames.of('EUR'), "歐元");
    shouldBe(currencyNames.of('TWD'), "新台幣");
    shouldBe(currencyNames.of('CNY'), "人民幣");
    shouldBe(currencyNames.of('JPY'), "日圓");

    // Get display names of currency code in English in short
    currencyNames = new Intl.DisplayNames(['en'], {type: 'currency', style: 'short' });
    shouldBe(currencyNames.of('USD'), "$");
    shouldBe(currencyNames.of('EUR'), "€");
    shouldBe(currencyNames.of('TWD'), "NT$");
    shouldBe(currencyNames.of('CNY'), "CN¥");
    shouldBe(currencyNames.of('JPY'), "¥");

    // Get display names of currency code in English in narrow
    currencyNames = new Intl.DisplayNames(['en'], {type: 'currency', style: 'narrow' });
    shouldBe(currencyNames.of('USD'), "$");
    shouldBe(currencyNames.of('EUR'), "€");
    shouldBe(currencyNames.of('TWD'), "$");
    shouldBe(currencyNames.of('CNY'), "¥");
    shouldBe(currencyNames.of('JPY'), "¥");
    shouldBe(currencyNames.of('JPZ'), "JPZ"); // Fallback code
    shouldBe(JSON.stringify(currencyNames.resolvedOptions()), `{"locale":"en","style":"narrow","type":"currency","fallback":"code"}`);

    currencyNames = new Intl.DisplayNames(['en'], {type: 'currency', style: 'narrow', fallback: 'none' });
    shouldBe(currencyNames.of('JPZ'), undefined); // Fallback none
    shouldBe(JSON.stringify(currencyNames.resolvedOptions()), `{"locale":"en","style":"narrow","type":"currency","fallback":"none"}`);

    regionNames = new Intl.DisplayNames(['en'], {type: 'region'});
    shouldBe(regionNames.of('999'), "999");
    shouldBe(JSON.stringify(regionNames.resolvedOptions()), `{"locale":"en","style":"long","type":"region","fallback":"code"}`);

    regionNames = new Intl.DisplayNames(['en'], {type: 'region', fallback:'none'});
    if ($vm.icuVersion() > 64)
        shouldBe(regionNames.of('999'), undefined);
    shouldBe(JSON.stringify(regionNames.resolvedOptions()), `{"locale":"en","style":"long","type":"region","fallback":"none"}`);

    languageNames = new Intl.DisplayNames(['en'], {type: 'language'});
    if ($vm.icuVersion() > 64) {
        shouldBe(languageNames.of('en-AA'), "en-AA");
        var object = {
            toString() {
                return 'en-AA';
            }
        };
        shouldBe(languageNames.of(object), object);
    }

    languageNames = new Intl.DisplayNames(['en'], {type: 'language', fallback: 'none'});
    if ($vm.icuVersion() > 64) {
        shouldBe(languageNames.of('en-AA'), undefined);
        shouldBe(languageNames.of(object), undefined);
    }

    shouldBe(Intl.DisplayNames.prototype[Symbol.toStringTag], `Intl.DisplayNames`);
    shouldBe(JSON.stringify(Intl.DisplayNames.supportedLocalesOf("en")), `["en"]`);
    shouldBe(JSON.stringify(Intl.DisplayNames.supportedLocalesOf("ja-JP")), `["ja-JP"]`);
    shouldBe(JSON.stringify(Intl.DisplayNames.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
    shouldThrow(() => Intl.DisplayNames.supportedLocalesOf(""), `RangeError: invalid language tag: `)

    shouldThrow(() => {
        Intl.DisplayNames.prototype.of.call({});
    }, `TypeError: Intl.DisplayNames.prototype.of called on value that's not an object initialized as a DisplayNames`);

    shouldThrow(() => {
        Intl.DisplayNames.prototype.resolvedOptions.call({});
    }, `TypeError: Intl.DisplayNames.prototype.resolvedOptions called on value that's not an object initialized as a DisplayNames`);
}
