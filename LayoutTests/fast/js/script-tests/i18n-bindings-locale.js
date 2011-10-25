description(

"This test checks properties and methods of Locale object."

);

function reportResult(_actual, _expected) {
    if (isResultCorrect(_actual, _expected))
        testPassed(_actual + ' is ' + _expected);
    else
        testFailed(_actual + ' should be ' + _expected);
}

localeTests = function() {
    this.defaultLocale = function() {
        var locale = new v8Locale();
        reportResult(locale.locale, 'en-US');
        reportResult(locale.language, 'en');
        reportResult(locale.script, undefined);
        reportResult(locale.region, 'US');
    };

    this.baseLocale = function() {
        var locale = new v8Locale('sr');
        reportResult(locale.locale, 'sr');
        reportResult(locale.language, 'sr');
        reportResult(locale.script, undefined);
        reportResult(locale.region, undefined);
    };

    this.languageScriptLocale = function() {
        var locale = new v8Locale('zh-Hans');
        reportResult(locale.locale, 'zh-Hans');
        reportResult(locale.language, 'zh');
        reportResult(locale.script, 'Hans');
        reportResult(locale.region, undefined);
    };

    this.languageScriptRegionLocale = function() {
        var locale = new v8Locale('zh-Hans-CN');
        reportResult(locale.locale, 'zh-Hans-CN');
        reportResult(locale.language, 'zh');
        reportResult(locale.script, 'Hans');
        reportResult(locale.region, 'CN');
    };

    this.languageScriptRegionExtensionLocale = function() {
        var locale = new v8Locale('de-DE@collation=phone');
        reportResult(locale.locale, 'de-DE@collation=phone');
        reportResult(locale.language, 'de');
        reportResult(locale.script, undefined);
        reportResult(locale.region, 'DE');
    };

    this.languageScriptRegionDashUExtensionLocale = function() {
        var locale = new v8Locale('de-DE-u-co-phonebook');
        reportResult(locale.locale, 'de-DE-u-co-phonebook');
        reportResult(locale.language, 'de');
        reportResult(locale.script, undefined);
        reportResult(locale.region, 'DE');
    };
  
    this.availableLocales = function() {
        var locales = v8Locale.availableLocales();
        reportResult(locales.length > 100, true);
        reportResult(locales.join().indexOf('sr') >= 0, true);
    };
  
    this.maximizedLocale = function() {
        var locale = new v8Locale('sr').maximizedLocale();
        reportResult(locale.locale, 'sr-Cyrl-RS');
        reportResult(locale.language, 'sr');
        reportResult(locale.script, 'Cyrl');
        reportResult(locale.region, 'RS');
    };
  
    this.maximizedLocaleWithExtension = function() {
        var locale = new v8Locale('de@collation=phone').maximizedLocale();
        reportResult(locale.locale, 'de-Latn-DE@collation=phone');
        reportResult(locale.language, 'de');
        reportResult(locale.script, 'Latn');
        reportResult(locale.region, 'DE');
    };
  
    this.minimizedLocale = function() {
        var locale = new v8Locale('sr-Cyrl-RS').minimizedLocale();
        reportResult(locale.locale, 'sr');
        reportResult(locale.language, 'sr');
        reportResult(locale.script, undefined);
        reportResult(locale.region, undefined);
    };
  
    this.minimizedLocaleWithExtension = function() {
        var locale = new v8Locale('de-Latn-DE@collation=phone').minimizedLocale();
        // FIXME: Fix ICU to return de@collation=phone.
        reportResult(locale.locale, 'de-@collation=phone');
        reportResult(locale.language, 'de');
        reportResult(locale.script, undefined);
        reportResult(locale.region, undefined);
    };
    
    this.displayForBaseLocale = function() {
        var locale = new v8Locale('en');
        reportResult(locale.displayLanguage(), 'English');
        reportResult(locale.displayScript(), undefined);
        reportResult(locale.displayRegion(), undefined);
        reportResult(locale.displayName(), 'English');
    };
  
    this.displayInSameLocale = function() {
        var locale = new v8Locale('en').maximizedLocale();
        reportResult(locale.displayLanguage(), 'English');
        reportResult(locale.displayScript(), 'Latin');
        reportResult(locale.displayRegion(), 'United States');
        reportResult(locale.displayName(), 'English (Latin, United States)');
    };
  
    this.displayInDifferentLocale = function() {
        var locale = new v8Locale('sr').maximizedLocale();
        var displayLocale = new v8Locale('en');
        reportResult(locale.displayLanguage(displayLocale), 'Serbian');
        reportResult(locale.displayScript(displayLocale), 'Cyrillic');
        reportResult(locale.displayRegion(displayLocale), 'Serbia');
        reportResult(locale.displayName(displayLocale), 'Serbian (Cyrillic, Serbia)');
    };
  
    this.displayInCyrillicScript = function() {
        var locale = new v8Locale('sr').maximizedLocale();
        reportResult(locale.displayLanguage(), 'Српски');
        reportResult(locale.displayScript(), 'Ћирилица');
        reportResult(locale.displayRegion(), 'Србија');
        reportResult(locale.displayName(), 'Српски (Ћирилица, Србија)');
    };
  
    this.displayInHebrewScript = function() {
        var locale = new v8Locale('he').maximizedLocale();
        reportResult(locale.displayLanguage(), 'עברית');
        reportResult(locale.displayScript(), 'עברי');
        reportResult(locale.displayRegion(), 'ישראל');
        reportResult(locale.displayName(), 'עברית (עברי, ישראל)');
    };
};

(function() {
    var allTests = new localeTests();
    for (var test in allTests) {
        allTests[test]();
    }
})();
