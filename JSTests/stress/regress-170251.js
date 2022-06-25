Object.prototype.__defineGetter__(1000, () => 2);

let locales = [
    'mr', 'bs', 'ee-TG', 'ms', 'kam-KE', 'mt', 'ha', 'es-HN', 'ml-IN', 'ro-MD',
    'kab-DZ', 'he', 'es-CO', 'my', 'es-PA', 'az-Latn', 'mer', 'en-NZ', 'xog-UG',
    'sg', 'fr-GP', 'sr-Cyrl-BA', 'hi', 'fil-PH', 'lt-LT', 'si', 'en-MT', 'si-LK',
    'luo-KE', 'it-CH', 'teo', 'mfe', 'sk', 'uz-Cyrl-UZ', 'sl', 'rm-CH', 'az-Cyrl-AZ',
    'fr-GQ', 'kde', 'sn', 'cgg-UG', 'so', 'fr-RW', 'es-SV', 'mas-TZ', 'en-MU', 'sq',
    'hr', 'sr', 'en-PH', 'ca', 'hu', 'mk-MK', 'fr-TD', 'nb', 'sv', 'kln-KE', 'sw',
    'nd', 'sr-Latn', 'el-GR', 'hy', 'ne', 'el-CY', 'es-CR', 'fo-FO', 'pa-Arab-PK',
    'seh', 'ar-YE', 'ja-JP', 'ur-PK', 'pa-Guru', 'gl-ES', 'zh-Hant-HK', 'ar-EG', 'nl',
    'th-TH', 'es-PE', 'fr-KM', 'nn', 'kk-Cyrl-KZ', 'kea', 'lv-LV', 'kln', 'tzm-Latn',
    'yo', 'gsw-CH', 'ha-Latn-GH', 'is-IS', 'pt-BR', 'cs', 'en-PK', 'fa-IR', 'zh-Hans-SG',
    'luo', 'ta', 'fr-TG', 'kde-TZ', 'mr-IN', 'ar-SA', 'ka-GE', 'mfe-MU', 'id', 'fr-LU',
    'de-LU', 'ru-MD', 'cy', 'zh-Hans-HK', 'te', 'bg-BG', 'shi-Latn', 'ig', 'ses', 'ii',
    'es-BO', 'th', 'ko-KR', 'ti', 'it-IT', 'shi-Latn-MA', 'pt-MZ', 'ff-SN', 'haw',
    'zh-Hans', 'so-KE', 'bn-IN', 'en-UM', 'to', 'id-ID', 'uz-Cyrl', 'en-GU', 'es-EC',
    'en-US-posix', 'sr-Latn-BA', 'is', 'luy', 'tr', 'en-NA', 'it', 'da', 'bo-IN',
    'vun-TZ', 'ar-SD', 'uz-Latn-UZ', 'az-Latn-AZ', 'de', 'es-GQ', 'ta-IN', 'de-DE',
    'fr-FR', 'rof-TZ', 'ar-LY', 'en-BW', 'asa', 'zh', 'ha-Latn', 'fr-NE', 'es-MX',
    'bem-ZM', 'zh-Hans-CN', 'bn-BD', 'pt-GW', 'om', 'jmc', 'de-AT', 'kk-Cyrl', 'sw-TZ',
    'ar-OM', 'et-EE', 'or', 'da-DK', 'ro-RO', 'zh-Hant', 'bm-ML', 'ja', 'fr-CA', 'naq',
    'zu', 'en-IE', 'ar-MA', 'es-GT', 'uz-Arab-AF', 'en-AS', 'bs-BA', 'am-ET', 'ar-TN',
    'haw-US', 'ar-JO', 'fa-AF', 'uz-Latn', 'en-BZ', 'nyn-UG', 'ebu-KE', 'te-IN', 'cy-GB',
    'uk', 'nyn', 'en-JM', 'en-US', 'fil', 'ar-KW', 'af-ZA', 'en-CA', 'fr-DJ', 'ti-ER',
    'ig-NG', 'en-AU', 'ur', 'fr-MC', 'pt-PT', 'pa', 'es-419', 'fr-CD', 'en-SG', 'bo-CN',
    'kn-IN', 'sr-Cyrl-RS', 'lg-UG', 'gu-IN', 'ee', 'nd-ZW', 'bem', 'uz', 'sw-KE',
    'sq-AL', 'hr-HR', 'mas-KE', 'el', 'ti-ET', 'es-AR', 'pl', 'en', 'eo', 'shi', 'kok',
    'fr-CF', 'fr-RE', 'mas', 'rof', 'ru-UA', 'yo-NG', 'dav-KE', 'gv-GB', 'pa-Arab', 'es',
    'teo-UG', 'ps', 'es-PR', 'fr-MF', 'et', 'pt', 'eu', 'ka', 'rwk-TZ', 'nb-NO', 'fr-CG'
];
Intl.getCanonicalLocales(locales);
