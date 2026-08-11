/******/ (() => { // webpackBootstrap
/******/ 	// runtime can't be in strict mode because a global variable is assign and maybe created.
/******/ 	var __webpack_modules__ = ({

/***/ 82:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isEthereumAddress;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var eth = /^(0x)[0-9a-f]{40}$/i;
function isEthereumAddress(str) {
  (0, _assertString.default)(str);
  return eth.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 317:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = trim;
var _rtrim = _interopRequireDefault(__webpack_require__(2483));
var _ltrim = _interopRequireDefault(__webpack_require__(2309));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function trim(str, chars) {
  return (0, _rtrim.default)((0, _ltrim.default)(str, chars), chars);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 410:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = blacklist;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function blacklist(str, chars) {
  (0, _assertString.default)(str);
  return str.replace(new RegExp("[".concat(chars, "]+"), 'g'), '');
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 561:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = stripLow;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _blacklist = _interopRequireDefault(__webpack_require__(410));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function stripLow(str, keep_new_lines) {
  (0, _assertString.default)(str);
  var chars = keep_new_lines ? '\\x00-\\x09\\x0B\\x0C\\x0E-\\x1F\\x7F' : '\\x00-\\x1F\\x7F';
  return (0, _blacklist.default)(str, chars);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 604:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISSN;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var issn = '^\\d{4}-?\\d{3}[\\dX]$';
function isISSN(str) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  (0, _assertString.default)(str);
  var testIssn = issn;
  testIssn = options.require_hyphen ? testIssn.replace('?', '') : testIssn;
  testIssn = options.case_sensitive ? new RegExp(testIssn) : new RegExp(testIssn, 'i');
  if (!testIssn.test(str)) {
    return false;
  }
  var digits = str.replace('-', '').toUpperCase();
  var checksum = 0;
  for (var i = 0; i < digits.length; i++) {
    var digit = digits[i];
    checksum += (digit === 'X' ? 10 : +digit) * (8 - i);
  }
  return checksum % 11 === 0;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 629:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isWhitelisted;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isWhitelisted(str, chars) {
  (0, _assertString.default)(str);
  for (var i = str.length - 1; i >= 0; i--) {
    if (chars.indexOf(str[i]) === -1) {
      return false;
    }
  }
  return true;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 676:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLicensePlate;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var validators = {
  'cs-CZ': function csCZ(str) {
    return /^(([ABCDEFHIJKLMNPRSTUVXYZ]|[0-9])-?){5,8}$/.test(str);
  },
  'de-DE': function deDE(str) {
    return /^((A|AA|AB|AC|AE|AH|AK|AM|AN|A\u00d6|AP|AS|AT|AU|AW|AZ|B|BA|BB|BC|BE|BF|BH|BI|BK|BL|BM|BN|BO|B\u00d6|BS|BT|BZ|C|CA|CB|CE|CO|CR|CW|D|DA|DD|DE|DH|DI|DL|DM|DN|DO|DU|DW|DZ|E|EA|EB|ED|EE|EF|EG|EH|EI|EL|EM|EN|ER|ES|EU|EW|F|FB|FD|FF|FG|FI|FL|FN|FO|FR|FS|FT|F\u00dc|FW|FZ|G|GA|GC|GD|GE|GF|GG|GI|GK|GL|GM|GN|G\u00d6|GP|GR|GS|GT|G\u00dc|GV|GW|GZ|H|HA|HB|HC|HD|HE|HF|HG|HH|HI|HK|HL|HM|HN|HO|HP|HR|HS|HU|HV|HX|HY|HZ|IK|IL|IN|IZ|J|JE|JL|K|KA|KB|KC|KE|KF|KG|KH|KI|KK|KL|KM|KN|KO|KR|KS|KT|KU|KW|KY|L|LA|LB|LC|LD|LF|LG|LH|LI|LL|LM|LN|L\u00d6|LP|LR|LU|M|MA|MB|MC|MD|ME|MG|MH|MI|MK|ML|MM|MN|MO|MQ|MR|MS|M\u00dc|MW|MY|MZ|N|NB|ND|NE|NF|NH|NI|NK|NM|N\u00d6|NP|NR|NT|NU|NW|NY|NZ|OA|OB|OC|OD|OE|OF|OG|OH|OK|OL|OP|OS|OZ|P|PA|PB|PE|PF|PI|PL|PM|PN|PR|PS|PW|PZ|R|RA|RC|RD|RE|RG|RH|RI|RL|RM|RN|RO|RP|RS|RT|RU|RV|RW|RZ|S|SB|SC|SE|SG|SI|SK|SL|SM|SN|SO|SP|SR|ST|SU|SW|SY|SZ|TE|TF|TG|TO|TP|TR|TS|TT|T\u00dc|\u00dcB|UE|UH|UL|UM|UN|V|VB|VG|VK|VR|VS|W|WA|WB|WE|WF|WI|WK|WL|WM|WN|WO|WR|WS|WT|W\u00dc|WW|WZ|Z|ZE|ZI|ZP|ZR|ZW|ZZ)[- ]?[A-Z]{1,2}[- ]?\d{1,4}|(ABG|ABI|AIB|AIC|ALF|ALZ|ANA|ANG|ANK|APD|ARN|ART|ASL|ASZ|AUR|AZE|BAD|BAR|BBG|BCH|BED|BER|BGD|BGL|BID|BIN|BIR|BIT|BIW|BKS|BLB|BLK|BNA|BOG|BOH|BOR|BOT|BRA|BRB|BRG|BRK|BRL|BRV|BSB|BSK|BTF|B\u00dcD|BUL|B\u00dcR|B\u00dcS|B\u00dcZ|CAS|CHA|CLP|CLZ|COC|COE|CUX|DAH|DAN|DAU|DBR|DEG|DEL|DGF|DIL|DIN|DIZ|DKB|DLG|DON|DUD|D\u00dcW|EBE|EBN|EBS|ECK|EIC|EIL|EIN|EIS|EMD|EMS|ERB|ERH|ERK|ERZ|ESB|ESW|FDB|FDS|FEU|FFB|FKB|FL\u00d6|FOR|FRG|FRI|FRW|FTL|F\u00dcS|GAN|GAP|GDB|GEL|GEO|GER|GHA|GHC|GLA|GMN|GNT|GOA|GOH|GRA|GRH|GRI|GRM|GRZ|GTH|GUB|GUN|GVM|HAB|HAL|HAM|HAS|HBN|HBS|HCH|HDH|HDL|HEB|HEF|HEI|HER|HET|HGN|HGW|HHM|HIG|HIP|HM\u00dc|HOG|HOH|HOL|HOM|HOR|H\u00d6S|HOT|HRO|HSK|HST|HVL|HWI|IGB|ILL|J\u00dcL|KEH|KEL|KEM|KIB|KLE|KLZ|K\u00d6N|K\u00d6T|K\u00d6Z|KRU|K\u00dcN|KUS|KYF|LAN|LAU|LBS|LBZ|LDK|LDS|LEO|LER|LEV|LIB|LIF|LIP|L\u00d6B|LOS|LRO|LSZ|L\u00dcN|LUP|LWL|MAB|MAI|MAK|MAL|MED|MEG|MEI|MEK|MEL|MER|MET|MGH|MGN|MHL|MIL|MKK|MOD|MOL|MON|MOS|MSE|MSH|MSP|MST|MTK|MTL|M\u00dcB|M\u00dcR|MYK|MZG|NAB|NAI|NAU|NDH|NEA|NEB|NEC|NEN|NES|NEW|NMB|NMS|NOH|NOL|NOM|NOR|NVP|NWM|OAL|OBB|OBG|OCH|OHA|\u00d6HR|OHV|OHZ|OPR|OSL|OVI|OVL|OVP|PAF|PAN|PAR|PCH|PEG|PIR|PL\u00d6|PR\u00dc|QFT|QLB|RDG|REG|REH|REI|RID|RIE|ROD|ROF|ROK|ROL|ROS|ROT|ROW|RSL|R\u00dcD|R\u00dcG|SAB|SAD|SAN|SAW|SBG|SBK|SCZ|SDH|SDL|SDT|SEB|SEE|SEF|SEL|SFB|SFT|SGH|SHA|SHG|SHK|SHL|SIG|SIM|SLE|SLF|SLK|SLN|SLS|SL\u00dc|SLZ|SM\u00dc|SOB|SOG|SOK|S\u00d6M|SON|SPB|SPN|SRB|SRO|STA|STB|STD|STE|STL|SUL|S\u00dcW|SWA|SZB|TBB|TDO|TET|TIR|T\u00d6L|TUT|UEM|UER|UFF|USI|VAI|VEC|VER|VIB|VIE|VIT|VOH|WAF|WAK|WAN|WAR|WAT|WBS|WDA|WEL|WEN|WER|WES|WHV|WIL|WIS|WIT|WIZ|WLG|WMS|WND|WOB|WOH|WOL|WOR|WOS|WRN|WSF|WST|WSW|WTL|WTM|WUG|W\u00dcM|WUN|WUR|WZL|ZEL|ZIG)[- ]?(([A-Z][- ]?\d{1,4})|([A-Z]{2}[- ]?\d{1,3})))[- ]?(E|H)?$/.test(str);
  },
  'de-LI': function deLI(str) {
    return /^FL[- ]?\d{1,5}[UZ]?$/.test(str);
  },
  'en-IN': function enIN(str) {
    return /^[A-Z]{2}[ -]?[0-9]{1,2}(?:[ -]?[A-Z])(?:[ -]?[A-Z]*)?[ -]?[0-9]{4}$/.test(str);
  },
  'en-SG': function enSG(str) {
    return /^[A-Z]{3}[ -]?[\d]{4}[ -]?[A-Z]{1}$/.test(str);
  },
  'es-AR': function esAR(str) {
    return /^(([A-Z]{2} ?[0-9]{3} ?[A-Z]{2})|([A-Z]{3} ?[0-9]{3}))$/.test(str);
  },
  'fi-FI': function fiFI(str) {
    return /^(?=.{4,7})(([A-Z]{1,3}|[0-9]{1,3})[\s-]?([A-Z]{1,3}|[0-9]{1,5}))$/.test(str);
  },
  'hu-HU': function huHU(str) {
    return /^((((?!AAA)(([A-NPRSTVZWXY]{1})([A-PR-Z]{1})([A-HJ-NPR-Z]))|(A[ABC]I)|A[ABC]O|A[A-W]Q|BPI|BPO|UCO|UDO|XAO)-(?!000)\d{3})|(M\d{6})|((CK|DT|CD|HC|H[ABEFIKLMNPRSTVX]|MA|OT|R[A-Z]) \d{2}-\d{2})|(CD \d{3}-\d{3})|(C-(C|X) \d{4})|(X-(A|B|C) \d{4})|(([EPVZ]-\d{5}))|(S A[A-Z]{2} \d{2})|(SP \d{2}-\d{2}))$/.test(str);
  },
  'pt-BR': function ptBR(str) {
    return /^[A-Z]{3}[ -]?[0-9][A-Z][0-9]{2}|[A-Z]{3}[ -]?[0-9]{4}$/.test(str);
  },
  'pt-PT': function ptPT(str) {
    return /^(([A-Z]{2}[ -\u00b7]?[0-9]{2}[ -\u00b7]?[0-9]{2})|([0-9]{2}[ -\u00b7]?[A-Z]{2}[ -\u00b7]?[0-9]{2})|([0-9]{2}[ -\u00b7]?[0-9]{2}[ -\u00b7]?[A-Z]{2})|([A-Z]{2}[ -\u00b7]?[0-9]{2}[ -\u00b7]?[A-Z]{2}))$/.test(str);
  },
  'sq-AL': function sqAL(str) {
    return /^[A-Z]{2}[- ]?((\d{3}[- ]?(([A-Z]{2})|T))|(R[- ]?\d{3}))$/.test(str);
  },
  'sv-SE': function svSE(str) {
    return /^[A-HJ-PR-UW-Z]{3} ?[\d]{2}[A-HJ-PR-UW-Z1-9]$|(^[A-Z\u00c5\u00c4\u00d6 ]{2,7}$)/.test(str.trim());
  },
  'en-PK': function enPK(str) {
    return /(^[A-Z]{2}((\s|-){0,1})[0-9]{3,4}((\s|-)[0-9]{2}){0,1}$)|(^[A-Z]{3}((\s|-){0,1})[0-9]{3,4}((\s|-)[0-9]{2}){0,1}$)|(^[A-Z]{4}((\s|-){0,1})[0-9]{3,4}((\s|-)[0-9]{2}){0,1}$)|(^[A-Z]((\s|-){0,1})[0-9]{4}((\s|-)[0-9]{2}){0,1}$)/.test(str.trim());
  }
};
function isLicensePlate(str, locale) {
  (0, _assertString.default)(str);
  if (locale in validators) {
    return validators[locale](str);
  } else if (locale === 'any') {
    for (var key in validators) {
      /* eslint guard-for-in: 0 */
      var validator = validators[key];
      if (validator(str)) {
        return true;
      }
    }
    return false;
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 700:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = equals;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function equals(str, comparison) {
  (0, _assertString.default)(str);
  return str === comparison;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 821:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isNullOrUndefined;
function isNullOrUndefined(value) {
  return value === null || value === undefined;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 855:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = toInt;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function toInt(str, radix) {
  (0, _assertString.default)(str);
  return parseInt(str, radix || 10);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 995:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isFloat;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _nullUndefinedCheck = _interopRequireDefault(__webpack_require__(821));
var _alpha = __webpack_require__(3237);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isFloat(str, options) {
  (0, _assertString.default)(str);
  options = options || {};
  var float = new RegExp("^(?:[-+])?(?:[0-9]+)?(?:\\".concat(options.locale ? _alpha.decimal[options.locale] : '.', "[0-9]*)?(?:[eE][\\+\\-]?(?:[0-9]+))?$"));
  if (str === '' || str === '.' || str === ',' || str === '-' || str === '+') {
    return false;
  }
  var value = parseFloat(str.replace(',', '.'));
  return float.test(str) && (!options.hasOwnProperty('min') || (0, _nullUndefinedCheck.default)(options.min) || value >= options.min) && (!options.hasOwnProperty('max') || (0, _nullUndefinedCheck.default)(options.max) || value <= options.max) && (!options.hasOwnProperty('lt') || (0, _nullUndefinedCheck.default)(options.lt) || value < options.lt) && (!options.hasOwnProperty('gt') || (0, _nullUndefinedCheck.default)(options.gt) || value > options.gt);
}
var locales = exports.locales = Object.keys(_alpha.decimal);

/***/ }),

/***/ 1062:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isCreditCard;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isLuhnNumber = _interopRequireDefault(__webpack_require__(3609));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var cards = {
  amex: /^3[47][0-9]{13}$/,
  dinersclub: /^3(?:0[0-5]|[68][0-9])[0-9]{11}$/,
  discover: /^6(?:011|5[0-9][0-9])[0-9]{12,15}$/,
  jcb: /^(?:2131|1800|35\d{3})\d{11}$/,
  mastercard: /^5[1-5][0-9]{2}|(222[1-9]|22[3-9][0-9]|2[3-6][0-9]{2}|27[01][0-9]|2720)[0-9]{12}$/,
  // /^[25][1-7][0-9]{14}$/;
  unionpay: /^(6[27][0-9]{14}|^(81[0-9]{14,17}))$/,
  visa: /^(?:4[0-9]{12})(?:[0-9]{3,6})?$/
};
var allCards = function () {
  var tmpCardsArray = [];
  for (var cardProvider in cards) {
    // istanbul ignore else
    if (cards.hasOwnProperty(cardProvider)) {
      tmpCardsArray.push(cards[cardProvider]);
    }
  }
  return tmpCardsArray;
}();
function isCreditCard(card) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  (0, _assertString.default)(card);
  var provider = options.provider;
  var sanitized = card.replace(/[- ]+/g, '');
  if (provider && provider.toLowerCase() in cards) {
    // specific provider in the list
    if (!cards[provider.toLowerCase()].test(sanitized)) {
      return false;
    }
  } else if (provider && !(provider.toLowerCase() in cards)) {
    /* specific provider not in the list */
    throw new Error("".concat(provider, " is not a valid credit card provider."));
  } else if (!allCards.some(function (cardProvider) {
    return cardProvider.test(sanitized);
  })) {
    // no specific provider
    return false;
  }
  return (0, _isLuhnNumber.default)(card);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1128:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = normalizeEmail;
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var default_normalize_email_options = {
  // The following options apply to all email addresses
  // Lowercases the local part of the email address.
  // Please note this may violate RFC 5321 as per http://stackoverflow.com/a/9808332/192024).
  // The domain is always lowercased, as per RFC 1035
  all_lowercase: true,
  // The following conversions are specific to GMail
  // Lowercases the local part of the GMail address (known to be case-insensitive)
  gmail_lowercase: true,
  // Removes dots from the local part of the email address, as that's ignored by GMail
  gmail_remove_dots: true,
  // Removes the subaddress (e.g. "+foo") from the email address
  gmail_remove_subaddress: true,
  // Conversts the googlemail.com domain to gmail.com
  gmail_convert_googlemaildotcom: true,
  // The following conversions are specific to Outlook.com / Windows Live / Hotmail
  // Lowercases the local part of the Outlook.com address (known to be case-insensitive)
  outlookdotcom_lowercase: true,
  // Removes the subaddress (e.g. "+foo") from the email address
  outlookdotcom_remove_subaddress: true,
  // The following conversions are specific to Yahoo
  // Lowercases the local part of the Yahoo address (known to be case-insensitive)
  yahoo_lowercase: true,
  // Removes the subaddress (e.g. "-foo") from the email address
  yahoo_remove_subaddress: true,
  // The following conversions are specific to Yandex
  // Lowercases the local part of the Yandex address (known to be case-insensitive)
  yandex_lowercase: true,
  // all yandex domains are equal, this explicitly sets the domain to 'yandex.ru'
  yandex_convert_yandexru: true,
  // The following conversions are specific to iCloud
  // Lowercases the local part of the iCloud address (known to be case-insensitive)
  icloud_lowercase: true,
  // Removes the subaddress (e.g. "+foo") from the email address
  icloud_remove_subaddress: true
};

// List of domains used by iCloud
var icloud_domains = ['icloud.com', 'me.com'];

// List of domains used by Outlook.com and its predecessors
// This list is likely incomplete.
// Partial reference:
// https://blogs.office.com/2013/04/17/outlook-com-gets-two-step-verification-sign-in-by-alias-and-new-international-domains/
var outlookdotcom_domains = ['hotmail.at', 'hotmail.be', 'hotmail.ca', 'hotmail.cl', 'hotmail.co.il', 'hotmail.co.nz', 'hotmail.co.th', 'hotmail.co.uk', 'hotmail.com', 'hotmail.com.ar', 'hotmail.com.au', 'hotmail.com.br', 'hotmail.com.gr', 'hotmail.com.mx', 'hotmail.com.pe', 'hotmail.com.tr', 'hotmail.com.vn', 'hotmail.cz', 'hotmail.de', 'hotmail.dk', 'hotmail.es', 'hotmail.fr', 'hotmail.hu', 'hotmail.id', 'hotmail.ie', 'hotmail.in', 'hotmail.it', 'hotmail.jp', 'hotmail.kr', 'hotmail.lv', 'hotmail.my', 'hotmail.ph', 'hotmail.pt', 'hotmail.sa', 'hotmail.sg', 'hotmail.sk', 'live.be', 'live.co.uk', 'live.com', 'live.com.ar', 'live.com.mx', 'live.de', 'live.es', 'live.eu', 'live.fr', 'live.it', 'live.nl', 'msn.com', 'outlook.at', 'outlook.be', 'outlook.cl', 'outlook.co.il', 'outlook.co.nz', 'outlook.co.th', 'outlook.com', 'outlook.com.ar', 'outlook.com.au', 'outlook.com.br', 'outlook.com.gr', 'outlook.com.pe', 'outlook.com.tr', 'outlook.com.vn', 'outlook.cz', 'outlook.de', 'outlook.dk', 'outlook.es', 'outlook.fr', 'outlook.hu', 'outlook.id', 'outlook.ie', 'outlook.in', 'outlook.it', 'outlook.jp', 'outlook.kr', 'outlook.lv', 'outlook.my', 'outlook.ph', 'outlook.pt', 'outlook.sa', 'outlook.sg', 'outlook.sk', 'passport.com'];

// List of domains used by Yahoo Mail
// This list is likely incomplete
var yahoo_domains = ['rocketmail.com', 'yahoo.ca', 'yahoo.co.uk', 'yahoo.com', 'yahoo.de', 'yahoo.fr', 'yahoo.in', 'yahoo.it', 'ymail.com'];

// List of domains used by yandex.ru
var yandex_domains = ['yandex.ru', 'yandex.ua', 'yandex.kz', 'yandex.com', 'yandex.by', 'ya.ru'];

// replace single dots, but not multiple consecutive dots
function dotsReplacer(match) {
  if (match.length > 1) {
    return match;
  }
  return '';
}
function normalizeEmail(email, options) {
  options = (0, _merge.default)(options, default_normalize_email_options);
  var raw_parts = email.split('@');
  var domain = raw_parts.pop();
  var user = raw_parts.join('@');
  var parts = [user, domain];

  // The domain is always lowercased, as it's case-insensitive per RFC 1035
  parts[1] = parts[1].toLowerCase();
  if (parts[1] === 'gmail.com' || parts[1] === 'googlemail.com') {
    // Address is GMail
    if (options.gmail_remove_subaddress) {
      parts[0] = parts[0].split('+')[0];
    }
    if (options.gmail_remove_dots) {
      // this does not replace consecutive dots like example..email@gmail.com
      parts[0] = parts[0].replace(/\.+/g, dotsReplacer);
    }
    if (!parts[0].length) {
      return false;
    }
    if (options.all_lowercase || options.gmail_lowercase) {
      parts[0] = parts[0].toLowerCase();
    }
    parts[1] = options.gmail_convert_googlemaildotcom ? 'gmail.com' : parts[1];
  } else if (icloud_domains.indexOf(parts[1]) >= 0) {
    // Address is iCloud
    if (options.icloud_remove_subaddress) {
      parts[0] = parts[0].split('+')[0];
    }
    if (!parts[0].length) {
      return false;
    }
    if (options.all_lowercase || options.icloud_lowercase) {
      parts[0] = parts[0].toLowerCase();
    }
  } else if (outlookdotcom_domains.indexOf(parts[1]) >= 0) {
    // Address is Outlook.com
    if (options.outlookdotcom_remove_subaddress) {
      parts[0] = parts[0].split('+')[0];
    }
    if (!parts[0].length) {
      return false;
    }
    if (options.all_lowercase || options.outlookdotcom_lowercase) {
      parts[0] = parts[0].toLowerCase();
    }
  } else if (yahoo_domains.indexOf(parts[1]) >= 0) {
    // Address is Yahoo
    if (options.yahoo_remove_subaddress) {
      var components = parts[0].split('-');
      parts[0] = components.length > 1 ? components.slice(0, -1).join('-') : components[0];
    }
    if (!parts[0].length) {
      return false;
    }
    if (options.all_lowercase || options.yahoo_lowercase) {
      parts[0] = parts[0].toLowerCase();
    }
  } else if (yandex_domains.indexOf(parts[1]) >= 0) {
    if (options.all_lowercase || options.yandex_lowercase) {
      parts[0] = parts[0].toLowerCase();
    }
    parts[1] = options.yandex_convert_yandexru ? 'yandex.ru' : parts[1];
  } else if (options.all_lowercase) {
    // Any other address
    parts[0] = parts[0].toLowerCase();
  }
  return parts.join('@');
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1195:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isAfter;
var _toDate = _interopRequireDefault(__webpack_require__(3752));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function isAfter(date, options) {
  // For backwards compatibility:
  // isAfter(str [, date]), i.e. `options` could be used as argument for the legacy `date`
  var comparisonDate = (_typeof(options) === 'object' ? options.comparisonDate : options) || Date().toString();
  var comparison = (0, _toDate.default)(comparisonDate);
  var original = (0, _toDate.default)(date);
  return !!(original && comparison && original > comparison);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1252:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMongoId;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isHexadecimal = _interopRequireDefault(__webpack_require__(2002));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isMongoId(str) {
  (0, _assertString.default)(str);
  return (0, _isHexadecimal.default)(str) && str.length === 24;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1371:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = toFloat;
var _isFloat = _interopRequireDefault(__webpack_require__(995));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function toFloat(str) {
  if (!(0, _isFloat.default)(str)) return NaN;
  return parseFloat(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1449:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isVariableWidth;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isFullWidth = __webpack_require__(9666);
var _isHalfWidth = __webpack_require__(9534);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isVariableWidth(str) {
  (0, _assertString.default)(str);
  return _isFullWidth.fullWidth.test(str) && _isHalfWidth.halfWidth.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1572:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = checkHost;
function isRegExp(obj) {
  return Object.prototype.toString.call(obj) === '[object RegExp]';
}
function checkHost(host, matches) {
  for (var i = 0; i < matches.length; i++) {
    var match = matches[i];
    if (host === match || isRegExp(match) && match.test(host)) {
      return true;
    }
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1578:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isRFC3339;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/* Based on https://tools.ietf.org/html/rfc3339#section-5.6 */

var dateFullYear = /[0-9]{4}/;
var dateMonth = /(0[1-9]|1[0-2])/;
var dateMDay = /([12]\d|0[1-9]|3[01])/;
var timeHour = /([01][0-9]|2[0-3])/;
var timeMinute = /[0-5][0-9]/;
var timeSecond = /([0-5][0-9]|60)/;
var timeSecFrac = /(\.[0-9]+)?/;
var timeNumOffset = new RegExp("[-+]".concat(timeHour.source, ":").concat(timeMinute.source));
var timeOffset = new RegExp("([zZ]|".concat(timeNumOffset.source, ")"));
var partialTime = new RegExp("".concat(timeHour.source, ":").concat(timeMinute.source, ":").concat(timeSecond.source).concat(timeSecFrac.source));
var fullDate = new RegExp("".concat(dateFullYear.source, "-").concat(dateMonth.source, "-").concat(dateMDay.source));
var fullTime = new RegExp("".concat(partialTime.source).concat(timeOffset.source));
var rfc3339 = new RegExp("^".concat(fullDate.source, "[ tT]").concat(fullTime.source, "$"));
function isRFC3339(str) {
  (0, _assertString.default)(str);
  return rfc3339.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1666:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isNumeric;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _alpha = __webpack_require__(3237);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var numericNoSymbols = /^[0-9]+$/;
function isNumeric(str, options) {
  (0, _assertString.default)(str);
  if (options && options.no_symbols) {
    return numericNoSymbols.test(str);
  }
  return new RegExp("^[+-]?([0-9]*[".concat((options || {}).locale ? _alpha.decimal[options.locale] : '.', "])?[0-9]+$")).test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1697:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBoolean;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _includesArray = _interopRequireDefault(__webpack_require__(8644));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var defaultOptions = {
  loose: false
};
var strictBooleans = ['true', 'false', '1', '0'];
var looseBooleans = [].concat(strictBooleans, ['yes', 'no']);
function isBoolean(str) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : defaultOptions;
  (0, _assertString.default)(str);
  if (options.loose) {
    return (0, _includesArray.default)(looseBooleans, str.toLowerCase());
  }
  return (0, _includesArray.default)(strictBooleans, str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1954:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISRC;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// see http://isrc.ifpi.org/en/isrc-standard/code-syntax
var isrc = /^[A-Z]{2}[0-9A-Z]{3}\d{2}\d{5}$/;
function isISRC(str) {
  (0, _assertString.default)(str);
  return isrc.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 1996:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = whitelist;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function whitelist(str, chars) {
  (0, _assertString.default)(str);
  return str.replace(new RegExp("[^".concat(chars, "]+"), 'g'), '');
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2002:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isHexadecimal;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var hexadecimal = /^(0x|0h)?[0-9A-F]+$/i;
function isHexadecimal(str) {
  (0, _assertString.default)(str);
  return hexadecimal.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2056:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isEmpty;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var default_is_empty_options = {
  ignore_whitespace: false
};
function isEmpty(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, default_is_empty_options);
  return (options.ignore_whitespace ? str.trim().length : str.length) === 0;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2309:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = ltrim;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function ltrim(str, chars) {
  (0, _assertString.default)(str);
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions#Escaping
  var pattern = chars ? new RegExp("^[".concat(chars.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), "]+"), 'g') : /^\s+/g;
  return str.replace(pattern, '');
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2337:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMailtoURI;
var _trim = _interopRequireDefault(__webpack_require__(317));
var _isEmail = _interopRequireDefault(__webpack_require__(9517));
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
function parseMailtoQueryString(queryString) {
  var allowedParams = new Set(['subject', 'body', 'cc', 'bcc']),
    query = {
      cc: '',
      bcc: ''
    };
  var isParseFailed = false;
  var queryParams = queryString.split('&');
  if (queryParams.length > 4) {
    return false;
  }
  var _iterator = _createForOfIteratorHelper(queryParams),
    _step;
  try {
    for (_iterator.s(); !(_step = _iterator.n()).done;) {
      var q = _step.value;
      var _q$split = q.split('='),
        _q$split2 = _slicedToArray(_q$split, 2),
        key = _q$split2[0],
        value = _q$split2[1];

      // checked for invalid and duplicated query params
      if (key && !allowedParams.has(key)) {
        isParseFailed = true;
        break;
      }
      if (value && (key === 'cc' || key === 'bcc')) {
        query[key] = value;
      }
      if (key) {
        allowedParams.delete(key);
      }
    }
  } catch (err) {
    _iterator.e(err);
  } finally {
    _iterator.f();
  }
  return isParseFailed ? false : query;
}
function isMailtoURI(url, options) {
  (0, _assertString.default)(url);
  if (url.indexOf('mailto:') !== 0) {
    return false;
  }
  var _url$replace$split = url.replace('mailto:', '').split('?'),
    _url$replace$split2 = _slicedToArray(_url$replace$split, 2),
    to = _url$replace$split2[0],
    _url$replace$split2$ = _url$replace$split2[1],
    queryString = _url$replace$split2$ === void 0 ? '' : _url$replace$split2$;
  if (!to && !queryString) {
    return true;
  }
  var query = parseMailtoQueryString(queryString);
  if (!query) {
    return false;
  }
  return "".concat(to, ",").concat(query.cc, ",").concat(query.bcc).split(',').every(function (email) {
    email = (0, _trim.default)(email, ' ');
    if (email) {
      return (0, _isEmail.default)(email, options);
    }
    return true;
  });
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2483:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = rtrim;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function rtrim(str, chars) {
  (0, _assertString.default)(str);
  if (chars) {
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions#Escaping
    var pattern = new RegExp("[".concat(chars.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), "]+$"), 'g');
    return str.replace(pattern, '');
  }
  // Use a faster and more safe than regex trim method https://blog.stevenlevithan.com/archives/faster-trim-javascript
  var strIndex = str.length - 1;
  while (/\s/.test(str.charAt(strIndex))) {
    strIndex -= 1;
  }
  return str.slice(0, strIndex + 1);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2576:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = matches;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function matches(str, pattern, modifiers) {
  (0, _assertString.default)(str);
  if (Object.prototype.toString.call(pattern) !== '[object RegExp]') {
    pattern = new RegExp(pattern, modifiers);
  }
  return !!str.match(pattern);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2645:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIdentityCard;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _includesArray = _interopRequireDefault(__webpack_require__(8644));
var _isInt = _interopRequireDefault(__webpack_require__(6084));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var validators = {
  PL: function PL(str) {
    (0, _assertString.default)(str);
    var weightOfDigits = {
      1: 1,
      2: 3,
      3: 7,
      4: 9,
      5: 1,
      6: 3,
      7: 7,
      8: 9,
      9: 1,
      10: 3,
      11: 0
    };
    if (str != null && str.length === 11 && (0, _isInt.default)(str, {
      allow_leading_zeroes: true
    })) {
      var digits = str.split('').slice(0, -1);
      var sum = digits.reduce(function (acc, digit, index) {
        return acc + Number(digit) * weightOfDigits[index + 1];
      }, 0);
      var modulo = sum % 10;
      var lastDigit = Number(str.charAt(str.length - 1));
      if (modulo === 0 && lastDigit === 0 || lastDigit === 10 - modulo) {
        return true;
      }
    }
    return false;
  },
  ES: function ES(str) {
    (0, _assertString.default)(str);
    var DNI = /^[0-9X-Z][0-9]{7}[TRWAGMYFPDXBNJZSQVHLCKE]$/;
    var charsValue = {
      X: 0,
      Y: 1,
      Z: 2
    };
    var controlDigits = ['T', 'R', 'W', 'A', 'G', 'M', 'Y', 'F', 'P', 'D', 'X', 'B', 'N', 'J', 'Z', 'S', 'Q', 'V', 'H', 'L', 'C', 'K', 'E'];

    // sanitize user input
    var sanitized = str.trim().toUpperCase();

    // validate the data structure
    if (!DNI.test(sanitized)) {
      return false;
    }

    // validate the control digit
    var number = sanitized.slice(0, -1).replace(/[X,Y,Z]/g, function (char) {
      return charsValue[char];
    });
    return sanitized.endsWith(controlDigits[number % 23]);
  },
  FI: function FI(str) {
    // https://dvv.fi/en/personal-identity-code#:~:text=control%20character%20for%20a-,personal,-identity%20code%20calculated
    (0, _assertString.default)(str);
    if (str.length !== 11) {
      return false;
    }
    if (!str.match(/^\d{6}[\-A\+]\d{3}[0-9ABCDEFHJKLMNPRSTUVWXY]{1}$/)) {
      return false;
    }
    var checkDigits = '0123456789ABCDEFHJKLMNPRSTUVWXY';
    var idAsNumber = parseInt(str.slice(0, 6), 10) * 1000 + parseInt(str.slice(7, 10), 10);
    var remainder = idAsNumber % 31;
    var checkDigit = checkDigits[remainder];
    return checkDigit === str.slice(10, 11);
  },
  IN: function IN(str) {
    var DNI = /^[1-9]\d{3}\s?\d{4}\s?\d{4}$/;

    // multiplication table
    var d = [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 4, 0, 6, 7, 8, 9, 5], [2, 3, 4, 0, 1, 7, 8, 9, 5, 6], [3, 4, 0, 1, 2, 8, 9, 5, 6, 7], [4, 0, 1, 2, 3, 9, 5, 6, 7, 8], [5, 9, 8, 7, 6, 0, 4, 3, 2, 1], [6, 5, 9, 8, 7, 1, 0, 4, 3, 2], [7, 6, 5, 9, 8, 2, 1, 0, 4, 3], [8, 7, 6, 5, 9, 3, 2, 1, 0, 4], [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]];

    // permutation table
    var p = [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 5, 7, 6, 2, 8, 3, 0, 9, 4], [5, 8, 0, 3, 7, 9, 6, 1, 4, 2], [8, 9, 1, 6, 0, 4, 3, 5, 2, 7], [9, 4, 5, 3, 1, 2, 6, 8, 7, 0], [4, 2, 8, 6, 5, 7, 3, 9, 0, 1], [2, 7, 9, 3, 8, 0, 6, 4, 1, 5], [7, 0, 4, 6, 9, 1, 3, 2, 5, 8]];

    // sanitize user input
    var sanitized = str.trim();

    // validate the data structure
    if (!DNI.test(sanitized)) {
      return false;
    }
    var c = 0;
    var invertedArray = sanitized.replace(/\s/g, '').split('').map(Number).reverse();
    invertedArray.forEach(function (val, i) {
      c = d[c][p[i % 8][val]];
    });
    return c === 0;
  },
  IR: function IR(str) {
    if (!str.match(/^\d{10}$/)) return false;
    str = "0000".concat(str).slice(str.length - 6);
    if (parseInt(str.slice(3, 9), 10) === 0) return false;
    var lastNumber = parseInt(str.slice(9, 10), 10);
    var sum = 0;
    for (var i = 0; i < 9; i++) {
      sum += parseInt(str.slice(i, i + 1), 10) * (10 - i);
    }
    sum %= 11;
    return sum < 2 && lastNumber === sum || sum >= 2 && lastNumber === 11 - sum;
  },
  IT: function IT(str) {
    if (str.length !== 9) return false;
    if (str === 'CA00000AA') return false; // https://it.wikipedia.org/wiki/Carta_d%27identit%C3%A0_elettronica_italiana
    return str.search(/C[A-Z]\d{5}[A-Z]{2}/i) > -1;
  },
  NO: function NO(str) {
    var sanitized = str.trim();
    if (isNaN(Number(sanitized))) return false;
    if (sanitized.length !== 11) return false;
    if (sanitized === '00000000000') return false;

    // https://no.wikipedia.org/wiki/F%C3%B8dselsnummer
    var f = sanitized.split('').map(Number);
    var k1 = (11 - (3 * f[0] + 7 * f[1] + 6 * f[2] + 1 * f[3] + 8 * f[4] + 9 * f[5] + 4 * f[6] + 5 * f[7] + 2 * f[8]) % 11) % 11;
    var k2 = (11 - (5 * f[0] + 4 * f[1] + 3 * f[2] + 2 * f[3] + 7 * f[4] + 6 * f[5] + 5 * f[6] + 4 * f[7] + 3 * f[8] + 2 * k1) % 11) % 11;
    if (k1 !== f[9] || k2 !== f[10]) return false;
    return true;
  },
  TH: function TH(str) {
    if (!str.match(/^[1-8]\d{12}$/)) return false;

    // validate check digit
    var sum = 0;
    for (var i = 0; i < 12; i++) {
      sum += parseInt(str[i], 10) * (13 - i);
    }
    return str[12] === ((11 - sum % 11) % 10).toString();
  },
  LK: function LK(str) {
    var old_nic = /^[1-9]\d{8}[vx]$/i;
    var new_nic = /^[1-9]\d{11}$/i;
    if (str.length === 10 && old_nic.test(str)) return true;else if (str.length === 12 && new_nic.test(str)) return true;
    return false;
  },
  'he-IL': function heIL(str) {
    var DNI = /^\d{9}$/;

    // sanitize user input
    var sanitized = str.trim();

    // validate the data structure
    if (!DNI.test(sanitized)) {
      return false;
    }
    var id = sanitized;
    var sum = 0,
      incNum;
    for (var i = 0; i < id.length; i++) {
      incNum = Number(id[i]) * (i % 2 + 1); // Multiply number by 1 or 2
      sum += incNum > 9 ? incNum - 9 : incNum; // Sum the digits up and add to total
    }
    return sum % 10 === 0;
  },
  'ar-LY': function arLY(str) {
    // Libya National Identity Number NIN is 12 digits, the first digit is either 1 or 2
    var NIN = /^(1|2)\d{11}$/;

    // sanitize user input
    var sanitized = str.trim();

    // validate the data structure
    if (!NIN.test(sanitized)) {
      return false;
    }
    return true;
  },
  'ar-TN': function arTN(str) {
    var DNI = /^\d{8}$/;

    // sanitize user input
    var sanitized = str.trim();

    // validate the data structure
    if (!DNI.test(sanitized)) {
      return false;
    }
    return true;
  },
  'zh-CN': function zhCN(str) {
    var provincesAndCities = ['11',
    // \u5317\u4eac
    '12',
    // \u5929\u6d25
    '13',
    // \u6cb3\u5317
    '14',
    // \u5c71\u897f
    '15',
    // \u5185\u8499\u53e4
    '21',
    // \u8fbd\u5b81
    '22',
    // \u5409\u6797
    '23',
    // \u9ed1\u9f99\u6c5f
    '31',
    // \u4e0a\u6d77
    '32',
    // \u6c5f\u82cf
    '33',
    // \u6d59\u6c5f
    '34',
    // \u5b89\u5fbd
    '35',
    // \u798f\u5efa
    '36',
    // \u6c5f\u897f
    '37',
    // \u5c71\u4e1c
    '41',
    // \u6cb3\u5357
    '42',
    // \u6e56\u5317
    '43',
    // \u6e56\u5357
    '44',
    // \u5e7f\u4e1c
    '45',
    // \u5e7f\u897f
    '46',
    // \u6d77\u5357
    '50',
    // \u91cd\u5e86
    '51',
    // \u56db\u5ddd
    '52',
    // \u8d35\u5dde
    '53',
    // \u4e91\u5357
    '54',
    // \u897f\u85cf
    '61',
    // \u9655\u897f
    '62',
    // \u7518\u8083
    '63',
    // \u9752\u6d77
    '64',
    // \u5b81\u590f
    '65',
    // \u65b0\u7586
    '71',
    // \u53f0\u6e7e
    '81',
    // \u9999\u6e2f
    '82',
    // \u6fb3\u95e8
    '91' // \u56fd\u5916
    ];
    var powers = ['7', '9', '10', '5', '8', '4', '2', '1', '6', '3', '7', '9', '10', '5', '8', '4', '2'];
    var parityBit = ['1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2'];
    var checkAddressCode = function checkAddressCode(addressCode) {
      return (0, _includesArray.default)(provincesAndCities, addressCode);
    };
    var checkBirthDayCode = function checkBirthDayCode(birDayCode) {
      var yyyy = parseInt(birDayCode.substring(0, 4), 10);
      var mm = parseInt(birDayCode.substring(4, 6), 10);
      var dd = parseInt(birDayCode.substring(6), 10);
      var xdata = new Date(yyyy, mm - 1, dd);
      if (xdata > new Date()) {
        return false;
        // eslint-disable-next-line max-len
      } else if (xdata.getFullYear() === yyyy && xdata.getMonth() === mm - 1 && xdata.getDate() === dd) {
        return true;
      }
      return false;
    };
    var getParityBit = function getParityBit(idCardNo) {
      var id17 = idCardNo.substring(0, 17);
      var power = 0;
      for (var i = 0; i < 17; i++) {
        power += parseInt(id17.charAt(i), 10) * parseInt(powers[i], 10);
      }
      var mod = power % 11;
      return parityBit[mod];
    };
    var checkParityBit = function checkParityBit(idCardNo) {
      return getParityBit(idCardNo) === idCardNo.charAt(17).toUpperCase();
    };
    var check15IdCardNo = function check15IdCardNo(idCardNo) {
      var check = /^[1-9]\d{7}((0[1-9])|(1[0-2]))((0[1-9])|([1-2][0-9])|(3[0-1]))\d{3}$/.test(idCardNo);
      if (!check) return false;
      var addressCode = idCardNo.substring(0, 2);
      check = checkAddressCode(addressCode);
      if (!check) return false;
      var birDayCode = "19".concat(idCardNo.substring(6, 12));
      check = checkBirthDayCode(birDayCode);
      if (!check) return false;
      return true;
    };
    var check18IdCardNo = function check18IdCardNo(idCardNo) {
      var check = /^[1-9]\d{5}[1-9]\d{3}((0[1-9])|(1[0-2]))((0[1-9])|([1-2][0-9])|(3[0-1]))\d{3}(\d|x|X)$/.test(idCardNo);
      if (!check) return false;
      var addressCode = idCardNo.substring(0, 2);
      check = checkAddressCode(addressCode);
      if (!check) return false;
      var birDayCode = idCardNo.substring(6, 14);
      check = checkBirthDayCode(birDayCode);
      if (!check) return false;
      return checkParityBit(idCardNo);
    };
    var checkIdCardNo = function checkIdCardNo(idCardNo) {
      var check = /^\d{15}|(\d{17}(\d|x|X))$/.test(idCardNo);
      if (!check) return false;
      if (idCardNo.length === 15) {
        return check15IdCardNo(idCardNo);
      }
      return check18IdCardNo(idCardNo);
    };
    return checkIdCardNo(str);
  },
  'zh-HK': function zhHK(str) {
    // sanitize user input
    str = str.trim();

    // HKID number starts with 1 or 2 letters, followed by 6 digits,
    // then a checksum contained in square / round brackets or nothing
    var regexHKID = /^[A-Z]{1,2}[0-9]{6}((\([0-9A]\))|(\[[0-9A]\])|([0-9A]))$/;
    var regexIsDigit = /^[0-9]$/;

    // convert the user input to all uppercase and apply regex
    str = str.toUpperCase();
    if (!regexHKID.test(str)) return false;
    str = str.replace(/\[|\]|\(|\)/g, '');
    if (str.length === 8) str = "3".concat(str);
    var checkSumVal = 0;
    for (var i = 0; i <= 7; i++) {
      var convertedChar = void 0;
      if (!regexIsDigit.test(str[i])) convertedChar = (str[i].charCodeAt(0) - 55) % 11;else convertedChar = str[i];
      checkSumVal += convertedChar * (9 - i);
    }
    checkSumVal %= 11;
    var checkSumConverted;
    if (checkSumVal === 0) checkSumConverted = '0';else if (checkSumVal === 1) checkSumConverted = 'A';else checkSumConverted = String(11 - checkSumVal);
    if (checkSumConverted === str[str.length - 1]) return true;
    return false;
  },
  'zh-TW': function zhTW(str) {
    var ALPHABET_CODES = {
      A: 10,
      B: 11,
      C: 12,
      D: 13,
      E: 14,
      F: 15,
      G: 16,
      H: 17,
      I: 34,
      J: 18,
      K: 19,
      L: 20,
      M: 21,
      N: 22,
      O: 35,
      P: 23,
      Q: 24,
      R: 25,
      S: 26,
      T: 27,
      U: 28,
      V: 29,
      W: 32,
      X: 30,
      Y: 31,
      Z: 33
    };
    var sanitized = str.trim().toUpperCase();
    if (!/^[A-Z][0-9]{9}$/.test(sanitized)) return false;
    return Array.from(sanitized).reduce(function (sum, number, index) {
      if (index === 0) {
        var code = ALPHABET_CODES[number];
        return code % 10 * 9 + Math.floor(code / 10);
      }
      if (index === 9) {
        return (10 - sum % 10 - Number(number)) % 10 === 0;
      }
      return sum + Number(number) * (9 - index);
    }, 0);
  },
  PK: function PK(str) {
    // Pakistani National Identity Number CNIC is 13 digits
    var CNIC = /^[1-7][0-9]{4}-[0-9]{7}-[1-9]$/;

    // sanitize user input
    var sanitized = str.trim();

    // validate the data structure
    return CNIC.test(sanitized);
  }
};
function isIdentityCard(str, locale) {
  (0, _assertString.default)(str);
  if (locale in validators) {
    return validators[locale](str);
  } else if (locale === 'any') {
    for (var key in validators) {
      // https://github.com/gotwarlost/istanbul/blob/master/ignoring-code-for-coverage.md#ignoring-code-for-coverage-purposes
      // istanbul ignore else
      if (validators.hasOwnProperty(key)) {
        var validator = validators[key];
        if (validator(str)) {
          return true;
        }
      }
    }
    return false;
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2678:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISIN;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var isin = /^[A-Z]{2}[0-9A-Z]{9}[0-9]$/;

// this link details how the check digit is calculated:
// https://www.isin.org/isin-format/. it is a little bit
// odd in that it works with digits, not numbers. in order
// to make only one pass through the ISIN characters, the
// each alpha character is handled as 2 characters within
// the loop.

function isISIN(str) {
  (0, _assertString.default)(str);
  if (!isin.test(str)) {
    return false;
  }
  var double = true;
  var sum = 0;
  // convert values
  for (var i = str.length - 2; i >= 0; i--) {
    if (str[i] >= 'A' && str[i] <= 'Z') {
      var value = str[i].charCodeAt(0) - 55;
      var lo = value % 10;
      var hi = Math.trunc(value / 10);
      // letters have two digits, so handle the low order
      // and high order digits separately.
      for (var _i = 0, _arr = [lo, hi]; _i < _arr.length; _i++) {
        var digit = _arr[_i];
        if (double) {
          if (digit >= 5) {
            sum += 1 + (digit - 5) * 2;
          } else {
            sum += digit * 2;
          }
        } else {
          sum += digit;
        }
        double = !double;
      }
    } else {
      var _digit = str[i].charCodeAt(0) - '0'.charCodeAt(0);
      if (double) {
        if (_digit >= 5) {
          sum += 1 + (_digit - 5) * 2;
        } else {
          sum += _digit * 2;
        }
      } else {
        sum += _digit;
      }
      double = !double;
    }
  }
  var check = Math.trunc((sum + 9) / 10) * 10 - sum;
  return +str[str.length - 1] === check;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 2830:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMultibyte;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/* eslint-disable no-control-regex */
var multibyte = /[^\x00-\x7F]/;
/* eslint-enable no-control-regex */

function isMultibyte(str) {
  (0, _assertString.default)(str);
  return multibyte.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3158:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISO31661Numeric;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// from https://en.wikipedia.org/wiki/ISO_3166-1_numeric
var validISO31661NumericCountriesCodes = new Set(['004', '008', '010', '012', '016', '020', '024', '028', '031', '032', '036', '040', '044', '048', '050', '051', '052', '056', '060', '064', '068', '070', '072', '074', '076', '084', '086', '090', '092', '096', '100', '104', '108', '112', '116', '120', '124', '132', '136', '140', '144', '148', '152', '156', '158', '162', '166', '170', '174', '175', '178', '180', '184', '188', '191', '192', '196', '203', '204', '208', '212', '214', '218', '222', '226', '231', '232', '233', '234', '238', '239', '242', '246', '248', '250', '254', '258', '260', '262', '266', '268', '270', '275', '276', '288', '292', '296', '300', '304', '308', '312', '316', '320', '324', '328', '332', '334', '336', '340', '344', '348', '352', '356', '360', '364', '368', '372', '376', '380', '384', '388', '392', '398', '400', '404', '408', '410', '414', '417', '418', '422', '426', '428', '430', '434', '438', '440', '442', '446', '450', '454', '458', '462', '466', '470', '474', '478', '480', '484', '492', '496', '498', '499', '500', '504', '508', '512', '516', '520', '524', '528', '531', '533', '534', '535', '540', '548', '554', '558', '562', '566', '570', '574', '578', '580', '581', '583', '584', '585', '586', '591', '598', '600', '604', '608', '612', '616', '620', '624', '626', '630', '634', '638', '642', '643', '646', '652', '654', '659', '660', '662', '663', '666', '670', '674', '678', '682', '686', '688', '690', '694', '702', '703', '704', '705', '706', '710', '716', '724', '728', '729', '732', '740', '744', '748', '752', '756', '760', '762', '764', '768', '772', '776', '780', '784', '788', '792', '795', '796', '798', '800', '804', '807', '818', '826', '831', '832', '833', '834', '840', '850', '854', '858', '860', '862', '876', '882', '887', '894']);
function isISO31661Numeric(str) {
  (0, _assertString.default)(str);
  return validISO31661NumericCountriesCodes.has(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3196:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMACAddress;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var macAddress48 = /^(?:[0-9a-fA-F]{2}([-:\s]))([0-9a-fA-F]{2}\1){4}([0-9a-fA-F]{2})$/;
var macAddress48NoSeparators = /^([0-9a-fA-F]){12}$/;
var macAddress48WithDots = /^([0-9a-fA-F]{4}\.){2}([0-9a-fA-F]{4})$/;
var macAddress64 = /^(?:[0-9a-fA-F]{2}([-:\s]))([0-9a-fA-F]{2}\1){6}([0-9a-fA-F]{2})$/;
var macAddress64NoSeparators = /^([0-9a-fA-F]){16}$/;
var macAddress64WithDots = /^([0-9a-fA-F]{4}\.){3}([0-9a-fA-F]{4})$/;
function isMACAddress(str, options) {
  (0, _assertString.default)(str);
  if (options !== null && options !== void 0 && options.eui) {
    options.eui = String(options.eui);
  }
  /**
   * @deprecated `no_colons` TODO: remove it in the next major
  */
  if (options !== null && options !== void 0 && options.no_colons || options !== null && options !== void 0 && options.no_separators) {
    if (options.eui === '48') {
      return macAddress48NoSeparators.test(str);
    }
    if (options.eui === '64') {
      return macAddress64NoSeparators.test(str);
    }
    return macAddress48NoSeparators.test(str) || macAddress64NoSeparators.test(str);
  }
  if ((options === null || options === void 0 ? void 0 : options.eui) === '48') {
    return macAddress48.test(str) || macAddress48WithDots.test(str);
  }
  if ((options === null || options === void 0 ? void 0 : options.eui) === '64') {
    return macAddress64.test(str) || macAddress64WithDots.test(str);
  }
  return isMACAddress(str, {
    eui: '48'
  }) || isMACAddress(str, {
    eui: '64'
  });
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3214:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = toBoolean;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function toBoolean(str, strict) {
  (0, _assertString.default)(str);
  if (strict) {
    return str === '1' || /^true$/i.test(str);
  }
  return str !== '0' && !/^false$/i.test(str) && str !== '';
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3237:
/***/ ((__unused_webpack_module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.farsiLocales = exports.englishLocales = exports.dotDecimal = exports.decimal = exports.commaDecimal = exports.bengaliLocales = exports.arabicLocales = exports.alphanumeric = exports.alpha = void 0;
var alpha = exports.alpha = {
  'en-US': /^[A-Z]+$/i,
  'az-AZ': /^[A-VXYZ\u00c7\u018f\u011e\u0130\u0131\u00d6\u015e\u00dc]+$/i,
  'bg-BG': /^[\u0410-\u042f]+$/i,
  'cs-CZ': /^[A-Z\u00c1\u010c\u010e\u00c9\u011a\u00cd\u0147\u00d3\u0158\u0160\u0164\u00da\u016e\u00dd\u017d]+$/i,
  'da-DK': /^[A-Z\u00c6\u00d8\u00c5]+$/i,
  'de-DE': /^[A-Z\u00c4\u00d6\u00dc\u00df]+$/i,
  'el-GR': /^[\u0391-\u03ce]+$/i,
  'es-ES': /^[A-Z\u00c1\u00c9\u00cd\u00d1\u00d3\u00da\u00dc]+$/i,
  'fa-IR': /^[\u0627\u0628\u067e\u062a\u062b\u062c\u0686\u062d\u062e\u062f\u0630\u0631\u0632\u0698\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a\u0641\u0642\u06a9\u06af\u0644\u0645\u0646\u0648\u0647\u06cc]+$/i,
  'fi-FI': /^[A-Z\u00c5\u00c4\u00d6]+$/i,
  'fr-FR': /^[A-Z\u00c0\u00c2\u00c6\u00c7\u00c9\u00c8\u00ca\u00cb\u00cf\u00ce\u00d4\u0152\u00d9\u00db\u00dc\u0178]+$/i,
  'it-IT': /^[A-Z\u00c0\u00c9\u00c8\u00cc\u00ce\u00d3\u00d2\u00d9]+$/i,
  'ja-JP': /^[\u3041-\u3093\u30a1-\u30f6\uff66-\uff9f\u4e00-\u9fa0\u30fc\u30fb\u3002\u3001]+$/i,
  'nb-NO': /^[A-Z\u00c6\u00d8\u00c5]+$/i,
  'nl-NL': /^[A-Z\u00c1\u00c9\u00cb\u00cf\u00d3\u00d6\u00dc\u00da]+$/i,
  'nn-NO': /^[A-Z\u00c6\u00d8\u00c5]+$/i,
  'hu-HU': /^[A-Z\u00c1\u00c9\u00cd\u00d3\u00d6\u0150\u00da\u00dc\u0170]+$/i,
  'pl-PL': /^[A-Z\u0104\u0106\u0118\u015a\u0141\u0143\u00d3\u017b\u0179]+$/i,
  'pt-PT': /^[A-Z\u00c3\u00c1\u00c0\u00c2\u00c4\u00c7\u00c9\u00ca\u00cb\u00cd\u00cf\u00d5\u00d3\u00d4\u00d6\u00da\u00dc]+$/i,
  'ru-RU': /^[\u0410-\u042f\u0401]+$/i,
  'kk-KZ': /^[\u0410-\u042f\u0401\u04D8\u04B0\u0406\u04A2\u0492\u04AE\u049A\u04E8\u04BA]+$/i,
  'sl-SI': /^[A-Z\u010c\u0106\u0110\u0160\u017d]+$/i,
  'sk-SK': /^[A-Z\u00c1\u010c\u010e\u00c9\u00cd\u0147\u00d3\u0160\u0164\u00da\u00dd\u017d\u0139\u0154\u013d\u00c4\u00d4]+$/i,
  'sr-RS@latin': /^[A-Z\u010c\u0106\u017d\u0160\u0110]+$/i,
  'sr-RS': /^[\u0410-\u042f\u0402\u0408\u0409\u040a\u040b\u040f]+$/i,
  'sv-SE': /^[A-Z\u00c5\u00c4\u00d6]+$/i,
  'th-TH': /^[\u0e01-\u0e50\s]+$/i,
  'tr-TR': /^[A-Z\u00c7\u011e\u0130\u0131\u00d6\u015e\u00dc]+$/i,
  'uk-UA': /^[\u0410-\u0429\u042c\u042e\u042f\u0404I\u0407\u0490\u0456]+$/i,
  'vi-VN': /^[A-Z\u00c0\u00c1\u1ea0\u1ea2\u00c3\u00c2\u1ea6\u1ea4\u1eac\u1ea8\u1eaa\u0102\u1eb0\u1eae\u1eb6\u1eb2\u1eb4\u0110\u00c8\u00c9\u1eb8\u1eba\u1ebc\u00ca\u1ec0\u1ebe\u1ec6\u1ec2\u1ec4\u00cc\u00cd\u1eca\u1ec8\u0128\u00d2\u00d3\u1ecc\u1ece\u00d5\u00d4\u1ed2\u1ed0\u1ed8\u1ed4\u1ed6\u01a0\u1edc\u1eda\u1ee2\u1ede\u1ee0\u00d9\u00da\u1ee4\u1ee6\u0168\u01af\u1eea\u1ee8\u1ef0\u1eec\u1eee\u1ef2\u00dd\u1ef4\u1ef6\u1ef8]+$/i,
  'ko-KR': /^[\u3131-\u314e\u314f-\u3163\uac00-\ud7a3]*$/,
  'ku-IQ': /^[\u0626\u0627\u0628\u067e\u062a\u062c\u0686\u062d\u062e\u062f\u0631\u0695\u0632\u0698\u0633\u0634\u0639\u063a\u0641\u06a4\u0642\u06a9\u06af\u0644\u06b5\u0645\u0646\u0648\u06c6\u06be\u06d5\u06cc\u06ce\u064a\u0637\u0624\u062b\u0622\u0625\u0623\u0643\u0636\u0635\u0629\u0638\u0630]+$/i,
  ar: /^[\u0621\u0622\u0623\u0624\u0625\u0626\u0627\u0628\u0629\u062a\u062b\u062c\u062d\u062e\u062f\u0630\u0631\u0632\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a\u0641\u0642\u0643\u0644\u0645\u0646\u0647\u0648\u0649\u064a\u064b\u064c\u064d\u064e\u064f\u0650\u0651\u0652\u0670]+$/,
  he: /^[\u05d0-\u05ea]+$/,
  fa: /^['\u0622\u0627\u0621\u0623\u0624\u0626\u0628\u067e\u062a\u062b\u062c\u0686\u062d\u062e\u062f\u0630\u0631\u0632\u0698\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a\u0641\u0642\u06a9\u06af\u0644\u0645\u0646\u0648\u0647\u0629\u06cc']+$/i,
  bn: /^['\u0980\u0981\u0982\u0983\u0985\u0986\u0987\u0988\u0989\u098a\u098b\u098c\u098f\u0990\u0993\u0994\u0995\u0996\u0997\u0998\u0999\u099a\u099b\u099c\u099d\u099e\u099f\u09a0\u09a1\u09a2\u09a3\u09a4\u09a5\u09a6\u09a7\u09a8\u09aa\u09ab\u09ac\u09ad\u09ae\u09af\u09b0\u09b2\u09b6\u09b7\u09b8\u09b9\u09bc\u09bd\u09be\u09bf\u09c0\u09c1\u09c2\u09c3\u09c4\u09c7\u09c8\u09cb\u09cc\u09cd\u09ce\u09d7\u09dc\u09dd\u09df\u09e0\u09e1\u09e2\u09e3\u09f0\u09f1\u09f2\u09f3\u09f4\u09f5\u09f6\u09f7\u09f8\u09f9\u09fa\u09fb']+$/,
  eo: /^[ABC\u0108D-G\u011cH\u0124IJ\u0134K-PRS\u015cTU\u016cVZ]+$/i,
  'hi-IN': /^[\u0900-\u0961]+[\u0972-\u097F]*$/i,
  'si-LK': /^[\u0D80-\u0DFF]+$/
};
var alphanumeric = exports.alphanumeric = {
  'en-US': /^[0-9A-Z]+$/i,
  'az-AZ': /^[0-9A-VXYZ\u00c7\u018f\u011e\u0130\u0131\u00d6\u015e\u00dc]+$/i,
  'bg-BG': /^[0-9\u0410-\u042f]+$/i,
  'cs-CZ': /^[0-9A-Z\u00c1\u010c\u010e\u00c9\u011a\u00cd\u0147\u00d3\u0158\u0160\u0164\u00da\u016e\u00dd\u017d]+$/i,
  'da-DK': /^[0-9A-Z\u00c6\u00d8\u00c5]+$/i,
  'de-DE': /^[0-9A-Z\u00c4\u00d6\u00dc\u00df]+$/i,
  'el-GR': /^[0-9\u0391-\u03c9]+$/i,
  'es-ES': /^[0-9A-Z\u00c1\u00c9\u00cd\u00d1\u00d3\u00da\u00dc]+$/i,
  'fi-FI': /^[0-9A-Z\u00c5\u00c4\u00d6]+$/i,
  'fr-FR': /^[0-9A-Z\u00c0\u00c2\u00c6\u00c7\u00c9\u00c8\u00ca\u00cb\u00cf\u00ce\u00d4\u0152\u00d9\u00db\u00dc\u0178]+$/i,
  'it-IT': /^[0-9A-Z\u00c0\u00c9\u00c8\u00cc\u00ce\u00d3\u00d2\u00d9]+$/i,
  'ja-JP': /^[0-9\uff10-\uff19\u3041-\u3093\u30a1-\u30f6\uff66-\uff9f\u4e00-\u9fa0\u30fc\u30fb\u3002\u3001]+$/i,
  'hu-HU': /^[0-9A-Z\u00c1\u00c9\u00cd\u00d3\u00d6\u0150\u00da\u00dc\u0170]+$/i,
  'nb-NO': /^[0-9A-Z\u00c6\u00d8\u00c5]+$/i,
  'nl-NL': /^[0-9A-Z\u00c1\u00c9\u00cb\u00cf\u00d3\u00d6\u00dc\u00da]+$/i,
  'nn-NO': /^[0-9A-Z\u00c6\u00d8\u00c5]+$/i,
  'pl-PL': /^[0-9A-Z\u0104\u0106\u0118\u015a\u0141\u0143\u00d3\u017b\u0179]+$/i,
  'pt-PT': /^[0-9A-Z\u00c3\u00c1\u00c0\u00c2\u00c4\u00c7\u00c9\u00ca\u00cb\u00cd\u00cf\u00d5\u00d3\u00d4\u00d6\u00da\u00dc]+$/i,
  'ru-RU': /^[0-9\u0410-\u042f\u0401]+$/i,
  'kk-KZ': /^[0-9\u0410-\u042f\u0401\u04D8\u04B0\u0406\u04A2\u0492\u04AE\u049A\u04E8\u04BA]+$/i,
  'sl-SI': /^[0-9A-Z\u010c\u0106\u0110\u0160\u017d]+$/i,
  'sk-SK': /^[0-9A-Z\u00c1\u010c\u010e\u00c9\u00cd\u0147\u00d3\u0160\u0164\u00da\u00dd\u017d\u0139\u0154\u013d\u00c4\u00d4]+$/i,
  'sr-RS@latin': /^[0-9A-Z\u010c\u0106\u017d\u0160\u0110]+$/i,
  'sr-RS': /^[0-9\u0410-\u042f\u0402\u0408\u0409\u040a\u040b\u040f]+$/i,
  'sv-SE': /^[0-9A-Z\u00c5\u00c4\u00d6]+$/i,
  'th-TH': /^[\u0e01-\u0e59\s]+$/i,
  'tr-TR': /^[0-9A-Z\u00c7\u011e\u0130\u0131\u00d6\u015e\u00dc]+$/i,
  'uk-UA': /^[0-9\u0410-\u0429\u042c\u042e\u042f\u0404I\u0407\u0490\u0456]+$/i,
  'ko-KR': /^[0-9\u3131-\u314e\u314f-\u3163\uac00-\ud7a3]*$/,
  'ku-IQ': /^[\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u06690-9\u0626\u0627\u0628\u067e\u062a\u062c\u0686\u062d\u062e\u062f\u0631\u0695\u0632\u0698\u0633\u0634\u0639\u063a\u0641\u06a4\u0642\u06a9\u06af\u0644\u06b5\u0645\u0646\u0648\u06c6\u06be\u06d5\u06cc\u06ce\u064a\u0637\u0624\u062b\u0622\u0625\u0623\u0643\u0636\u0635\u0629\u0638\u0630]+$/i,
  'vi-VN': /^[0-9A-Z\u00c0\u00c1\u1ea0\u1ea2\u00c3\u00c2\u1ea6\u1ea4\u1eac\u1ea8\u1eaa\u0102\u1eb0\u1eae\u1eb6\u1eb2\u1eb4\u0110\u00c8\u00c9\u1eb8\u1eba\u1ebc\u00ca\u1ec0\u1ebe\u1ec6\u1ec2\u1ec4\u00cc\u00cd\u1eca\u1ec8\u0128\u00d2\u00d3\u1ecc\u1ece\u00d5\u00d4\u1ed2\u1ed0\u1ed8\u1ed4\u1ed6\u01a0\u1edc\u1eda\u1ee2\u1ede\u1ee0\u00d9\u00da\u1ee4\u1ee6\u0168\u01af\u1eea\u1ee8\u1ef0\u1eec\u1eee\u1ef2\u00dd\u1ef4\u1ef6\u1ef8]+$/i,
  ar: /^[\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u06690-9\u0621\u0622\u0623\u0624\u0625\u0626\u0627\u0628\u0629\u062a\u062b\u062c\u062d\u062e\u062f\u0630\u0631\u0632\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a\u0641\u0642\u0643\u0644\u0645\u0646\u0647\u0648\u0649\u064a\u064b\u064c\u064d\u064e\u064f\u0650\u0651\u0652\u0670]+$/,
  he: /^[0-9\u05d0-\u05ea]+$/,
  fa: /^['0-9\u0622\u0627\u0621\u0623\u0624\u0626\u0628\u067e\u062a\u062b\u062c\u0686\u062d\u062e\u062f\u0630\u0631\u0632\u0698\u0633\u0634\u0635\u0636\u0637\u0638\u0639\u063a\u0641\u0642\u06a9\u06af\u0644\u0645\u0646\u0648\u0647\u0629\u06cc\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9\u06f0']+$/i,
  bn: /^['\u0980\u0981\u0982\u0983\u0985\u0986\u0987\u0988\u0989\u098a\u098b\u098c\u098f\u0990\u0993\u0994\u0995\u0996\u0997\u0998\u0999\u099a\u099b\u099c\u099d\u099e\u099f\u09a0\u09a1\u09a2\u09a3\u09a4\u09a5\u09a6\u09a7\u09a8\u09aa\u09ab\u09ac\u09ad\u09ae\u09af\u09b0\u09b2\u09b6\u09b7\u09b8\u09b9\u09bc\u09bd\u09be\u09bf\u09c0\u09c1\u09c2\u09c3\u09c4\u09c7\u09c8\u09cb\u09cc\u09cd\u09ce\u09d7\u09dc\u09dd\u09df\u09e0\u09e1\u09e2\u09e3\u09e6\u09e7\u09e8\u09e9\u09ea\u09eb\u09ec\u09ed\u09ee\u09ef\u09f0\u09f1\u09f2\u09f3\u09f4\u09f5\u09f6\u09f7\u09f8\u09f9\u09fa\u09fb']+$/,
  eo: /^[0-9ABC\u0108D-G\u011cH\u0124IJ\u0134K-PRS\u015cTU\u016cVZ]+$/i,
  'hi-IN': /^[\u0900-\u0963]+[\u0966-\u097F]*$/i,
  'si-LK': /^[0-9\u0D80-\u0DFF]+$/
};
var decimal = exports.decimal = {
  'en-US': '.',
  ar: '\u066b'
};
var englishLocales = exports.englishLocales = ['AU', 'GB', 'HK', 'IN', 'NZ', 'ZA', 'ZM'];
for (var locale, i = 0; i < englishLocales.length; i++) {
  locale = "en-".concat(englishLocales[i]);
  alpha[locale] = alpha['en-US'];
  alphanumeric[locale] = alphanumeric['en-US'];
  decimal[locale] = decimal['en-US'];
}

// Source: http://www.localeplanet.com/java/
var arabicLocales = exports.arabicLocales = ['AE', 'BH', 'DZ', 'EG', 'IQ', 'JO', 'KW', 'LB', 'LY', 'MA', 'QM', 'QA', 'SA', 'SD', 'SY', 'TN', 'YE'];
for (var _locale, _i = 0; _i < arabicLocales.length; _i++) {
  _locale = "ar-".concat(arabicLocales[_i]);
  alpha[_locale] = alpha.ar;
  alphanumeric[_locale] = alphanumeric.ar;
  decimal[_locale] = decimal.ar;
}
var farsiLocales = exports.farsiLocales = ['IR', 'AF'];
for (var _locale2, _i2 = 0; _i2 < farsiLocales.length; _i2++) {
  _locale2 = "fa-".concat(farsiLocales[_i2]);
  alphanumeric[_locale2] = alphanumeric.fa;
  decimal[_locale2] = decimal.ar;
}
var bengaliLocales = exports.bengaliLocales = ['BD', 'IN'];
for (var _locale3, _i3 = 0; _i3 < bengaliLocales.length; _i3++) {
  _locale3 = "bn-".concat(bengaliLocales[_i3]);
  alpha[_locale3] = alpha.bn;
  alphanumeric[_locale3] = alphanumeric.bn;
  decimal[_locale3] = decimal['en-US'];
}

// Source: https://en.wikipedia.org/wiki/Decimal_mark
var dotDecimal = exports.dotDecimal = ['ar-EG', 'ar-LB', 'ar-LY'];
var commaDecimal = exports.commaDecimal = ['bg-BG', 'cs-CZ', 'da-DK', 'de-DE', 'el-GR', 'en-ZM', 'eo', 'es-ES', 'fr-CA', 'fr-FR', 'id-ID', 'it-IT', 'ku-IQ', 'hi-IN', 'hu-HU', 'nb-NO', 'nn-NO', 'nl-NL', 'pl-PL', 'pt-PT', 'ru-RU', 'kk-KZ', 'si-LK', 'sl-SI', 'sr-RS@latin', 'sr-RS', 'sv-SE', 'tr-TR', 'uk-UA', 'vi-VN'];
for (var _i4 = 0; _i4 < dotDecimal.length; _i4++) {
  decimal[dotDecimal[_i4]] = decimal['en-US'];
}
for (var _i5 = 0; _i5 < commaDecimal.length; _i5++) {
  decimal[commaDecimal[_i5]] = ',';
}
alpha['fr-CA'] = alpha['fr-FR'];
alphanumeric['fr-CA'] = alphanumeric['fr-FR'];
alpha['pt-BR'] = alpha['pt-PT'];
alphanumeric['pt-BR'] = alphanumeric['pt-PT'];
decimal['pt-BR'] = decimal['pt-PT'];

// see #862
alpha['pl-Pl'] = alpha['pl-PL'];
alphanumeric['pl-Pl'] = alphanumeric['pl-PL'];
decimal['pl-Pl'] = decimal['pl-PL'];

// see #1455
alpha['fa-AF'] = alpha.fa;

/***/ }),

/***/ 3399:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = assertString;
function assertString(input) {
  if (input === undefined || input === null) throw new TypeError("Expected a string but received a ".concat(input));
  if (input.constructor.name !== 'String') throw new TypeError("Expected a string but received a ".concat(input.constructor.name));
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3405:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.ScriptCodes = void 0;
exports["default"] = isISO15924;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// from https://www.unicode.org/iso15924/iso15924-codes.html
var validISO15924Codes = new Set(['Adlm', 'Afak', 'Aghb', 'Ahom', 'Arab', 'Aran', 'Armi', 'Armn', 'Avst', 'Bali', 'Bamu', 'Bass', 'Batk', 'Beng', 'Bhks', 'Blis', 'Bopo', 'Brah', 'Brai', 'Bugi', 'Buhd', 'Cakm', 'Cans', 'Cari', 'Cham', 'Cher', 'Chis', 'Chrs', 'Cirt', 'Copt', 'Cpmn', 'Cprt', 'Cyrl', 'Cyrs', 'Deva', 'Diak', 'Dogr', 'Dsrt', 'Dupl', 'Egyd', 'Egyh', 'Egyp', 'Elba', 'Elym', 'Ethi', 'Gara', 'Geok', 'Geor', 'Glag', 'Gong', 'Gonm', 'Goth', 'Gran', 'Grek', 'Gujr', 'Gukh', 'Guru', 'Hanb', 'Hang', 'Hani', 'Hano', 'Hans', 'Hant', 'Hatr', 'Hebr', 'Hira', 'Hluw', 'Hmng', 'Hmnp', 'Hrkt', 'Hung', 'Inds', 'Ital', 'Jamo', 'Java', 'Jpan', 'Jurc', 'Kali', 'Kana', 'Kawi', 'Khar', 'Khmr', 'Khoj', 'Kitl', 'Kits', 'Knda', 'Kore', 'Kpel', 'Krai', 'Kthi', 'Lana', 'Laoo', 'Latf', 'Latg', 'Latn', 'Leke', 'Lepc', 'Limb', 'Lina', 'Linb', 'Lisu', 'Loma', 'Lyci', 'Lydi', 'Mahj', 'Maka', 'Mand', 'Mani', 'Marc', 'Maya', 'Medf', 'Mend', 'Merc', 'Mero', 'Mlym', 'Modi', 'Mong', 'Moon', 'Mroo', 'Mtei', 'Mult', 'Mymr', 'Nagm', 'Nand', 'Narb', 'Nbat', 'Newa', 'Nkdb', 'Nkgb', 'Nkoo', 'Nshu', 'Ogam', 'Olck', 'Onao', 'Orkh', 'Orya', 'Osge', 'Osma', 'Ougr', 'Palm', 'Pauc', 'Pcun', 'Pelm', 'Perm', 'Phag', 'Phli', 'Phlp', 'Phlv', 'Phnx', 'Plrd', 'Piqd', 'Prti', 'Psin', 'Qaaa', 'Qaab', 'Qaac', 'Qaad', 'Qaae', 'Qaaf', 'Qaag', 'Qaah', 'Qaai', 'Qaaj', 'Qaak', 'Qaal', 'Qaam', 'Qaan', 'Qaao', 'Qaap', 'Qaaq', 'Qaar', 'Qaas', 'Qaat', 'Qaau', 'Qaav', 'Qaaw', 'Qaax', 'Qaay', 'Qaaz', 'Qaba', 'Qabb', 'Qabc', 'Qabd', 'Qabe', 'Qabf', 'Qabg', 'Qabh', 'Qabi', 'Qabj', 'Qabk', 'Qabl', 'Qabm', 'Qabn', 'Qabo', 'Qabp', 'Qabq', 'Qabr', 'Qabs', 'Qabt', 'Qabu', 'Qabv', 'Qabw', 'Qabx', 'Ranj', 'Rjng', 'Rohg', 'Roro', 'Runr', 'Samr', 'Sara', 'Sarb', 'Saur', 'Sgnw', 'Shaw', 'Shrd', 'Shui', 'Sidd', 'Sidt', 'Sind', 'Sinh', 'Sogd', 'Sogo', 'Sora', 'Soyo', 'Sund', 'Sunu', 'Sylo', 'Syrc', 'Syre', 'Syrj', 'Syrn', 'Tagb', 'Takr', 'Tale', 'Talu', 'Taml', 'Tang', 'Tavt', 'Tayo', 'Telu', 'Teng', 'Tfng', 'Tglg', 'Thaa', 'Thai', 'Tibt', 'Tirh', 'Tnsa', 'Todr', 'Tols', 'Toto', 'Tutg', 'Ugar', 'Vaii', 'Visp', 'Vith', 'Wara', 'Wcho', 'Wole', 'Xpeo', 'Xsux', 'Yezi', 'Yiii', 'Zanb', 'Zinh', 'Zmth', 'Zsye', 'Zsym', 'Zxxx', 'Zyyy', 'Zzzz']);
function isISO15924(str) {
  (0, _assertString.default)(str);
  return validISO15924Codes.has(str);
}
var ScriptCodes = exports.ScriptCodes = validISO15924Codes;

/***/ }),

/***/ 3442:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isPassportNumber;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/**
 * Reference:
 * https://en.wikipedia.org/ -- Wikipedia
 * https://docs.microsoft.com/en-us/microsoft-365/compliance/eu-passport-number -- EU Passport Number
 * https://countrycode.org/ -- Country Codes
 */
var passportRegexByCountryCode = {
  AM: /^[A-Z]{2}\d{7}$/,
  // ARMENIA
  AR: /^[A-Z]{3}\d{6}$/,
  // ARGENTINA
  AT: /^[A-Z]\d{7}$/,
  // AUSTRIA
  AU: /^[A-Z]\d{7}$/,
  // AUSTRALIA
  AZ: /^[A-Z]{1}\d{8}$/,
  // AZERBAIJAN
  BE: /^[A-Z]{2}\d{6}$/,
  // BELGIUM
  BG: /^\d{9}$/,
  // BULGARIA
  BR: /^[A-Z]{2}\d{6}$/,
  // BRAZIL
  BY: /^[A-Z]{2}\d{7}$/,
  // BELARUS
  CA: /^[A-Z]{2}\d{6}$|^[A-Z]\d{6}[A-Z]{2}$/,
  // CANADA
  CH: /^[A-Z]\d{7}$/,
  // SWITZERLAND
  CN: /^G\d{8}$|^E(?![IO])[A-Z0-9]\d{7}$/,
  // CHINA [G=Ordinary, E=Electronic] followed by 8-digits, or E followed by any UPPERCASE letter (except I and O) followed by 7 digits
  CY: /^[A-Z](\d{6}|\d{8})$/,
  // CYPRUS
  CZ: /^\d{8}$/,
  // CZECH REPUBLIC
  DE: /^[CFGHJKLMNPRTVWXYZ0-9]{9}$/,
  // GERMANY
  DK: /^\d{9}$/,
  // DENMARK
  DZ: /^\d{9}$/,
  // ALGERIA
  EE: /^([A-Z]\d{7}|[A-Z]{2}\d{7})$/,
  // ESTONIA (K followed by 7-digits), e-passports have 2 UPPERCASE followed by 7 digits
  ES: /^[A-Z0-9]{2}([A-Z0-9]?)\d{6}$/,
  // SPAIN
  FI: /^[A-Z]{2}\d{7}$/,
  // FINLAND
  FR: /^\d{2}[A-Z]{2}\d{5}$/,
  // FRANCE
  GB: /^\d{9}$/,
  // UNITED KINGDOM
  GR: /^[A-Z]{2}\d{7}$/,
  // GREECE
  HR: /^\d{9}$/,
  // CROATIA
  HU: /^[A-Z]{2}(\d{6}|\d{7})$/,
  // HUNGARY
  IE: /^[A-Z0-9]{2}\d{7}$/,
  // IRELAND
  IN: /^[A-Z]{1}-?\d{7}$/,
  // INDIA
  ID: /^[A-C]\d{7}$/,
  // INDONESIA
  IR: /^[A-Z]\d{8}$/,
  // IRAN
  IS: /^(A)\d{7}$/,
  // ICELAND
  IT: /^[A-Z0-9]{2}\d{7}$/,
  // ITALY
  JM: /^[Aa]\d{7}$/,
  // JAMAICA
  JP: /^[A-Z]{2}\d{7}$/,
  // JAPAN
  KR: /^[MS]\d{8}$/,
  // SOUTH KOREA, REPUBLIC OF KOREA, [S=PS Passports, M=PM Passports]
  KZ: /^[a-zA-Z]\d{7}$/,
  // KAZAKHSTAN
  LI: /^[a-zA-Z]\d{5}$/,
  // LIECHTENSTEIN
  LT: /^[A-Z0-9]{8}$/,
  // LITHUANIA
  LU: /^[A-Z0-9]{8}$/,
  // LUXEMBURG
  LV: /^[A-Z0-9]{2}\d{7}$/,
  // LATVIA
  LY: /^[A-Z0-9]{8}$/,
  // LIBYA
  MT: /^\d{7}$/,
  // MALTA
  MZ: /^([A-Z]{2}\d{7})|(\d{2}[A-Z]{2}\d{5})$/,
  // MOZAMBIQUE
  MY: /^[AHK]\d{8}$/,
  // MALAYSIA
  MX: /^\d{10,11}$/,
  // MEXICO
  NL: /^[A-Z]{2}[A-Z0-9]{6}\d$/,
  // NETHERLANDS
  NZ: /^([Ll]([Aa]|[Dd]|[Ff]|[Hh])|[Ee]([Aa]|[Pp])|[Nn])\d{6}$/,
  // NEW ZEALAND
  PH: /^([A-Z](\d{6}|\d{7}[A-Z]))|([A-Z]{2}(\d{6}|\d{7}))$/,
  // PHILIPPINES
  PK: /^[A-Z]{2}\d{7}$/,
  // PAKISTAN
  PL: /^[A-Z]{2}\d{7}$/,
  // POLAND
  PT: /^[A-Z]\d{6}$/,
  // PORTUGAL
  RO: /^\d{8,9}$/,
  // ROMANIA
  RU: /^\d{9}$/,
  // RUSSIAN FEDERATION
  SE: /^\d{8}$/,
  // SWEDEN
  SL: /^(P)[A-Z]\d{7}$/,
  // SLOVENIA
  SK: /^[0-9A-Z]\d{7}$/,
  // SLOVAKIA
  TH: /^[A-Z]{1,2}\d{6,7}$/,
  // THAILAND
  TR: /^[A-Z]\d{8}$/,
  // TURKEY
  UA: /^[A-Z]{2}\d{6}$/,
  // UKRAINE
  US: /^\d{9}$|^[A-Z]\d{8}$/,
  // UNITED STATES
  ZA: /^[TAMD]\d{8}$/ // SOUTH AFRICA
};
var locales = exports.locales = Object.keys(passportRegexByCountryCode);

/**
 * Check if str is a valid passport number
 * relative to provided ISO Country Code.
 *
 * @param {string} str
 * @param {string} countryCode
 * @return {boolean}
 */
function isPassportNumber(str, countryCode) {
  (0, _assertString.default)(str);
  /** Remove All Whitespaces, Convert to UPPERCASE */
  var normalizedStr = str.replace(/\s/g, '').toUpperCase();
  return countryCode.toUpperCase() in passportRegexByCountryCode && passportRegexByCountryCode[countryCode].test(normalizedStr);
}

/***/ }),

/***/ 3459:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isSurrogatePair;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var surrogatePair = /[\uD800-\uDBFF][\uDC00-\uDFFF]/;
function isSurrogatePair(str) {
  (0, _assertString.default)(str);
  return surrogatePair.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3583:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isDataURI;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var validMediaType = /^[a-z]+\/[a-z0-9\-\+\._]+$/i;
var validAttribute = /^[a-z\-]+=[a-z0-9\-]+$/i;
var validData = /^[a-z0-9!\$&'\(\)\*\+,;=\-\._~:@\/\?%\s]*$/i;
function isDataURI(str) {
  (0, _assertString.default)(str);
  var data = str.split(',');
  if (data.length < 2) {
    return false;
  }
  var attributes = data.shift().trim().split(';');
  var schemeAndMediaType = attributes.shift();
  if (schemeAndMediaType.slice(0, 5) !== 'data:') {
    return false;
  }
  var mediaType = schemeAndMediaType.slice(5);
  if (mediaType !== '' && !validMediaType.test(mediaType)) {
    return false;
  }
  for (var i = 0; i < attributes.length; i++) {
    if (!(i === attributes.length - 1 && attributes[i].toLowerCase() === 'base64') && !validAttribute.test(attributes[i])) {
      return false;
    }
  }
  for (var _i = 0; _i < data.length; _i++) {
    if (!validData.test(data[_i])) {
      return false;
    }
  }
  return true;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3609:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLuhnNumber;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isLuhnNumber(str) {
  (0, _assertString.default)(str);
  var sanitized = str.replace(/[- ]+/g, '');
  var sum = 0;
  var digit;
  var tmpNum;
  var shouldDouble;
  for (var i = sanitized.length - 1; i >= 0; i--) {
    digit = sanitized.substring(i, i + 1);
    tmpNum = parseInt(digit, 10);
    if (shouldDouble) {
      tmpNum *= 2;
      if (tmpNum >= 10) {
        sum += tmpNum % 10 + 1;
      } else {
        sum += tmpNum;
      }
    } else {
      sum += tmpNum;
    }
    shouldDouble = !shouldDouble;
  }
  return !!(sum % 10 === 0 ? sanitized : false);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3610:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = merge;
function merge() {
  var obj = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {};
  var defaults = arguments.length > 1 ? arguments[1] : undefined;
  for (var key in defaults) {
    if (typeof obj[key] === 'undefined') {
      obj[key] = defaults[key];
    }
  }
  return obj;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3641:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIBAN;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _includesArray = _interopRequireDefault(__webpack_require__(8644));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/**
 * List of country codes with
 * corresponding IBAN regular expression
 * Reference: https://en.wikipedia.org/wiki/International_Bank_Account_Number
 */
var ibanRegexThroughCountryCode = {
  AD: /^(AD[0-9]{2})\d{8}[A-Z0-9]{12}$/,
  AE: /^(AE[0-9]{2})\d{3}\d{16}$/,
  AL: /^(AL[0-9]{2})\d{8}[A-Z0-9]{16}$/,
  AT: /^(AT[0-9]{2})\d{16}$/,
  AZ: /^(AZ[0-9]{2})[A-Z0-9]{4}\d{20}$/,
  BA: /^(BA[0-9]{2})\d{16}$/,
  BE: /^(BE[0-9]{2})\d{12}$/,
  BG: /^(BG[0-9]{2})[A-Z]{4}\d{6}[A-Z0-9]{8}$/,
  BH: /^(BH[0-9]{2})[A-Z]{4}[A-Z0-9]{14}$/,
  BR: /^(BR[0-9]{2})\d{23}[A-Z]{1}[A-Z0-9]{1}$/,
  BY: /^(BY[0-9]{2})[A-Z0-9]{4}\d{20}$/,
  CH: /^(CH[0-9]{2})\d{5}[A-Z0-9]{12}$/,
  CR: /^(CR[0-9]{2})\d{18}$/,
  CY: /^(CY[0-9]{2})\d{8}[A-Z0-9]{16}$/,
  CZ: /^(CZ[0-9]{2})\d{20}$/,
  DE: /^(DE[0-9]{2})\d{18}$/,
  DK: /^(DK[0-9]{2})\d{14}$/,
  DO: /^(DO[0-9]{2})[A-Z]{4}\d{20}$/,
  DZ: /^(DZ\d{24})$/,
  EE: /^(EE[0-9]{2})\d{16}$/,
  EG: /^(EG[0-9]{2})\d{25}$/,
  ES: /^(ES[0-9]{2})\d{20}$/,
  FI: /^(FI[0-9]{2})\d{14}$/,
  FO: /^(FO[0-9]{2})\d{14}$/,
  FR: /^(FR[0-9]{2})\d{10}[A-Z0-9]{11}\d{2}$/,
  GB: /^(GB[0-9]{2})[A-Z]{4}\d{14}$/,
  GE: /^(GE[0-9]{2})[A-Z0-9]{2}\d{16}$/,
  GI: /^(GI[0-9]{2})[A-Z]{4}[A-Z0-9]{15}$/,
  GL: /^(GL[0-9]{2})\d{14}$/,
  GR: /^(GR[0-9]{2})\d{7}[A-Z0-9]{16}$/,
  GT: /^(GT[0-9]{2})[A-Z0-9]{4}[A-Z0-9]{20}$/,
  HR: /^(HR[0-9]{2})\d{17}$/,
  HU: /^(HU[0-9]{2})\d{24}$/,
  IE: /^(IE[0-9]{2})[A-Z]{4}\d{14}$/,
  IL: /^(IL[0-9]{2})\d{19}$/,
  IQ: /^(IQ[0-9]{2})[A-Z]{4}\d{15}$/,
  IR: /^(IR[0-9]{2})0\d{2}0\d{18}$/,
  IS: /^(IS[0-9]{2})\d{22}$/,
  IT: /^(IT[0-9]{2})[A-Z]{1}\d{10}[A-Z0-9]{12}$/,
  JO: /^(JO[0-9]{2})[A-Z]{4}\d{22}$/,
  KW: /^(KW[0-9]{2})[A-Z]{4}[A-Z0-9]{22}$/,
  KZ: /^(KZ[0-9]{2})\d{3}[A-Z0-9]{13}$/,
  LB: /^(LB[0-9]{2})\d{4}[A-Z0-9]{20}$/,
  LC: /^(LC[0-9]{2})[A-Z]{4}[A-Z0-9]{24}$/,
  LI: /^(LI[0-9]{2})\d{5}[A-Z0-9]{12}$/,
  LT: /^(LT[0-9]{2})\d{16}$/,
  LU: /^(LU[0-9]{2})\d{3}[A-Z0-9]{13}$/,
  LV: /^(LV[0-9]{2})[A-Z]{4}[A-Z0-9]{13}$/,
  MA: /^(MA[0-9]{26})$/,
  MC: /^(MC[0-9]{2})\d{10}[A-Z0-9]{11}\d{2}$/,
  MD: /^(MD[0-9]{2})[A-Z0-9]{20}$/,
  ME: /^(ME[0-9]{2})\d{18}$/,
  MK: /^(MK[0-9]{2})\d{3}[A-Z0-9]{10}\d{2}$/,
  MR: /^(MR[0-9]{2})\d{23}$/,
  MT: /^(MT[0-9]{2})[A-Z]{4}\d{5}[A-Z0-9]{18}$/,
  MU: /^(MU[0-9]{2})[A-Z]{4}\d{19}[A-Z]{3}$/,
  MZ: /^(MZ[0-9]{2})\d{21}$/,
  NL: /^(NL[0-9]{2})[A-Z]{4}\d{10}$/,
  NO: /^(NO[0-9]{2})\d{11}$/,
  PK: /^(PK[0-9]{2})[A-Z0-9]{4}\d{16}$/,
  PL: /^(PL[0-9]{2})\d{24}$/,
  PS: /^(PS[0-9]{2})[A-Z]{4}[A-Z0-9]{21}$/,
  PT: /^(PT[0-9]{2})\d{21}$/,
  QA: /^(QA[0-9]{2})[A-Z]{4}[A-Z0-9]{21}$/,
  RO: /^(RO[0-9]{2})[A-Z]{4}[A-Z0-9]{16}$/,
  RS: /^(RS[0-9]{2})\d{18}$/,
  SA: /^(SA[0-9]{2})\d{2}[A-Z0-9]{18}$/,
  SC: /^(SC[0-9]{2})[A-Z]{4}\d{20}[A-Z]{3}$/,
  SE: /^(SE[0-9]{2})\d{20}$/,
  SI: /^(SI[0-9]{2})\d{15}$/,
  SK: /^(SK[0-9]{2})\d{20}$/,
  SM: /^(SM[0-9]{2})[A-Z]{1}\d{10}[A-Z0-9]{12}$/,
  SV: /^(SV[0-9]{2})[A-Z0-9]{4}\d{20}$/,
  TL: /^(TL[0-9]{2})\d{19}$/,
  TN: /^(TN[0-9]{2})\d{20}$/,
  TR: /^(TR[0-9]{2})\d{5}[A-Z0-9]{17}$/,
  UA: /^(UA[0-9]{2})\d{6}[A-Z0-9]{19}$/,
  VA: /^(VA[0-9]{2})\d{18}$/,
  VG: /^(VG[0-9]{2})[A-Z]{4}\d{16}$/,
  XK: /^(XK[0-9]{2})\d{16}$/
};

/**
 * Check if the country codes passed are valid using the
 * ibanRegexThroughCountryCode as a reference
 *
 * @param {array} countryCodeArray
 * @return {boolean}
 */

function hasOnlyValidCountryCodes(countryCodeArray) {
  var countryCodeArrayFilteredWithObjectIbanCode = countryCodeArray.filter(function (countryCode) {
    return !(countryCode in ibanRegexThroughCountryCode);
  });
  if (countryCodeArrayFilteredWithObjectIbanCode.length > 0) {
    return false;
  }
  return true;
}

/**
 * Check whether string has correct universal IBAN format
 * The IBAN consists of up to 34 alphanumeric characters, as follows:
 * Country Code using ISO 3166-1 alpha-2, two letters
 * check digits, two digits and
 * Basic Bank Account Number (BBAN), up to 30 alphanumeric characters.
 * NOTE: Permitted IBAN characters are: digits [0-9] and the 26 latin alphabetic [A-Z]
 *
 * @param {string} str - string under validation
 * @param {object} options - object to pass the countries to be either whitelisted or blacklisted
 * @return {boolean}
 */
function hasValidIbanFormat(str, options) {
  // Strip white spaces and hyphens
  var strippedStr = str.replace(/[\s\-]+/gi, '').toUpperCase();
  var isoCountryCode = strippedStr.slice(0, 2).toUpperCase();
  var isoCountryCodeInIbanRegexCodeObject = isoCountryCode in ibanRegexThroughCountryCode;
  if (options.whitelist) {
    if (!hasOnlyValidCountryCodes(options.whitelist)) {
      return false;
    }
    var isoCountryCodeInWhiteList = (0, _includesArray.default)(options.whitelist, isoCountryCode);
    if (!isoCountryCodeInWhiteList) {
      return false;
    }
  }
  if (options.blacklist) {
    var isoCountryCodeInBlackList = (0, _includesArray.default)(options.blacklist, isoCountryCode);
    if (isoCountryCodeInBlackList) {
      return false;
    }
  }
  return isoCountryCodeInIbanRegexCodeObject && ibanRegexThroughCountryCode[isoCountryCode].test(strippedStr);
}

/**
   * Check whether string has valid IBAN Checksum
   * by performing basic mod-97 operation and
   * the remainder should equal 1
   * -- Start by rearranging the IBAN by moving the four initial characters to the end of the string
   * -- Replace each letter in the string with two digits, A -> 10, B = 11, Z = 35
   * -- Interpret the string as a decimal integer and
   * -- compute the remainder on division by 97 (mod 97)
   * Reference: https://en.wikipedia.org/wiki/International_Bank_Account_Number
   *
   * @param {string} str
   * @return {boolean}
   */
function hasValidIbanChecksum(str) {
  var strippedStr = str.replace(/[^A-Z0-9]+/gi, '').toUpperCase(); // Keep only digits and A-Z latin alphabetic
  var rearranged = strippedStr.slice(4) + strippedStr.slice(0, 4);
  var alphaCapsReplacedWithDigits = rearranged.replace(/[A-Z]/g, function (char) {
    return char.charCodeAt(0) - 55;
  });
  var remainder = alphaCapsReplacedWithDigits.match(/\d{1,7}/g).reduce(function (acc, value) {
    return Number(acc + value) % 97;
  }, '');
  return remainder === 1;
}
function isIBAN(str) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  (0, _assertString.default)(str);
  return hasValidIbanFormat(str, options) && hasValidIbanChecksum(str);
}
var locales = exports.locales = Object.keys(ibanRegexThroughCountryCode);

/***/ }),

/***/ 3735:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isDivisibleBy;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _toFloat = _interopRequireDefault(__webpack_require__(1371));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isDivisibleBy(str, num) {
  (0, _assertString.default)(str);
  return (0, _toFloat.default)(str) % parseInt(num, 10) === 0;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3752:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = toDate;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function toDate(date) {
  (0, _assertString.default)(date);
  date = Date.parse(date);
  return !isNaN(date) ? new Date(date) : null;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3832:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISO31661Alpha3;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-3
var validISO31661Alpha3CountriesCodes = new Set(['AFG', 'ALA', 'ALB', 'DZA', 'ASM', 'AND', 'AGO', 'AIA', 'ATA', 'ATG', 'ARG', 'ARM', 'ABW', 'AUS', 'AUT', 'AZE', 'BHS', 'BHR', 'BGD', 'BRB', 'BLR', 'BEL', 'BLZ', 'BEN', 'BMU', 'BTN', 'BOL', 'BES', 'BIH', 'BWA', 'BVT', 'BRA', 'IOT', 'BRN', 'BGR', 'BFA', 'BDI', 'KHM', 'CMR', 'CAN', 'CPV', 'CYM', 'CAF', 'TCD', 'CHL', 'CHN', 'CXR', 'CCK', 'COL', 'COM', 'COG', 'COD', 'COK', 'CRI', 'CIV', 'HRV', 'CUB', 'CUW', 'CYP', 'CZE', 'DNK', 'DJI', 'DMA', 'DOM', 'ECU', 'EGY', 'SLV', 'GNQ', 'ERI', 'EST', 'ETH', 'FLK', 'FRO', 'FJI', 'FIN', 'FRA', 'GUF', 'PYF', 'ATF', 'GAB', 'GMB', 'GEO', 'DEU', 'GHA', 'GIB', 'GRC', 'GRL', 'GRD', 'GLP', 'GUM', 'GTM', 'GGY', 'GIN', 'GNB', 'GUY', 'HTI', 'HMD', 'VAT', 'HND', 'HKG', 'HUN', 'ISL', 'IND', 'IDN', 'IRN', 'IRQ', 'IRL', 'IMN', 'ISR', 'ITA', 'JAM', 'JPN', 'JEY', 'JOR', 'KAZ', 'KEN', 'KIR', 'PRK', 'KOR', 'KWT', 'KGZ', 'LAO', 'LVA', 'LBN', 'LSO', 'LBR', 'LBY', 'LIE', 'LTU', 'LUX', 'MAC', 'MKD', 'MDG', 'MWI', 'MYS', 'MDV', 'MLI', 'MLT', 'MHL', 'MTQ', 'MRT', 'MUS', 'MYT', 'MEX', 'FSM', 'MDA', 'MCO', 'MNG', 'MNE', 'MSR', 'MAR', 'MOZ', 'MMR', 'NAM', 'NRU', 'NPL', 'NLD', 'NCL', 'NZL', 'NIC', 'NER', 'NGA', 'NIU', 'NFK', 'MNP', 'NOR', 'OMN', 'PAK', 'PLW', 'PSE', 'PAN', 'PNG', 'PRY', 'PER', 'PHL', 'PCN', 'POL', 'PRT', 'PRI', 'QAT', 'REU', 'ROU', 'RUS', 'RWA', 'BLM', 'SHN', 'KNA', 'LCA', 'MAF', 'SPM', 'VCT', 'WSM', 'SMR', 'STP', 'SAU', 'SEN', 'SRB', 'SYC', 'SLE', 'SGP', 'SXM', 'SVK', 'SVN', 'SLB', 'SOM', 'ZAF', 'SGS', 'SSD', 'ESP', 'LKA', 'SDN', 'SUR', 'SJM', 'SWZ', 'SWE', 'CHE', 'SYR', 'TWN', 'TJK', 'TZA', 'THA', 'TLS', 'TGO', 'TKL', 'TON', 'TTO', 'TUN', 'TUR', 'TKM', 'TCA', 'TUV', 'UGA', 'UKR', 'ARE', 'GBR', 'USA', 'UMI', 'URY', 'UZB', 'VUT', 'VEN', 'VNM', 'VGB', 'VIR', 'WLF', 'ESH', 'YEM', 'ZMB', 'ZWE']);
function isISO31661Alpha3(str) {
  (0, _assertString.default)(str);
  return validISO31661Alpha3CountriesCodes.has(str.toUpperCase());
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3906:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isPort;
var _isInt = _interopRequireDefault(__webpack_require__(6084));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isPort(str) {
  return (0, _isInt.default)(str, {
    allow_leading_zeroes: false,
    min: 0,
    max: 65535
  });
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 3939:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isPostalCode;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// common patterns
var threeDigit = /^\d{3}$/;
var fourDigit = /^\d{4}$/;
var fiveDigit = /^\d{5}$/;
var sixDigit = /^\d{6}$/;
var patterns = {
  AD: /^AD\d{3}$/,
  AT: fourDigit,
  AU: fourDigit,
  AZ: /^AZ\d{4}$/,
  BA: /^([7-8]\d{4}$)/,
  BD: /^([1-8][0-9]{3}|9[0-4][0-9]{2})$/,
  BE: fourDigit,
  BG: fourDigit,
  BR: /^\d{5}-?\d{3}$/,
  BY: /^2[1-4]\d{4}$/,
  CA: /^[ABCEGHJKLMNPRSTVXY]\d[ABCEGHJ-NPRSTV-Z][\s\-]?\d[ABCEGHJ-NPRSTV-Z]\d$/i,
  CH: fourDigit,
  CN: /^(0[1-7]|1[012356]|2[0-7]|3[0-6]|4[0-7]|5[1-7]|6[1-7]|7[1-5]|8[1345]|9[09])\d{4}$/,
  CO: /^(05|08|11|13|15|17|18|19|20|23|25|27|41|44|47|50|52|54|63|66|68|70|73|76|81|85|86|88|91|94|95|97|99)(\d{4})$/,
  CZ: /^\d{3}\s?\d{2}$/,
  DE: fiveDigit,
  DK: fourDigit,
  DO: fiveDigit,
  DZ: fiveDigit,
  EE: fiveDigit,
  ES: /^(5[0-2]{1}|[0-4]{1}\d{1})\d{3}$/,
  FI: fiveDigit,
  FR: /^(?:(?:0[1-9]|[1-8]\d|9[0-5])\d{3}|97[1-46]\d{2})$/,
  GB: /^(gir\s?0aa|[a-z]{1,2}\d[\da-z]?\s?(\d[a-z]{2})?)$/i,
  GR: /^\d{3}\s?\d{2}$/,
  HR: /^([1-5]\d{4}$)/,
  HT: /^HT\d{4}$/,
  HU: fourDigit,
  ID: fiveDigit,
  IE: /^(?!.*(?:o))[A-Za-z]\d[\dw]\s\w{4}$/i,
  IL: /^(\d{5}|\d{7})$/,
  IN: /^((?!10|29|35|54|55|65|66|86|87|88|89)[1-9][0-9]{5})$/,
  IR: /^(?!(\d)\1{3})[13-9]{4}[1346-9][013-9]{5}$/,
  IS: threeDigit,
  IT: fiveDigit,
  JP: /^\d{3}\-\d{4}$/,
  KE: fiveDigit,
  KR: /^(\d{5}|\d{6})$/,
  LI: /^(948[5-9]|949[0-7])$/,
  LT: /^LT\-\d{5}$/,
  LU: fourDigit,
  LV: /^LV\-\d{4}$/,
  LK: fiveDigit,
  MG: threeDigit,
  MX: fiveDigit,
  MT: /^[A-Za-z]{3}\s{0,1}\d{4}$/,
  MY: fiveDigit,
  NL: /^[1-9]\d{3}\s?(?!sa|sd|ss)[a-z]{2}$/i,
  NO: fourDigit,
  NP: /^(10|21|22|32|33|34|44|45|56|57)\d{3}$|^(977)$/i,
  NZ: fourDigit,
  // https://www.pakpost.gov.pk/postcodes.php
  PK: fiveDigit,
  PL: /^\d{2}\-\d{3}$/,
  PR: /^00[679]\d{2}([ -]\d{4})?$/,
  PT: /^\d{4}\-\d{3}?$/,
  RO: sixDigit,
  RU: sixDigit,
  SA: fiveDigit,
  SE: /^[1-9]\d{2}\s?\d{2}$/,
  SG: sixDigit,
  SI: fourDigit,
  SK: /^\d{3}\s?\d{2}$/,
  TH: fiveDigit,
  TN: fourDigit,
  TW: /^\d{3}(\d{2,3})?$/,
  UA: fiveDigit,
  US: /^\d{5}(-\d{4})?$/,
  ZA: fourDigit,
  ZM: fiveDigit
};
var locales = exports.locales = Object.keys(patterns);
function isPostalCode(str, locale) {
  (0, _assertString.default)(str);
  if (locale in patterns) {
    return patterns[locale].test(str);
  } else if (locale === 'any') {
    for (var key in patterns) {
      // https://github.com/gotwarlost/istanbul/blob/master/ignoring-code-for-coverage.md#ignoring-code-for-coverage-purposes
      // istanbul ignore else
      if (patterns.hasOwnProperty(key)) {
        var pattern = patterns[key];
        if (pattern.test(str)) {
          return true;
        }
      }
    }
    return false;
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}

/***/ }),

/***/ 3973:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isHash;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var lengths = {
  md5: 32,
  md4: 32,
  sha1: 40,
  sha256: 64,
  sha384: 96,
  sha512: 128,
  ripemd128: 32,
  ripemd160: 40,
  tiger128: 32,
  tiger160: 40,
  tiger192: 48,
  crc32: 8,
  crc32b: 8
};
function isHash(str, algorithm) {
  (0, _assertString.default)(str);
  var hash = new RegExp("^[a-fA-F0-9]{".concat(lengths[algorithm], "}$"));
  return hash.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4294:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isAscii;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/* eslint-disable no-control-regex */
var ascii = /^[\x00-\x7F]+$/;
/* eslint-enable no-control-regex */

function isAscii(str) {
  (0, _assertString.default)(str);
  return ascii.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4325:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isAbaRouting;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// http://www.brainjar.com/js/validation/
// https://www.aba.com/news-research/research-analysis/routing-number-policy-procedures
// series reserved for future use are excluded
var isRoutingReg = /^(?!(1[3-9])|(20)|(3[3-9])|(4[0-9])|(5[0-9])|(60)|(7[3-9])|(8[1-9])|(9[0-2])|(9[3-9]))[0-9]{9}$/;
function isAbaRouting(str) {
  (0, _assertString.default)(str);
  if (!isRoutingReg.test(str)) return false;
  var checkSumVal = 0;
  for (var i = 0; i < str.length; i++) {
    if (i % 3 === 0) checkSumVal += str[i] * 3;else if (i % 3 === 1) checkSumVal += str[i] * 7;else checkSumVal += str[i] * 1;
  }
  return checkSumVal % 10 === 0;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4633:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMimeType;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/*
  Checks if the provided string matches to a correct Media type format (MIME type)

  This function only checks is the string format follows the
  established rules by the according RFC specifications.
  This function supports 'charset' in textual media types
  (https://tools.ietf.org/html/rfc6657).

  This function does not check against all the media types listed
  by the IANA (https://www.iana.org/assignments/media-types/media-types.xhtml)
  because of lightness purposes : it would require to include
  all these MIME types in this library, which would weigh it
  significantly. This kind of effort maybe is not worth for the use that
  this function has in this entire library.

  More information in the RFC specifications :
  - https://tools.ietf.org/html/rfc2045
  - https://tools.ietf.org/html/rfc2046
  - https://tools.ietf.org/html/rfc7231#section-3.1.1.1
  - https://tools.ietf.org/html/rfc7231#section-3.1.1.5
*/

// Match simple MIME types
// NB :
//   Subtype length must not exceed 100 characters.
//   This rule does not comply to the RFC specs (what is the max length ?).
var mimeTypeSimple = /^(application|audio|font|image|message|model|multipart|text|video)\/[a-zA-Z0-9\.\-\+_]{1,100}$/i; // eslint-disable-line max-len

// Handle "charset" in "text/*"
var mimeTypeText = /^text\/[a-zA-Z0-9\.\-\+]{1,100};\s?charset=("[a-zA-Z0-9\.\-\+\s]{0,70}"|[a-zA-Z0-9\.\-\+]{0,70})(\s?\([a-zA-Z0-9\.\-\+\s]{1,20}\))?$/i; // eslint-disable-line max-len

// Handle "boundary" in "multipart/*"
var mimeTypeMultipart = /^multipart\/[a-zA-Z0-9\.\-\+]{1,100}(;\s?(boundary|charset)=("[a-zA-Z0-9\.\-\+\s]{0,70}"|[a-zA-Z0-9\.\-\+]{0,70})(\s?\([a-zA-Z0-9\.\-\+\s]{1,20}\))?){0,2}$/i; // eslint-disable-line max-len

function isMimeType(str) {
  (0, _assertString.default)(str);
  return mimeTypeSimple.test(str) || mimeTypeText.test(str) || mimeTypeMultipart.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4636:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = void 0;
var includes = function includes(str, val) {
  return str.indexOf(val) !== -1;
};
var _default = exports["default"] = includes;
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4641:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isHexColor;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var hexcolor = /^#?([0-9A-F]{3}|[0-9A-F]{4}|[0-9A-F]{6}|[0-9A-F]{8})$/i;
function isHexColor(str) {
  (0, _assertString.default)(str);
  return hexcolor.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 4834:
/***/ ((__unused_webpack_module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.iso7064Check = iso7064Check;
exports.luhnCheck = luhnCheck;
exports.reverseMultiplyAndSum = reverseMultiplyAndSum;
exports.verhoeffCheck = verhoeffCheck;
/**
 * Algorithmic validation functions
 * May be used as is or implemented in the workflow of other validators.
 */

/*
 * ISO 7064 validation function
 * Called with a string of numbers (incl. check digit)
 * to validate according to ISO 7064 (MOD 11, 10).
 */
function iso7064Check(str) {
  var checkvalue = 10;
  for (var i = 0; i < str.length - 1; i++) {
    checkvalue = (parseInt(str[i], 10) + checkvalue) % 10 === 0 ? 10 * 2 % 11 : (parseInt(str[i], 10) + checkvalue) % 10 * 2 % 11;
  }
  checkvalue = checkvalue === 1 ? 0 : 11 - checkvalue;
  return checkvalue === parseInt(str[10], 10);
}

/*
 * Luhn (mod 10) validation function
 * Called with a string of numbers (incl. check digit)
 * to validate according to the Luhn algorithm.
 */
function luhnCheck(str) {
  var checksum = 0;
  var second = false;
  for (var i = str.length - 1; i >= 0; i--) {
    if (second) {
      var product = parseInt(str[i], 10) * 2;
      if (product > 9) {
        // sum digits of product and add to checksum
        checksum += product.toString().split('').map(function (a) {
          return parseInt(a, 10);
        }).reduce(function (a, b) {
          return a + b;
        }, 0);
      } else {
        checksum += product;
      }
    } else {
      checksum += parseInt(str[i], 10);
    }
    second = !second;
  }
  return checksum % 10 === 0;
}

/*
 * Reverse TIN multiplication and summation helper function
 * Called with an array of single-digit integers and a base multiplier
 * to calculate the sum of the digits multiplied in reverse.
 * Normally used in variations of MOD 11 algorithmic checks.
 */
function reverseMultiplyAndSum(digits, base) {
  var total = 0;
  for (var i = 0; i < digits.length; i++) {
    total += digits[i] * (base - i);
  }
  return total;
}

/*
 * Verhoeff validation helper function
 * Called with a string of numbers
 * to validate according to the Verhoeff algorithm.
 */
function verhoeffCheck(str) {
  var d_table = [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 4, 0, 6, 7, 8, 9, 5], [2, 3, 4, 0, 1, 7, 8, 9, 5, 6], [3, 4, 0, 1, 2, 8, 9, 5, 6, 7], [4, 0, 1, 2, 3, 9, 5, 6, 7, 8], [5, 9, 8, 7, 6, 0, 4, 3, 2, 1], [6, 5, 9, 8, 7, 1, 0, 4, 3, 2], [7, 6, 5, 9, 8, 2, 1, 0, 4, 3], [8, 7, 6, 5, 9, 3, 2, 1, 0, 4], [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]];
  var p_table = [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 5, 7, 6, 2, 8, 3, 0, 9, 4], [5, 8, 0, 3, 7, 9, 6, 1, 4, 2], [8, 9, 1, 6, 0, 4, 3, 5, 2, 7], [9, 4, 5, 3, 1, 2, 6, 8, 7, 0], [4, 2, 8, 6, 5, 7, 3, 9, 0, 1], [2, 7, 9, 3, 8, 0, 6, 4, 1, 5], [7, 0, 4, 6, 9, 1, 3, 2, 5, 8]];

  // Copy (to prevent replacement) and reverse
  var str_copy = str.split('').reverse().join('');
  var checksum = 0;
  for (var i = 0; i < str_copy.length; i++) {
    checksum = d_table[checksum][p_table[i % 8][parseInt(str_copy[i], 10)]];
  }
  return checksum === 0;
}

/***/ }),

/***/ 5186:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isUUID;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var uuid = {
  1: /^[0-9A-F]{8}-[0-9A-F]{4}-1[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  2: /^[0-9A-F]{8}-[0-9A-F]{4}-2[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  3: /^[0-9A-F]{8}-[0-9A-F]{4}-3[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  4: /^[0-9A-F]{8}-[0-9A-F]{4}-4[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  5: /^[0-9A-F]{8}-[0-9A-F]{4}-5[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  6: /^[0-9A-F]{8}-[0-9A-F]{4}-6[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  7: /^[0-9A-F]{8}-[0-9A-F]{4}-7[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  8: /^[0-9A-F]{8}-[0-9A-F]{4}-8[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/i,
  nil: /^00000000-0000-0000-0000-000000000000$/i,
  max: /^ffffffff-ffff-ffff-ffff-ffffffffffff$/i,
  loose: /^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$/i,
  // From https://github.com/uuidjs/uuid/blob/main/src/regex.js
  all: /^(?:[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|00000000-0000-0000-0000-000000000000|ffffffff-ffff-ffff-ffff-ffffffffffff)$/i
};
function isUUID(str, version) {
  (0, _assertString.default)(str);
  if (version === undefined || version === null) {
    version = 'all';
  }
  return version in uuid ? uuid[version].test(str) : false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5251:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMobilePhone;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/* eslint-disable max-len */
var phones = {
  'am-AM': /^(\+?374|0)(33|4[134]|55|77|88|9[13-689])\d{6}$/,
  'ar-AE': /^((\+?971)|0)?5[024568]\d{7}$/,
  'ar-BH': /^(\+?973)?(3|6)\d{7}$/,
  'ar-DZ': /^(\+?213|0)(5|6|7)\d{8}$/,
  'ar-LB': /^(\+?961)?((3|81)\d{6}|7\d{7})$/,
  'ar-EG': /^((\+?20)|0)?1[0125]\d{8}$/,
  'ar-IQ': /^(\+?964|0)?7[0-9]\d{8}$/,
  'ar-JO': /^(\+?962|0)?7[789]\d{7}$/,
  'ar-KW': /^(\+?965)([569]\d{7}|41\d{6})$/,
  'ar-LY': /^((\+?218)|0)?(9[1-6]\d{7}|[1-8]\d{7,9})$/,
  'ar-MA': /^(?:(?:\+|00)212|0)[5-7]\d{8}$/,
  'ar-OM': /^((\+|00)968)?([79][1-9])\d{6}$/,
  'ar-PS': /^(\+?970|0)5[6|9](\d{7})$/,
  'ar-SA': /^(!?(\+?966)|0)?5\d{8}$/,
  'ar-SD': /^((\+?249)|0)?(9[012369]|1[012])\d{7}$/,
  'ar-SY': /^(!?(\+?963)|0)?9\d{8}$/,
  'ar-TN': /^(\+?216)?[2459]\d{7}$/,
  'az-AZ': /^(\+994|0)(10|5[015]|7[07]|99)\d{7}$/,
  'bs-BA': /^((((\+|00)3876)|06))((([0-3]|[5-6])\d{6})|(4\d{7}))$/,
  'be-BY': /^(\+?375)?(24|25|29|33|44)\d{7}$/,
  'bg-BG': /^(\+?359|0)?8[789]\d{7}$/,
  'bn-BD': /^(\+?880|0)1[13456789][0-9]{8}$/,
  'ca-AD': /^(\+376)?[346]\d{5}$/,
  'cs-CZ': /^(\+?420)? ?[1-9][0-9]{2} ?[0-9]{3} ?[0-9]{3}$/,
  'da-DK': /^(\+?45)?\s?\d{2}\s?\d{2}\s?\d{2}\s?\d{2}$/,
  'de-DE': /^((\+49|0)1)(5[0-25-9]\d|6([23]|0\d?)|7([0-57-9]|6\d))\d{7,9}$/,
  'de-AT': /^(\+43|0)\d{1,4}\d{3,12}$/,
  'de-CH': /^(\+41|0)([1-9])\d{1,9}$/,
  'de-LU': /^(\+352)?((6\d1)\d{6})$/,
  'dv-MV': /^(\+?960)?(7[2-9]|9[1-9])\d{5}$/,
  'el-GR': /^(\+?30|0)?6(8[5-9]|9(?![26])[0-9])\d{7}$/,
  'el-CY': /^(\+?357?)?(9(9|7|6|5|4)\d{6})$/,
  'en-AI': /^(\+?1|0)264(?:2(35|92)|4(?:6[1-2]|76|97)|5(?:3[6-9]|8[1-4])|7(?:2(4|9)|72))\d{4}$/,
  'en-AU': /^(\+?61|0)4\d{8}$/,
  'en-AG': /^(?:\+1|1)268(?:464|7(?:1[3-9]|[28]\d|3[0246]|64|7[0-689]))\d{4}$/,
  'en-BM': /^(\+?1)?441(((3|7)\d{6}$)|(5[0-3][0-9]\d{4}$)|(59\d{5}$))/,
  'en-BS': /^(\+?1[-\s]?|0)?\(?242\)?[-\s]?\d{3}[-\s]?\d{4}$/,
  'en-GB': /^(\+?44|0)7[1-9]\d{8}$/,
  'en-GG': /^(\+?44|0)1481\d{6}$/,
  'en-GH': /^(\+233|0)(20|50|24|54|27|57|26|56|23|53|28|55|59)\d{7}$/,
  'en-GY': /^(\+592|0)6\d{6}$/,
  'en-HK': /^(\+?852[-\s]?)?[456789]\d{3}[-\s]?\d{4}$/,
  'en-MO': /^(\+?853[-\s]?)?[6]\d{3}[-\s]?\d{4}$/,
  'en-IE': /^(\+?353|0)8[356789]\d{7}$/,
  'en-IN': /^(\+?91|0)?[6789]\d{9}$/,
  'en-JM': /^(\+?876)?\d{7}$/,
  'en-KE': /^(\+?254|0)(7|1)\d{8}$/,
  'fr-CF': /^(\+?236| ?)(70|75|77|72|21|22)\d{6}$/,
  'en-SS': /^(\+?211|0)(9[1257])\d{7}$/,
  'en-KI': /^((\+686|686)?)?( )?((6|7)(2|3|8)[0-9]{6})$/,
  'en-KN': /^(?:\+1|1)869(?:46\d|48[89]|55[6-8]|66\d|76[02-7])\d{4}$/,
  'en-LS': /^(\+?266)(22|28|57|58|59|27|52)\d{6}$/,
  'en-MT': /^(\+?356|0)?(99|79|77|21|27|22|25)[0-9]{6}$/,
  'en-MU': /^(\+?230|0)?\d{8}$/,
  'en-MW': /^(\+?265|0)(((77|88|31|99|98|21)\d{7})|(((111)|1)\d{6})|(32000\d{4}))$/,
  'en-NA': /^(\+?264|0)(6|8)\d{7}$/,
  'en-NG': /^(\+?234|0)?[789]\d{9}$/,
  'en-NZ': /^(\+?64|0)[28]\d{7,9}$/,
  'en-PG': /^(\+?675|0)?(7\d|8[18])\d{6}$/,
  'en-PK': /^((00|\+)?92|0)3[0-6]\d{8}$/,
  'en-PH': /^(09|\+639)\d{9}$/,
  'en-RW': /^(\+?250|0)?[7]\d{8}$/,
  'en-SG': /^(\+65)?[3689]\d{7}$/,
  'en-SL': /^(\+?232|0)\d{8}$/,
  'en-TZ': /^(\+?255|0)?[67]\d{8}$/,
  'en-UG': /^(\+?256|0)?[7]\d{8}$/,
  'en-US': /^((\+1|1)?( |-)?)?(\([2-9][0-9]{2}\)|[2-9][0-9]{2})( |-)?([2-9][0-9]{2}( |-)?[0-9]{4})$/,
  'en-ZA': /^(\+?27|0)\d{9}$/,
  'en-ZM': /^(\+?26)?0[79][567]\d{7}$/,
  'en-ZW': /^(\+263)[0-9]{9}$/,
  'en-BW': /^(\+?267)?(7[1-8]{1})\d{6}$/,
  'es-AR': /^\+?549(11|[2368]\d)\d{8}$/,
  'es-BO': /^(\+?591)?(6|7)\d{7}$/,
  'es-CO': /^(\+?57)?3(0(0|1|2|4|5)|1\d|2[0-4]|5(0|1))\d{7}$/,
  'es-CL': /^(\+?56|0)[2-9]\d{1}\d{7}$/,
  'es-CR': /^(\+506)?[2-8]\d{7}$/,
  'es-CU': /^(\+53|0053)?5\d{7}$/,
  'es-DO': /^(\+?1)?8[024]9\d{7}$/,
  'es-HN': /^(\+?504)?[9|8|3|2]\d{7}$/,
  'es-EC': /^(\+?593|0)([2-7]|9[2-9])\d{7}$/,
  'es-ES': /^(\+?34)?[6|7]\d{8}$/,
  'es-GT': /^(\+?502)?[2|6|7]\d{7}$/,
  'es-PE': /^(\+?51)?9\d{8}$/,
  'es-MX': /^(\+?52)?(1|01)?\d{10,11}$/,
  'es-NI': /^(\+?505)\d{7,8}$/,
  'es-PA': /^(\+?507)\d{7,8}$/,
  'es-PY': /^(\+?595|0)9[9876]\d{7}$/,
  'es-SV': /^(\+?503)?[67]\d{7}$/,
  'es-UY': /^(\+598|0)9[1-9][\d]{6}$/,
  'es-VE': /^(\+?58)?(2|4)\d{9}$/,
  'et-EE': /^(\+?372)?\s?(5|8[1-4])\s?([0-9]\s?){6,7}$/,
  'fa-IR': /^(\+?98[\-\s]?|0)9[0-39]\d[\-\s]?\d{3}[\-\s]?\d{4}$/,
  'fi-FI': /^(\+?358|0)\s?(4[0-6]|50)\s?(\d\s?){4,8}$/,
  'fj-FJ': /^(\+?679)?\s?\d{3}\s?\d{4}$/,
  'fo-FO': /^(\+?298)?\s?\d{2}\s?\d{2}\s?\d{2}$/,
  'fr-BF': /^(\+226|0)[67]\d{7}$/,
  'fr-BJ': /^(\+229)\d{8}$/,
  'fr-CD': /^(\+?243|0)?(8|9)\d{8}$/,
  'fr-CM': /^(\+?237)6[0-9]{8}$/,
  'fr-FR': /^(\+?33|0)[67]\d{8}$/,
  'fr-GF': /^(\+?594|0|00594)[67]\d{8}$/,
  'fr-GP': /^(\+?590|0|00590)[67]\d{8}$/,
  'fr-MQ': /^(\+?596|0|00596)[67]\d{8}$/,
  'fr-PF': /^(\+?689)?8[789]\d{6}$/,
  'fr-RE': /^(\+?262|0|00262)[67]\d{8}$/,
  'fr-WF': /^(\+681)?\d{6}$/,
  'he-IL': /^(\+972|0)([23489]|5[012345689]|77)[1-9]\d{6}$/,
  'hu-HU': /^(\+?36|06)(20|30|31|50|70)\d{7}$/,
  'id-ID': /^(\+?62|0)8(1[123456789]|2[1238]|3[1238]|5[12356789]|7[78]|9[56789]|8[123456789])([\s?|\d]{5,11})$/,
  'ir-IR': /^(\+98|0)?9\d{9}$/,
  'it-IT': /^(\+?39)?\s?3\d{2} ?\d{6,7}$/,
  'it-SM': /^((\+378)|(0549)|(\+390549)|(\+3780549))?6\d{5,9}$/,
  'ja-JP': /^(\+81[ \-]?(\(0\))?|0)[6789]0[ \-]?\d{4}[ \-]?\d{4}$/,
  'ka-GE': /^(\+?995)?(79\d{7}|5\d{8})$/,
  'kk-KZ': /^(\+?7|8)?7\d{9}$/,
  'kl-GL': /^(\+?299)?\s?\d{2}\s?\d{2}\s?\d{2}$/,
  'ko-KR': /^((\+?82)[ \-]?)?0?1([0|1|6|7|8|9]{1})[ \-]?\d{3,4}[ \-]?\d{4}$/,
  'ky-KG': /^(\+996\s?)?(22[0-9]|50[0-9]|55[0-9]|70[0-9]|75[0-9]|77[0-9]|880|990|995|996|997|998)\s?\d{3}\s?\d{3}$/,
  'lt-LT': /^(\+370|8)\d{8}$/,
  'lv-LV': /^(\+?371)2\d{7}$/,
  'mg-MG': /^((\+?261|0)(2|3)\d)?\d{7}$/,
  'mn-MN': /^(\+|00|011)?976(77|81|88|91|94|95|96|99)\d{6}$/,
  'my-MM': /^(\+?959|09|9)(2[5-7]|3[1-2]|4[0-5]|6[6-9]|7[5-9]|9[6-9])[0-9]{7}$/,
  'ms-MY': /^(\+?60|0)1(([0145](-|\s)?\d{7,8})|([236-9](-|\s)?\d{7}))$/,
  'mz-MZ': /^(\+?258)?8[234567]\d{7}$/,
  'nb-NO': /^(\+?47)?[49]\d{7}$/,
  'ne-NP': /^(\+?977)?9[78]\d{8}$/,
  'nl-BE': /^(\+?32|0)4\d{8}$/,
  'nl-NL': /^(((\+|00)?31\(0\))|((\+|00)?31)|0)6{1}\d{8}$/,
  'nl-AW': /^(\+)?297(56|59|64|73|74|99)\d{5}$/,
  'nn-NO': /^(\+?47)?[49]\d{7}$/,
  'pl-PL': /^(\+?48)? ?([5-8]\d|45) ?\d{3} ?\d{2} ?\d{2}$/,
  'pt-BR': /^((\+?55\ ?[1-9]{2}\ ?)|(\+?55\ ?\([1-9]{2}\)\ ?)|(0[1-9]{2}\ ?)|(\([1-9]{2}\)\ ?)|([1-9]{2}\ ?))((\d{4}\-?\d{4})|(9[1-9]{1}\d{3}\-?\d{4}))$/,
  'pt-PT': /^(\+?351)?9[1236]\d{7}$/,
  'pt-AO': /^(\+?244)?9\d{8}$/,
  'ro-MD': /^(\+?373|0)((6(0|1|2|6|7|8|9))|(7(6|7|8|9)))\d{6}$/,
  'ro-RO': /^(\+?40|0)\s?7\d{2}(\/|\s|\.|-)?\d{3}(\s|\.|-)?\d{3}$/,
  'ru-RU': /^(\+?7|8)?9\d{9}$/,
  'si-LK': /^(?:0|94|\+94)?(7(0|1|2|4|5|6|7|8)( |-)?)\d{7}$/,
  'sl-SI': /^(\+386\s?|0)(\d{1}\s?\d{3}\s?\d{2}\s?\d{2}|\d{2}\s?\d{3}\s?\d{3})$/,
  'sk-SK': /^(\+?421)? ?[1-9][0-9]{2} ?[0-9]{3} ?[0-9]{3}$/,
  'so-SO': /^(\+?252|0)((6[0-9])\d{7}|(7[1-9])\d{7})$/,
  'sq-AL': /^(\+355|0)6[2-9]\d{7}$/,
  'sr-RS': /^(\+3816|06)[- \d]{5,9}$/,
  'sv-SE': /^(\+?46|0)[\s\-]?7[\s\-]?[02369]([\s\-]?\d){7}$/,
  'tg-TJ': /^(\+?992)?[5][5]\d{7}$/,
  'th-TH': /^(\+66|66|0)\d{9}$/,
  'tr-TR': /^(\+?90|0)?5\d{9}$/,
  'tk-TM': /^(\+993|993|8)\d{8}$/,
  'uk-UA': /^(\+?38)?0(50|6[36-8]|7[357]|9[1-9])\d{7}$/,
  'uz-UZ': /^(\+?998)?(6[125-79]|7[1-69]|88|9\d)\d{7}$/,
  'vi-VN': /^((\+?84)|0)((3([2-9]))|(5([25689]))|(7([0|6-9]))|(8([1-9]))|(9([0-9])))([0-9]{7})$/,
  'zh-CN': /^((\+|00)86)?(1[3-9]|9[28])\d{9}$/,
  'zh-TW': /^(\+?886\-?|0)?9\d{8}$/,
  'dz-BT': /^(\+?975|0)?(17|16|77|02)\d{6}$/,
  'ar-YE': /^(((\+|00)9677|0?7)[0137]\d{7}|((\+|00)967|0)[1-7]\d{6})$/,
  'ar-EH': /^(\+?212|0)[\s\-]?(5288|5289)[\s\-]?\d{5}$/,
  'fa-AF': /^(\+93|0)?(2{1}[0-8]{1}|[3-5]{1}[0-4]{1})(\d{7})$/,
  'mk-MK': /^(\+?389|0)?((?:2[2-9]\d{6}|(?:3[1-4]|4[2-8])\d{6}|500\d{5}|5[2-9]\d{6}|7[0-9][2-9]\d{5}|8[1-9]\d{6}|800\d{5}|8009\d{4}))$/
};
/* eslint-enable max-len */

// aliases
phones['en-CA'] = phones['en-US'];
phones['fr-CA'] = phones['en-CA'];
phones['fr-BE'] = phones['nl-BE'];
phones['zh-HK'] = phones['en-HK'];
phones['zh-MO'] = phones['en-MO'];
phones['ga-IE'] = phones['en-IE'];
phones['fr-CH'] = phones['de-CH'];
phones['it-CH'] = phones['fr-CH'];
function isMobilePhone(str, locale, options) {
  (0, _assertString.default)(str);
  if (options && options.strictMode && !str.startsWith('+')) {
    return false;
  }
  if (Array.isArray(locale)) {
    return locale.some(function (key) {
      // https://github.com/gotwarlost/istanbul/blob/master/ignoring-code-for-coverage.md#ignoring-code-for-coverage-purposes
      // istanbul ignore else
      if (phones.hasOwnProperty(key)) {
        var phone = phones[key];
        if (phone.test(str)) {
          return true;
        }
      }
      return false;
    });
  } else if (locale in phones) {
    return phones[locale].test(str);
    // alias falsey locale as 'any'
  } else if (!locale || locale === 'any') {
    for (var key in phones) {
      // istanbul ignore else
      if (phones.hasOwnProperty(key)) {
        var phone = phones[key];
        if (phone.test(str)) {
          return true;
        }
      }
    }
    return false;
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
var locales = exports.locales = Object.keys(phones);

/***/ }),

/***/ 5259:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBIC;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isISO31661Alpha = __webpack_require__(8447);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// https://en.wikipedia.org/wiki/ISO_9362
var isBICReg = /^[A-Za-z]{6}[A-Za-z0-9]{2}([A-Za-z0-9]{3})?$/;
function isBIC(str) {
  (0, _assertString.default)(str);

  // toUpperCase() should be removed when a new major version goes out that changes
  // the regex to [A-Z] (per the spec).
  var countryCode = str.slice(4, 6).toUpperCase();
  if (!_isISO31661Alpha.CountryCodes.has(countryCode) && countryCode !== 'XK') {
    return false;
  }
  return isBICReg.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5366:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isVAT;
exports.vatMatchers = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var algorithms = _interopRequireWildcard(__webpack_require__(4834));
function _interopRequireWildcard(e, t) { if ("function" == typeof WeakMap) var r = new WeakMap(), n = new WeakMap(); return (_interopRequireWildcard = function _interopRequireWildcard(e, t) { if (!t && e && e.__esModule) return e; var o, i, f = { __proto__: null, default: e }; if (null === e || "object" != _typeof(e) && "function" != typeof e) return f; if (o = t ? n : r) { if (o.has(e)) return o.get(e); o.set(e, f); } for (var _t in e) "default" !== _t && {}.hasOwnProperty.call(e, _t) && ((i = (o = Object.defineProperty) && Object.getOwnPropertyDescriptor(e, _t)) && (i.get || i.set) ? o(f, _t, i) : f[_t] = e[_t]); return f; })(e, t); }
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var AU = function AU(str) {
  var match = str.match(/^(AU)?(\d{11})$/);
  if (!match) {
    return false;
  }
  // @see {@link https://abr.business.gov.au/Help/AbnFormat}
  var weights = [10, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19];
  str = str.replace(/^AU/, '');
  var ABN = (parseInt(str.slice(0, 1), 10) - 1).toString() + str.slice(1);
  var total = 0;
  for (var i = 0; i < 11; i++) {
    total += weights[i] * ABN.charAt(i);
  }
  return total !== 0 && total % 89 === 0;
};
var CH = function CH(str) {
  // @see {@link https://www.ech.ch/de/ech/ech-0097/5.2.0}
  var hasValidCheckNumber = function hasValidCheckNumber(digits) {
    var lastDigit = digits.pop(); // used as check number
    var weights = [5, 4, 3, 2, 7, 6, 5, 4];
    var calculatedCheckNumber = (11 - digits.reduce(function (acc, el, idx) {
      return acc + el * weights[idx];
    }, 0) % 11) % 11;
    return lastDigit === calculatedCheckNumber;
  };

  // @see {@link https://www.estv.admin.ch/estv/de/home/mehrwertsteuer/uid/mwst-uid-nummer.html}
  return /^(CHE[- ]?)?(\d{9}|(\d{3}\.\d{3}\.\d{3})|(\d{3} \d{3} \d{3})) ?(TVA|MWST|IVA)?$/.test(str) && hasValidCheckNumber(str.match(/\d/g).map(function (el) {
    return +el;
  }));
};
var PT = function PT(str) {
  var match = str.match(/^(PT)?(\d{9})$/);
  if (!match) {
    return false;
  }
  var tin = match[2];
  var checksum = 11 - algorithms.reverseMultiplyAndSum(tin.split('').slice(0, 8).map(function (a) {
    return parseInt(a, 10);
  }), 9) % 11;
  if (checksum > 9) {
    return parseInt(tin[8], 10) === 0;
  }
  return checksum === parseInt(tin[8], 10);
};
var vatMatchers = exports.vatMatchers = {
  /**
   * European Union VAT identification numbers
   */
  AT: function AT(str) {
    return /^(AT)?U\d{8}$/.test(str);
  },
  BE: function BE(str) {
    return /^(BE)?\d{10}$/.test(str);
  },
  BG: function BG(str) {
    return /^(BG)?\d{9,10}$/.test(str);
  },
  HR: function HR(str) {
    return /^(HR)?\d{11}$/.test(str);
  },
  CY: function CY(str) {
    return /^(CY)?\w{9}$/.test(str);
  },
  CZ: function CZ(str) {
    return /^(CZ)?\d{8,10}$/.test(str);
  },
  DK: function DK(str) {
    return /^(DK)?\d{8}$/.test(str);
  },
  EE: function EE(str) {
    return /^(EE)?\d{9}$/.test(str);
  },
  FI: function FI(str) {
    return /^(FI)?\d{8}$/.test(str);
  },
  FR: function FR(str) {
    return /^(FR)?\w{2}\d{9}$/.test(str);
  },
  DE: function DE(str) {
    return /^(DE)?\d{9}$/.test(str);
  },
  EL: function EL(str) {
    return /^(EL)?\d{9}$/.test(str);
  },
  HU: function HU(str) {
    return /^(HU)?\d{8}$/.test(str);
  },
  IE: function IE(str) {
    return /^(IE)?\d{7}\w{1}(W)?$/.test(str);
  },
  IT: function IT(str) {
    return /^(IT)?\d{11}$/.test(str);
  },
  LV: function LV(str) {
    return /^(LV)?\d{11}$/.test(str);
  },
  LT: function LT(str) {
    return /^(LT)?\d{9,12}$/.test(str);
  },
  LU: function LU(str) {
    return /^(LU)?\d{8}$/.test(str);
  },
  MT: function MT(str) {
    return /^(MT)?\d{8}$/.test(str);
  },
  NL: function NL(str) {
    return /^(NL)?\d{9}B\d{2}$/.test(str);
  },
  PL: function PL(str) {
    return /^(PL)?(\d{10}|(\d{3}-\d{3}-\d{2}-\d{2})|(\d{3}-\d{2}-\d{2}-\d{3}))$/.test(str);
  },
  PT: PT,
  RO: function RO(str) {
    return /^(RO)?\d{2,10}$/.test(str);
  },
  SK: function SK(str) {
    return /^(SK)?\d{10}$/.test(str);
  },
  SI: function SI(str) {
    return /^(SI)?\d{8}$/.test(str);
  },
  ES: function ES(str) {
    return /^(ES)?\w\d{7}[A-Z]$/.test(str);
  },
  SE: function SE(str) {
    return /^(SE)?\d{12}$/.test(str);
  },
  /**
   * VAT numbers of non-EU countries
   */
  AL: function AL(str) {
    return /^(AL)?\w{9}[A-Z]$/.test(str);
  },
  MK: function MK(str) {
    return /^(MK)?\d{13}$/.test(str);
  },
  AU: AU,
  BY: function BY(str) {
    return /^(\u0423\u041d\u041f )?\d{9}$/.test(str);
  },
  CA: function CA(str) {
    return /^(CA)?\d{9}$/.test(str);
  },
  IS: function IS(str) {
    return /^(IS)?\d{5,6}$/.test(str);
  },
  IN: function IN(str) {
    return /^(IN)?\d{15}$/.test(str);
  },
  ID: function ID(str) {
    return /^(ID)?(\d{15}|(\d{2}.\d{3}.\d{3}.\d{1}-\d{3}.\d{3}))$/.test(str);
  },
  IL: function IL(str) {
    return /^(IL)?\d{9}$/.test(str);
  },
  KZ: function KZ(str) {
    return /^(KZ)?\d{12}$/.test(str);
  },
  NZ: function NZ(str) {
    return /^(NZ)?\d{9}$/.test(str);
  },
  NG: function NG(str) {
    return /^(NG)?(\d{12}|(\d{8}-\d{4}))$/.test(str);
  },
  NO: function NO(str) {
    return /^(NO)?\d{9}MVA$/.test(str);
  },
  PH: function PH(str) {
    return /^(PH)?(\d{12}|\d{3} \d{3} \d{3} \d{3})$/.test(str);
  },
  RU: function RU(str) {
    return /^(RU)?(\d{10}|\d{12})$/.test(str);
  },
  SM: function SM(str) {
    return /^(SM)?\d{5}$/.test(str);
  },
  SA: function SA(str) {
    return /^(SA)?\d{15}$/.test(str);
  },
  RS: function RS(str) {
    return /^(RS)?\d{9}$/.test(str);
  },
  CH: CH,
  TR: function TR(str) {
    return /^(TR)?\d{10}$/.test(str);
  },
  UA: function UA(str) {
    return /^(UA)?\d{12}$/.test(str);
  },
  GB: function GB(str) {
    return /^GB((\d{3} \d{4} ([0-8][0-9]|9[0-6]))|(\d{9} \d{3})|(((GD[0-4])|(HA[5-9]))[0-9]{2}))$/.test(str);
  },
  UZ: function UZ(str) {
    return /^(UZ)?\d{9}$/.test(str);
  },
  /**
   * VAT numbers of Latin American countries
   */
  AR: function AR(str) {
    return /^(AR)?\d{11}$/.test(str);
  },
  BO: function BO(str) {
    return /^(BO)?\d{7}$/.test(str);
  },
  BR: function BR(str) {
    return /^(BR)?((\d{2}.\d{3}.\d{3}\/\d{4}-\d{2})|(\d{3}.\d{3}.\d{3}-\d{2}))$/.test(str);
  },
  CL: function CL(str) {
    return /^(CL)?\d{8}-\d{1}$/.test(str);
  },
  CO: function CO(str) {
    return /^(CO)?\d{10}$/.test(str);
  },
  CR: function CR(str) {
    return /^(CR)?\d{9,12}$/.test(str);
  },
  EC: function EC(str) {
    return /^(EC)?\d{13}$/.test(str);
  },
  SV: function SV(str) {
    return /^(SV)?\d{4}-\d{6}-\d{3}-\d{1}$/.test(str);
  },
  GT: function GT(str) {
    return /^(GT)?\d{7}-\d{1}$/.test(str);
  },
  HN: function HN(str) {
    return /^(HN)?$/.test(str);
  },
  MX: function MX(str) {
    return /^(MX)?\w{3,4}\d{6}\w{3}$/.test(str);
  },
  NI: function NI(str) {
    return /^(NI)?\d{3}-\d{6}-\d{4}\w{1}$/.test(str);
  },
  PA: function PA(str) {
    return /^(PA)?$/.test(str);
  },
  PY: function PY(str) {
    return /^(PY)?\d{6,8}-\d{1}$/.test(str);
  },
  PE: function PE(str) {
    return /^(PE)?\d{11}$/.test(str);
  },
  DO: function DO(str) {
    return /^(DO)?(\d{11}|(\d{3}-\d{7}-\d{1})|[1,4,5]{1}\d{8}|([1,4,5]{1})-\d{2}-\d{5}-\d{1})$/.test(str);
  },
  UY: function UY(str) {
    return /^(UY)?\d{12}$/.test(str);
  },
  VE: function VE(str) {
    return /^(VE)?[J,G,V,E]{1}-(\d{9}|(\d{8}-\d{1}))$/.test(str);
  }
};
function isVAT(str, countryCode) {
  (0, _assertString.default)(str);
  (0, _assertString.default)(countryCode);
  if (countryCode in vatMatchers) {
    return vatMatchers[countryCode](str);
  }
  throw new Error("Invalid country code: '".concat(countryCode, "'"));
}

/***/ }),

/***/ 5372:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIP;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
/**
11.3.  Examples

   The following addresses

             fe80::1234 (on the 1st link of the node)
             ff02::5678 (on the 5th link of the node)
             ff08::9abc (on the 10th organization of the node)

   would be represented as follows:

             fe80::1234%1
             ff02::5678%5
             ff08::9abc%10

   (Here we assume a natural translation from a zone index to the
   <zone_id> part, where the Nth zone of any scope is translated into
   "N".)

   If we use interface names as <zone_id>, those addresses could also be
   represented as follows:

            fe80::1234%ne0
            ff02::5678%pvc1.3
            ff08::9abc%interface10

   where the interface "ne0" belongs to the 1st link, "pvc1.3" belongs
   to the 5th link, and "interface10" belongs to the 10th organization.
 * * */
var IPv4SegmentFormat = '(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])';
var IPv4AddressFormat = "(".concat(IPv4SegmentFormat, "[.]){3}").concat(IPv4SegmentFormat);
var IPv4AddressRegExp = new RegExp("^".concat(IPv4AddressFormat, "$"));
var IPv6SegmentFormat = '(?:[0-9a-fA-F]{1,4})';
var IPv6AddressRegExp = new RegExp('^(' + "(?:".concat(IPv6SegmentFormat, ":){7}(?:").concat(IPv6SegmentFormat, "|:)|") + "(?:".concat(IPv6SegmentFormat, ":){6}(?:").concat(IPv4AddressFormat, "|:").concat(IPv6SegmentFormat, "|:)|") + "(?:".concat(IPv6SegmentFormat, ":){5}(?::").concat(IPv4AddressFormat, "|(:").concat(IPv6SegmentFormat, "){1,2}|:)|") + "(?:".concat(IPv6SegmentFormat, ":){4}(?:(:").concat(IPv6SegmentFormat, "){0,1}:").concat(IPv4AddressFormat, "|(:").concat(IPv6SegmentFormat, "){1,3}|:)|") + "(?:".concat(IPv6SegmentFormat, ":){3}(?:(:").concat(IPv6SegmentFormat, "){0,2}:").concat(IPv4AddressFormat, "|(:").concat(IPv6SegmentFormat, "){1,4}|:)|") + "(?:".concat(IPv6SegmentFormat, ":){2}(?:(:").concat(IPv6SegmentFormat, "){0,3}:").concat(IPv4AddressFormat, "|(:").concat(IPv6SegmentFormat, "){1,5}|:)|") + "(?:".concat(IPv6SegmentFormat, ":){1}(?:(:").concat(IPv6SegmentFormat, "){0,4}:").concat(IPv4AddressFormat, "|(:").concat(IPv6SegmentFormat, "){1,6}|:)|") + "(?::((?::".concat(IPv6SegmentFormat, "){0,5}:").concat(IPv4AddressFormat, "|(?::").concat(IPv6SegmentFormat, "){1,7}|:))") + ')(%[0-9a-zA-Z.]{1,})?$');
function isIP(ipAddress) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  (0, _assertString.default)(ipAddress);

  // accessing 'arguments' for backwards compatibility: isIP(ipAddress [, version])
  // eslint-disable-next-line prefer-rest-params
  var version = (_typeof(options) === 'object' ? options.version : arguments[1]) || '';
  if (!version) {
    return isIP(ipAddress, {
      version: 4
    }) || isIP(ipAddress, {
      version: 6
    });
  }
  if (version.toString() === '4') {
    return IPv4AddressRegExp.test(ipAddress);
  }
  if (version.toString() === '6') {
    return IPv6AddressRegExp.test(ipAddress);
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5467:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isRgbColor;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); } /* eslint-disable prefer-rest-params */
var rgbColor = /^rgb\((([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]),){2}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\)$/;
var rgbaColor = /^rgba\((([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]),){3}(0?\.\d\d?|1(\.0)?|0(\.0)?)\)$/;
var rgbColorPercent = /^rgb\((([0-9]%|[1-9][0-9]%|100%),){2}([0-9]%|[1-9][0-9]%|100%)\)$/;
var rgbaColorPercent = /^rgba\((([0-9]%|[1-9][0-9]%|100%),){3}(0?\.\d\d?|1(\.0)?|0(\.0)?)\)$/;
var startsWithRgb = /^rgba?/;
function isRgbColor(str, options) {
  (0, _assertString.default)(str);
  // default options to true for percent and false for spaces
  var allowSpaces = false;
  var includePercentValues = true;
  if (_typeof(options) !== 'object') {
    if (arguments.length >= 2) {
      includePercentValues = arguments[1];
    }
  } else {
    allowSpaces = options.allowSpaces !== undefined ? options.allowSpaces : allowSpaces;
    includePercentValues = options.includePercentValues !== undefined ? options.includePercentValues : includePercentValues;
  }
  if (allowSpaces) {
    // make sure it starts with continous rgba? without spaces before stripping
    if (!startsWithRgb.test(str)) {
      return false;
    }
    // strip all whitespace
    str = str.replace(/\s/g, '');
  }
  if (!includePercentValues) {
    return rgbColor.test(str) || rgbaColor.test(str);
  }
  return rgbColor.test(str) || rgbaColor.test(str) || rgbColorPercent.test(str) || rgbaColorPercent.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5577:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isUppercase;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isUppercase(str) {
  (0, _assertString.default)(str);
  return str === str.toUpperCase();
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5730:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = multilineRegexp;
/**
 * Build RegExp object from an array
 * of multiple/multi-line regexp parts
 *
 * @param {string[]} parts
 * @param {string} flags
 * @return {object} - RegExp object
 */
function multilineRegexp(parts, flags) {
  var regexpAsStringLiteral = parts.join('');
  return new RegExp(regexpAsStringLiteral, flags);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5748:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBtcAddress;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var bech32 = /^(bc1|tb1|bc1p|tb1p)[ac-hj-np-z02-9]{39,58}$/;
var base58 = /^(1|2|3|m)[A-HJ-NP-Za-km-z1-9]{25,39}$/;
function isBtcAddress(str) {
  (0, _assertString.default)(str);
  return bech32.test(str) || base58.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5751:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isJSON;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _includesArray = _interopRequireDefault(__webpack_require__(8644));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
var default_json_options = {
  allow_primitives: false
};
function isJSON(str, options) {
  (0, _assertString.default)(str);
  try {
    options = (0, _merge.default)(options, default_json_options);
    var primitives = [];
    if (options.allow_primitives) {
      primitives = [null, false, true];
    }
    var obj = JSON.parse(str);
    return (0, _includesArray.default)(primitives, obj) || !!obj && _typeof(obj) === 'object';
  } catch (e) {/* ignore */}
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5772:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = toString;
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function toString(input) {
  if (_typeof(input) === 'object' && input !== null) {
    if (typeof input.toString === 'function') {
      input = input.toString();
    } else {
      input = '[object Object]';
    }
  } else if (input === null || typeof input === 'undefined' || isNaN(input) && !input.length) {
    input = '';
  }
  return String(input);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5777:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.isFreightContainerID = void 0;
exports.isISO6346 = isISO6346;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// https://en.wikipedia.org/wiki/ISO_6346
// according to ISO6346 standard, checksum digit is mandatory for freight container but recommended
// for other container types (J and Z)
var isISO6346Str = /^[A-Z]{3}(U[0-9]{7})|([J,Z][0-9]{6,7})$/;
var isDigit = /^[0-9]$/;
function isISO6346(str) {
  (0, _assertString.default)(str);
  str = str.toUpperCase();
  if (!isISO6346Str.test(str)) return false;
  if (str.length === 11) {
    var sum = 0;
    for (var i = 0; i < str.length - 1; i++) {
      if (!isDigit.test(str[i])) {
        var convertedCode = void 0;
        var letterCode = str.charCodeAt(i) - 55;
        if (letterCode < 11) convertedCode = letterCode;else if (letterCode >= 11 && letterCode <= 20) convertedCode = 12 + letterCode % 11;else if (letterCode >= 21 && letterCode <= 30) convertedCode = 23 + letterCode % 21;else convertedCode = 34 + letterCode % 31;
        sum += convertedCode * Math.pow(2, i);
      } else sum += str[i] * Math.pow(2, i);
    }
    var checkSumDigit = sum % 11;
    if (checkSumDigit === 10) checkSumDigit = 0;
    return Number(str[str.length - 1]) === checkSumDigit;
  }
  return true;
}
var isFreightContainerID = exports.isFreightContainerID = isISO6346;

/***/ }),

/***/ 5830:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLatLong;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _merge = _interopRequireDefault(__webpack_require__(3610));
var _includesString = _interopRequireDefault(__webpack_require__(4636));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var lat = /^\(?[+-]?(90(\.0+)?|[1-8]?\d(\.\d+)?)$/;
var long = /^\s?[+-]?(180(\.0+)?|1[0-7]\d(\.\d+)?|\d{1,2}(\.\d+)?)\)?$/;
var latDMS = /^(([1-8]?\d)\D+([1-5]?\d|60)\D+([1-5]?\d|60)(\.\d+)?|90\D+0\D+0)\D+[NSns]?$/i;
var longDMS = /^\s*([1-7]?\d{1,2}\D+([1-5]?\d|60)\D+([1-5]?\d|60)(\.\d+)?|180\D+0\D+0)\D+[EWew]?$/i;
var defaultLatLongOptions = {
  checkDMS: false
};
function isLatLong(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, defaultLatLongOptions);
  if (!(0, _includesString.default)(str, ',')) return false;
  var pair = str.split(',');
  if (pair[0].startsWith('(') && !pair[1].endsWith(')') || pair[1].endsWith(')') && !pair[0].startsWith('(')) return false;
  if (options.checkDMS) {
    return latDMS.test(pair[0]) && longDMS.test(pair[1]);
  }
  return lat.test(pair[0]) && long.test(pair[1]);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 5926:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isDecimal;
var _merge = _interopRequireDefault(__webpack_require__(3610));
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _includesArray = _interopRequireDefault(__webpack_require__(8644));
var _alpha = __webpack_require__(3237);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function decimalRegExp(options) {
  var regExp = new RegExp("^[-+]?([0-9]+)?(\\".concat(_alpha.decimal[options.locale], "[0-9]{").concat(options.decimal_digits, "})").concat(options.force_decimal ? '' : '?', "$"));
  return regExp;
}
var default_decimal_options = {
  force_decimal: false,
  decimal_digits: '1,',
  locale: 'en-US'
};
var blacklist = ['', '-', '+'];
function isDecimal(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, default_decimal_options);
  if (options.locale in _alpha.decimal) {
    return !(0, _includesArray.default)(blacklist, str.replace(/ /g, '')) && decimalRegExp(options).test(str);
  }
  throw new Error("Invalid locale '".concat(options.locale, "'"));
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6084:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isInt;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _nullUndefinedCheck = _interopRequireDefault(__webpack_require__(821));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var int = /^(?:[-+]?(?:0|[1-9][0-9]*))$/;
var intLeadingZeroes = /^[-+]?[0-9]+$/;
function isInt(str, options) {
  (0, _assertString.default)(str);
  options = options || {};

  // Get the regex to use for testing, based on whether
  // leading zeroes are allowed or not.
  var regex = options.allow_leading_zeroes === false ? int : intLeadingZeroes;

  // Check min/max/lt/gt
  var minCheckPassed = !options.hasOwnProperty('min') || (0, _nullUndefinedCheck.default)(options.min) || str >= options.min;
  var maxCheckPassed = !options.hasOwnProperty('max') || (0, _nullUndefinedCheck.default)(options.max) || str <= options.max;
  var ltCheckPassed = !options.hasOwnProperty('lt') || (0, _nullUndefinedCheck.default)(options.lt) || str < options.lt;
  var gtCheckPassed = !options.hasOwnProperty('gt') || (0, _nullUndefinedCheck.default)(options.gt) || str > options.gt;
  return regex.test(str) && minCheckPassed && maxCheckPassed && ltCheckPassed && gtCheckPassed;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6169:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISO8601;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/* eslint-disable max-len */
// from http://goo.gl/0ejHHW
var iso8601 = /^([\+-]?\d{4}(?!\d{2}\b))((-?)((0[1-9]|1[0-2])(\3([12]\d|0[1-9]|3[01]))?|W([0-4]\d|5[0-3])(-?[1-7])?|(00[1-9]|0[1-9]\d|[12]\d{2}|3([0-5]\d|6[1-6])))([T\s]((([01]\d|2[0-3])((:?)[0-5]\d)?|24:?00)([\.,]\d+(?!:))?)?(\17[0-5]\d([\.,]\d+)?)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?)?)?$/;
// same as above, except with a strict 'T' separator between date and time
var iso8601StrictSeparator = /^([\+-]?\d{4}(?!\d{2}\b))((-?)((0[1-9]|1[0-2])(\3([12]\d|0[1-9]|3[01]))?|W([0-4]\d|5[0-3])(-?[1-7])?|(00[1-9]|0[1-9]\d|[12]\d{2}|3([0-5]\d|6[1-6])))([T]((([01]\d|2[0-3])((:?)[0-5]\d)?|24:?00)([\.,]\d+(?!:))?)?(\17[0-5]\d([\.,]\d+)?)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?)?)?$/;
/* eslint-enable max-len */
var isValidDate = function isValidDate(str) {
  // str must have passed the ISO8601 check
  // this check is meant to catch invalid dates
  // like 2009-02-31
  // first check for ordinal dates
  var ordinalMatch = str.match(/^(\d{4})-?(\d{3})([ T]{1}\.*|$)/);
  if (ordinalMatch) {
    var oYear = Number(ordinalMatch[1]);
    var oDay = Number(ordinalMatch[2]);
    // if is leap year
    if (oYear % 4 === 0 && oYear % 100 !== 0 || oYear % 400 === 0) return oDay <= 366;
    return oDay <= 365;
  }
  var match = str.match(/(\d{4})-?(\d{0,2})-?(\d*)/).map(Number);
  var year = match[1];
  var month = match[2];
  var day = match[3];
  var monthString = month ? "0".concat(month).slice(-2) : month;
  var dayString = day ? "0".concat(day).slice(-2) : day;

  // create a date object and compare
  var d = new Date("".concat(year, "-").concat(monthString || '01', "-").concat(dayString || '01'));
  if (month && day) {
    return d.getUTCFullYear() === year && d.getUTCMonth() + 1 === month && d.getUTCDate() === day;
  }
  return true;
};
function isISO8601(str) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  (0, _assertString.default)(str);
  var check = options.strictSeparator ? iso8601StrictSeparator.test(str) : iso8601.test(str);
  if (check && options.strict) return isValidDate(str);
  return check;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6255:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isByteLength;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
/* eslint-disable prefer-rest-params */
function isByteLength(str, options) {
  (0, _assertString.default)(str);
  var min;
  var max;
  if (_typeof(options) === 'object') {
    min = options.min || 0;
    max = options.max;
  } else {
    // backwards compatibility: isByteLength(str, min [, max])
    min = arguments[1];
    max = arguments[2];
  }
  var len = encodeURI(str).split(/%..|./).length - 1;
  return len >= min && (typeof max === 'undefined' || len <= max);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6529:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isAlpha;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _alpha = __webpack_require__(3237);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isAlpha(_str) {
  var locale = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'en-US';
  var options = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};
  (0, _assertString.default)(_str);
  var str = _str;
  var ignore = options.ignore;
  if (ignore) {
    if (ignore instanceof RegExp) {
      str = str.replace(ignore, '');
    } else if (typeof ignore === 'string') {
      str = str.replace(new RegExp("[".concat(ignore.replace(/[-[\]{}()*+?.,\\^$|#\\s]/g, '\\$&'), "]"), 'g'), ''); // escape regex for ignore
    } else {
      throw new Error('ignore should be instance of a String or RegExp');
    }
  }
  if (locale in _alpha.alpha) {
    return _alpha.alpha[locale].test(str);
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
var locales = exports.locales = Object.keys(_alpha.alpha);

/***/ }),

/***/ 6617:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBase58;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// Accepted chars - 123456789ABCDEFGH JKLMN PQRSTUVWXYZabcdefghijk mnopqrstuvwxyz
var base58Reg = /^[A-HJ-NP-Za-km-z1-9]*$/;
function isBase58(str) {
  (0, _assertString.default)(str);
  if (base58Reg.test(str)) {
    return true;
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6658:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBefore;
var _toDate = _interopRequireDefault(__webpack_require__(3752));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function isBefore(date, options) {
  // For backwards compatibility:
  // isBefore(str [, date]), i.e. `options` could be used as argument for the legacy `date`
  var comparisonDate = (_typeof(options) === 'object' ? options.comparisonDate : options) || Date().toString();
  var comparison = (0, _toDate.default)(comparisonDate);
  var original = (0, _toDate.default)(date);
  return !!(original && comparison && original < comparison);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 6782:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isCurrency;
var _merge = _interopRequireDefault(__webpack_require__(3610));
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function currencyRegex(options) {
  var decimal_digits = "\\d{".concat(options.digits_after_decimal[0], "}");
  options.digits_after_decimal.forEach(function (digit, index) {
    if (index !== 0) decimal_digits = "".concat(decimal_digits, "|\\d{").concat(digit, "}");
  });
  var symbol = "(".concat(options.symbol.replace(/\W/, function (m) {
      return "\\".concat(m);
    }), ")").concat(options.require_symbol ? '' : '?'),
    negative = '-?',
    whole_dollar_amount_without_sep = '[1-9]\\d*',
    whole_dollar_amount_with_sep = "[1-9]\\d{0,2}(\\".concat(options.thousands_separator, "\\d{3})*"),
    valid_whole_dollar_amounts = ['0', whole_dollar_amount_without_sep, whole_dollar_amount_with_sep],
    whole_dollar_amount = "(".concat(valid_whole_dollar_amounts.join('|'), ")?"),
    decimal_amount = "(\\".concat(options.decimal_separator, "(").concat(decimal_digits, "))").concat(options.require_decimal ? '' : '?');
  var pattern = whole_dollar_amount + (options.allow_decimal || options.require_decimal ? decimal_amount : '');

  // default is negative sign before symbol, but there are two other options (besides parens)
  if (options.allow_negatives && !options.parens_for_negatives) {
    if (options.negative_sign_after_digits) {
      pattern += negative;
    } else if (options.negative_sign_before_digits) {
      pattern = negative + pattern;
    }
  }

  // South African Rand, for example, uses R 123 (space) and R-123 (no space)
  if (options.allow_negative_sign_placeholder) {
    pattern = "( (?!\\-))?".concat(pattern);
  } else if (options.allow_space_after_symbol) {
    pattern = " ?".concat(pattern);
  } else if (options.allow_space_after_digits) {
    pattern += '( (?!$))?';
  }
  if (options.symbol_after_digits) {
    pattern += symbol;
  } else {
    pattern = symbol + pattern;
  }
  if (options.allow_negatives) {
    if (options.parens_for_negatives) {
      pattern = "(\\(".concat(pattern, "\\)|").concat(pattern, ")");
    } else if (!(options.negative_sign_before_digits || options.negative_sign_after_digits)) {
      pattern = negative + pattern;
    }
  }

  // ensure there's a dollar and/or decimal amount, and that
  // it doesn't start with a space or a negative sign followed by a space
  return new RegExp("^(?!-? )(?=.*\\d)".concat(pattern, "$"));
}
var default_currency_options = {
  symbol: '$',
  require_symbol: false,
  allow_space_after_symbol: false,
  symbol_after_digits: false,
  allow_negatives: true,
  parens_for_negatives: false,
  negative_sign_before_digits: false,
  negative_sign_after_digits: false,
  allow_negative_sign_placeholder: false,
  thousands_separator: ',',
  decimal_separator: '.',
  allow_decimal: true,
  require_decimal: false,
  digits_after_decimal: [2],
  allow_space_after_digits: false
};
function isCurrency(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, default_currency_options);
  return currencyRegex(options).test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7071:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLocale;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/*
  = 3ALPHA              ; selected ISO 639 codes
    *2("-" 3ALPHA)      ; permanently reserved
 */
var extlang = '([A-Za-z]{3}(-[A-Za-z]{3}){0,2})';

/*
  = 2*3ALPHA            ; shortest ISO 639 code
    ["-" extlang]       ; sometimes followed by
                        ; extended language subtags
  / 4ALPHA              ; or reserved for future use
  / 5*8ALPHA            ; or registered language subtag
 */
var language = "(([a-zA-Z]{2,3}(-".concat(extlang, ")?)|([a-zA-Z]{5,8}))");

/*
  = 4ALPHA              ; ISO 15924 code
 */
var script = '([A-Za-z]{4})';

/*
  = 2ALPHA              ; ISO 3166-1 code
  / 3DIGIT              ; UN M.49 code
 */
var region = '([A-Za-z]{2}|\\d{3})';

/*
  = 5*8alphanum         ; registered variants
  / (DIGIT 3alphanum)
 */
var variant = '([A-Za-z0-9]{5,8}|(\\d[A-Z-a-z0-9]{3}))';

/*
  = DIGIT               ; 0 - 9
  / %x41-57             ; A - W
  / %x59-5A             ; Y - Z
  / %x61-77             ; a - w
  / %x79-7A             ; y - z
 */
var singleton = '(\\d|[A-W]|[Y-Z]|[a-w]|[y-z])';

/*
  = singleton 1*("-" (2*8alphanum))
                        ; Single alphanumerics
                        ; "x" reserved for private use
 */
var extension = "(".concat(singleton, "(-[A-Za-z0-9]{2,8})+)");

/*
  = "x" 1*("-" (1*8alphanum))
 */
var privateuse = '(x(-[A-Za-z0-9]{1,8})+)';

// irregular tags do not match the 'langtag' production and would not
// otherwise be considered 'well-formed'. These tags are all valid, but
// most are deprecated in favor of more modern subtags or subtag combination

var irregular = '((en-GB-oed)|(i-ami)|(i-bnn)|(i-default)|(i-enochian)|' + '(i-hak)|(i-klingon)|(i-lux)|(i-mingo)|(i-navajo)|(i-pwn)|(i-tao)|' + '(i-tay)|(i-tsu)|(sgn-BE-FR)|(sgn-BE-NL)|(sgn-CH-DE))';

// regular tags match the 'langtag' production, but their subtags are not
// extended language or variant subtags: their meaning is defined by
// their registration and all of these are deprecated in favor of a more
// modern subtag or sequence of subtags

var regular = '((art-lojban)|(cel-gaulish)|(no-bok)|(no-nyn)|(zh-guoyu)|' + '(zh-hakka)|(zh-min)|(zh-min-nan)|(zh-xiang))';

/*
  = irregular           ; non-redundant tags registered
  / regular             ; during the RFC 3066 era

 */
var grandfathered = "(".concat(irregular, "|").concat(regular, ")");

/*
  RFC 5646 defines delimitation of subtags via a hyphen:

      "Subtag" refers to a specific section of a tag, delimited by a
      hyphen, such as the subtags 'zh', 'Hant', and 'CN' in the tag "zh-
      Hant-CN".  Examples of subtags in this document are enclosed in
      single quotes ('Hant')

  However, we need to add "_" to maintain the existing behaviour.
 */
var delimiter = '(-|_)';

/*
  = language
    ["-" script]
    ["-" region]
    *("-" variant)
    *("-" extension)
    ["-" privateuse]
 */
var langtag = "".concat(language, "(").concat(delimiter).concat(script, ")?(").concat(delimiter).concat(region, ")?(").concat(delimiter).concat(variant, ")*(").concat(delimiter).concat(extension, ")*(").concat(delimiter).concat(privateuse, ")?");

/*
  Regex implementation based on BCP RFC 5646
  Tags for Identifying Languages
  https://www.rfc-editor.org/rfc/rfc5646.html
 */
var languageTagRegex = new RegExp("(^".concat(privateuse, "$)|(^").concat(grandfathered, "$)|(^").concat(langtag, "$)"));
function isLocale(str) {
  (0, _assertString.default)(str);
  return languageTagRegex.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7086:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isHSL;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var hslComma = /^hsla?\(((\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?))(deg|grad|rad|turn)?(,(\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?)%){2}(,((\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?)%?))?\)$/i;
var hslSpace = /^hsla?\(((\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?))(deg|grad|rad|turn)?(\s(\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?)%){2}\s?(\/\s((\+|\-)?([0-9]+(\.[0-9]+)?(e(\+|\-)?[0-9]+)?|\.[0-9]+(e(\+|\-)?[0-9]+)?)%?)\s?)?\)$/i;
function isHSL(str) {
  (0, _assertString.default)(str);

  // Strip duplicate spaces before calling the validation regex (See  #1598 for more info)
  var strippedStr = str.replace(/\s+/g, ' ').replace(/\s?(hsla?\(|\)|,)\s?/ig, '$1');
  if (strippedStr.indexOf(',') !== -1) {
    return hslComma.test(strippedStr);
  }
  return hslSpace.test(strippedStr);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7115:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIPRange;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isIP = _interopRequireDefault(__webpack_require__(5372));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var subnetMaybe = /^\d{1,3}$/;
var v4Subnet = 32;
var v6Subnet = 128;
function isIPRange(str) {
  var version = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : '';
  (0, _assertString.default)(str);
  var parts = str.split('/');

  // parts[0] -> ip, parts[1] -> subnet
  if (parts.length !== 2) {
    return false;
  }
  if (!subnetMaybe.test(parts[1])) {
    return false;
  }

  // Disallow preceding 0 i.e. 01, 02, ...
  if (parts[1].length > 1 && parts[1].startsWith('0')) {
    return false;
  }
  var isValidIP = (0, _isIP.default)(parts[0], version);
  if (!isValidIP) {
    return false;
  }

  // Define valid subnet according to IP's version
  var expectedSubnet = null;
  switch (String(version)) {
    case '4':
      expectedSubnet = v4Subnet;
      break;
    case '6':
      expectedSubnet = v6Subnet;
      break;
    default:
      expectedSubnet = (0, _isIP.default)(parts[0], '6') ? v6Subnet : v4Subnet;
  }
  return parts[1] <= expectedSubnet && parts[1] >= 0;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7179:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isStrongPassword;
var _merge = _interopRequireDefault(__webpack_require__(3610));
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var upperCaseRegex = /^[A-Z]$/;
var lowerCaseRegex = /^[a-z]$/;
var numberRegex = /^[0-9]$/;
var symbolRegex = /^[-#!$@\u00a3%^&*()_+|~=`{}\[\]:";'<>?,.\/\\ ]$/;
var defaultOptions = {
  minLength: 8,
  minLowercase: 1,
  minUppercase: 1,
  minNumbers: 1,
  minSymbols: 1,
  returnScore: false,
  pointsPerUnique: 1,
  pointsPerRepeat: 0.5,
  pointsForContainingLower: 10,
  pointsForContainingUpper: 10,
  pointsForContainingNumber: 10,
  pointsForContainingSymbol: 10
};

/* Counts number of occurrences of each char in a string
 * could be moved to util/ ?
*/
function countChars(str) {
  var result = {};
  Array.from(str).forEach(function (char) {
    var curVal = result[char];
    if (curVal) {
      result[char] += 1;
    } else {
      result[char] = 1;
    }
  });
  return result;
}

/* Return information about a password */
function analyzePassword(password) {
  var charMap = countChars(password);
  var analysis = {
    length: password.length,
    uniqueChars: Object.keys(charMap).length,
    uppercaseCount: 0,
    lowercaseCount: 0,
    numberCount: 0,
    symbolCount: 0
  };
  Object.keys(charMap).forEach(function (char) {
    /* istanbul ignore else */
    if (upperCaseRegex.test(char)) {
      analysis.uppercaseCount += charMap[char];
    } else if (lowerCaseRegex.test(char)) {
      analysis.lowercaseCount += charMap[char];
    } else if (numberRegex.test(char)) {
      analysis.numberCount += charMap[char];
    } else if (symbolRegex.test(char)) {
      analysis.symbolCount += charMap[char];
    }
  });
  return analysis;
}
function scorePassword(analysis, scoringOptions) {
  var points = 0;
  points += analysis.uniqueChars * scoringOptions.pointsPerUnique;
  points += (analysis.length - analysis.uniqueChars) * scoringOptions.pointsPerRepeat;
  if (analysis.lowercaseCount > 0) {
    points += scoringOptions.pointsForContainingLower;
  }
  if (analysis.uppercaseCount > 0) {
    points += scoringOptions.pointsForContainingUpper;
  }
  if (analysis.numberCount > 0) {
    points += scoringOptions.pointsForContainingNumber;
  }
  if (analysis.symbolCount > 0) {
    points += scoringOptions.pointsForContainingSymbol;
  }
  return points;
}
function isStrongPassword(str) {
  var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : null;
  (0, _assertString.default)(str);
  var analysis = analyzePassword(str);
  options = (0, _merge.default)(options || {}, defaultOptions);
  if (options.returnScore) {
    return scorePassword(analysis, options);
  }
  return analysis.length >= options.minLength && analysis.lowercaseCount >= options.minLowercase && analysis.uppercaseCount >= options.minUppercase && analysis.numberCount >= options.minNumbers && analysis.symbolCount >= options.minSymbols;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7349:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMagnetURI;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var magnetURIComponent = /(?:^magnet:\?|[^?&]&)xt(?:\.1)?=urn:(?:(?:aich|bitprint|btih|ed2k|ed2khash|kzhash|md5|sha1|tree:tiger):[a-z0-9]{32}(?:[a-z0-9]{8})?|btmh:1220[a-z0-9]{64})(?:$|&)/i;
function isMagnetURI(url) {
  (0, _assertString.default)(url);
  if (url.indexOf('magnet:?') !== 0) {
    return false;
  }
  return magnetURIComponent.test(url);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7612:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLowercase;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isLowercase(str) {
  (0, _assertString.default)(str);
  return str === str.toLowerCase();
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7658:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isFQDN;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var default_fqdn_options = {
  require_tld: true,
  allow_underscores: false,
  allow_trailing_dot: false,
  allow_numeric_tld: false,
  allow_wildcard: false,
  ignore_max_length: false
};
function isFQDN(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, default_fqdn_options);

  /* Remove the optional trailing dot before checking validity */
  if (options.allow_trailing_dot && str[str.length - 1] === '.') {
    str = str.substring(0, str.length - 1);
  }

  /* Remove the optional wildcard before checking validity */
  if (options.allow_wildcard === true && str.indexOf('*.') === 0) {
    str = str.substring(2);
  }
  var parts = str.split('.');
  var tld = parts[parts.length - 1];
  if (options.require_tld) {
    // disallow fqdns without tld
    if (parts.length < 2) {
      return false;
    }
    if (!options.allow_numeric_tld && !/^([a-z\u00A1-\u00A8\u00AA-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]{2,}|xn[a-z0-9-]{2,})$/i.test(tld)) {
      return false;
    }

    // disallow spaces
    if (/\s/.test(tld)) {
      return false;
    }
  }

  // reject numeric TLDs
  if (!options.allow_numeric_tld && /^\d+$/.test(tld)) {
    return false;
  }
  return parts.every(function (part) {
    if (part.length > 63 && !options.ignore_max_length) {
      return false;
    }
    if (!/^[a-z_\u00a1-\uffff0-9-]+$/i.test(part)) {
      return false;
    }

    // disallow full-width chars
    if (/[\uff01-\uff5e]/.test(part)) {
      return false;
    }

    // disallow parts starting or ending with hyphen
    if (/^-|-$/.test(part)) {
      return false;
    }
    if (!options.allow_underscores && /_/.test(part)) {
      return false;
    }
    return true;
  });
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7673:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBase32;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var base32 = /^[A-Z2-7]+=*$/;
var crockfordBase32 = /^[A-HJKMNP-TV-Z0-9]+$/;
var defaultBase32Options = {
  crockford: false
};
function isBase32(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, defaultBase32Options);
  if (options.crockford) {
    return crockfordBase32.test(str);
  }
  var len = str.length;
  if (len % 8 === 0 && base32.test(str)) {
    return true;
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7677:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = unescape;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function unescape(str) {
  (0, _assertString.default)(str);
  return str.replace(/&quot;/g, '"').replace(/&#x27;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&#x2F;/g, '/').replace(/&#x5C;/g, '\\').replace(/&#96;/g, '`').replace(/&amp;/g, '&');
  // &amp; replacement has to be the last one to prevent
  // bugs with intermediate strings containing escape sequences
  // See: https://github.com/validatorjs/validator.js/issues/1827
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7717:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isEAN;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/**
 * The most commonly used EAN standard is
 * the thirteen-digit EAN-13, while the
 * less commonly used 8-digit EAN-8 barcode was
 * introduced for use on small packages.
 * Also EAN/UCC-14 is used for Grouping of individual
 * trade items above unit level(Intermediate, Carton or Pallet).
 * For more info about EAN-14 checkout: https://www.gtin.info/itf-14-barcodes/
 * EAN consists of:
 * GS1 prefix, manufacturer code, product code and check digit
 * Reference: https://en.wikipedia.org/wiki/International_Article_Number
 * Reference: https://www.gtin.info/
 */

/**
 * Define EAN Lengths; 8 for EAN-8; 13 for EAN-13; 14 for EAN-14
 * and Regular Expression for valid EANs (EAN-8, EAN-13, EAN-14),
 * with exact numeric matching of 8 or 13 or 14 digits [0-9]
 */
var LENGTH_EAN_8 = 8;
var LENGTH_EAN_14 = 14;
var validEanRegex = /^(\d{8}|\d{13}|\d{14})$/;

/**
 * Get position weight given:
 * EAN length and digit index/position
 *
 * @param {number} length
 * @param {number} index
 * @return {number}
 */
function getPositionWeightThroughLengthAndIndex(length, index) {
  if (length === LENGTH_EAN_8 || length === LENGTH_EAN_14) {
    return index % 2 === 0 ? 3 : 1;
  }
  return index % 2 === 0 ? 1 : 3;
}

/**
 * Calculate EAN Check Digit
 * Reference: https://en.wikipedia.org/wiki/International_Article_Number#Calculation_of_checksum_digit
 *
 * @param {string} ean
 * @return {number}
 */
function calculateCheckDigit(ean) {
  var checksum = ean.slice(0, -1).split('').map(function (char, index) {
    return Number(char) * getPositionWeightThroughLengthAndIndex(ean.length, index);
  }).reduce(function (acc, partialSum) {
    return acc + partialSum;
  }, 0);
  var remainder = 10 - checksum % 10;
  return remainder < 10 ? remainder : 0;
}

/**
 * Check if string is valid EAN:
 * Matches EAN-8/EAN-13/EAN-14 regex
 * Has valid check digit.
 *
 * @param {string} str
 * @return {boolean}
 */
function isEAN(str) {
  (0, _assertString.default)(str);
  var actualCheckDigit = Number(str.slice(-1));
  return validEanRegex.test(str) && actualCheckDigit === calculateCheckDigit(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7741:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isTaxID;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var algorithms = _interopRequireWildcard(__webpack_require__(4834));
var _isDate = _interopRequireDefault(__webpack_require__(9013));
function _interopRequireWildcard(e, t) { if ("function" == typeof WeakMap) var r = new WeakMap(), n = new WeakMap(); return (_interopRequireWildcard = function _interopRequireWildcard(e, t) { if (!t && e && e.__esModule) return e; var o, i, f = { __proto__: null, default: e }; if (null === e || "object" != _typeof(e) && "function" != typeof e) return f; if (o = t ? n : r) { if (o.has(e)) return o.get(e); o.set(e, f); } for (var _t in e) "default" !== _t && {}.hasOwnProperty.call(e, _t) && ((i = (o = Object.defineProperty) && Object.getOwnPropertyDescriptor(e, _t)) && (i.get || i.set) ? o(f, _t, i) : f[_t] = e[_t]); return f; })(e, t); }
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _toConsumableArray(r) { return _arrayWithoutHoles(r) || _iterableToArray(r) || _unsupportedIterableToArray(r) || _nonIterableSpread(); }
function _nonIterableSpread() { throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _iterableToArray(r) { if ("undefined" != typeof Symbol && null != r[Symbol.iterator] || null != r["@@iterator"]) return Array.from(r); }
function _arrayWithoutHoles(r) { if (Array.isArray(r)) return _arrayLikeToArray(r); }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
/**
 * TIN Validation
 * Validates Tax Identification Numbers (TINs) from the US, EU member states and the United Kingdom.
 *
 * EU-UK:
 * National TIN validity is calculated using public algorithms as made available by DG TAXUD.
 *
 * See `https://ec.europa.eu/taxation_customs/tin/specs/FS-TIN%20Algorithms-Public.docx` for more information.
 *
 * US:
 * An Employer Identification Number (EIN), also known as a Federal Tax Identification Number,
 *  is used to identify a business entity.
 *
 * NOTES:
 *  - Prefix 47 is being reserved for future use
 *  - Prefixes 26, 27, 45, 46 and 47 were previously assigned by the Philadelphia campus.
 *
 * See `http://www.irs.gov/Businesses/Small-Businesses-&-Self-Employed/How-EINs-are-Assigned-and-Valid-EIN-Prefixes`
 * for more information.
 */

// Locale functions

/*
 * bg-BG validation function
 * (Edinen gra\u017edanski nomer (EGN/\u0415\u0413\u041d), persons only)
 * Checks if birth date (first six digits) is valid and calculates check (last) digit
 */
function bgBgCheck(tin) {
  // Extract full year, normalize month and check birth date validity
  var century_year = tin.slice(0, 2);
  var month = parseInt(tin.slice(2, 4), 10);
  if (month > 40) {
    month -= 40;
    century_year = "20".concat(century_year);
  } else if (month > 20) {
    month -= 20;
    century_year = "18".concat(century_year);
  } else {
    century_year = "19".concat(century_year);
  }
  if (month < 10) {
    month = "0".concat(month);
  }
  var date = "".concat(century_year, "/").concat(month, "/").concat(tin.slice(4, 6));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });

  // Calculate checksum by multiplying digits with fixed values
  var multip_lookup = [2, 4, 8, 5, 10, 9, 7, 3, 6];
  var checksum = 0;
  for (var i = 0; i < multip_lookup.length; i++) {
    checksum += digits[i] * multip_lookup[i];
  }
  checksum = checksum % 11 === 10 ? 0 : checksum % 11;
  return checksum === digits[9];
}

/**
 * Check if an input is a valid Canadian SIN (Social Insurance Number)
 *
 * The Social Insurance Number (SIN) is a 9 digit number that
 * you need to work in Canada or to have access to government programs and benefits.
 *
 * https://en.wikipedia.org/wiki/Social_Insurance_Number
 * https://www.canada.ca/en/employment-social-development/services/sin.html
 * https://www.codercrunch.com/challenge/819302488/sin-validator
 *
 * @param {string} input
 * @return {boolean}
 */
function isCanadianSIN(input) {
  var digitsArray = input.split('');
  var even = digitsArray.filter(function (_, idx) {
    return idx % 2;
  }).map(function (i) {
    return Number(i) * 2;
  }).join('').split('');
  var total = digitsArray.filter(function (_, idx) {
    return !(idx % 2);
  }).concat(even).map(function (i) {
    return Number(i);
  }).reduce(function (acc, cur) {
    return acc + cur;
  });
  return total % 10 === 0;
}

/*
 * cs-CZ validation function
 * (Rodn\u00e9 \u010d\u00edslo (R\u010c), persons only)
 * Checks if birth date (first six digits) is valid and divisibility by 11
 * Material not in DG TAXUD document sourced from:
 * -`https://lorenc.info/3MA381/overeni-spravnosti-rodneho-cisla.htm`
 * -`https://www.mvcr.cz/clanek/rady-a-sluzby-dokumenty-rodne-cislo.aspx`
 */
function csCzCheck(tin) {
  tin = tin.replace(/\W/, '');

  // Extract full year from TIN length
  var full_year = parseInt(tin.slice(0, 2), 10);
  if (tin.length === 10) {
    if (full_year < 54) {
      full_year = "20".concat(full_year);
    } else {
      full_year = "19".concat(full_year);
    }
  } else {
    if (tin.slice(6) === '000') {
      return false;
    } // Three-zero serial not assigned before 1954
    if (full_year < 54) {
      full_year = "19".concat(full_year);
    } else {
      return false; // No 18XX years seen in any of the resources
    }
  }
  // Add missing zero if needed
  if (full_year.length === 3) {
    full_year = [full_year.slice(0, 2), '0', full_year.slice(2)].join('');
  }

  // Extract month from TIN and normalize
  var month = parseInt(tin.slice(2, 4), 10);
  if (month > 50) {
    month -= 50;
  }
  if (month > 20) {
    // Month-plus-twenty was only introduced in 2004
    if (parseInt(full_year, 10) < 2004) {
      return false;
    }
    month -= 20;
  }
  if (month < 10) {
    month = "0".concat(month);
  }

  // Check date validity
  var date = "".concat(full_year, "/").concat(month, "/").concat(tin.slice(4, 6));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Verify divisibility by 11
  if (tin.length === 10) {
    if (parseInt(tin, 10) % 11 !== 0) {
      // Some numbers up to and including 1985 are still valid if
      // check (last) digit equals 0 and modulo of first 9 digits equals 10
      var checkdigit = parseInt(tin.slice(0, 9), 10) % 11;
      if (parseInt(full_year, 10) < 1986 && checkdigit === 10) {
        if (parseInt(tin.slice(9), 10) !== 0) {
          return false;
        }
      } else {
        return false;
      }
    }
  }
  return true;
}

/*
 * de-AT validation function
 * (Abgabenkontonummer, persons/entities)
 * Verify TIN validity by calling luhnCheck()
 */
function deAtCheck(tin) {
  return algorithms.luhnCheck(tin);
}

/*
 * de-DE validation function
 * (Steueridentifikationsnummer (Steuer-IdNr.), persons only)
 * Tests for single duplicate/triplicate value, then calculates ISO 7064 check (last) digit
 * Partial implementation of spec (same result with both algorithms always)
 */
function deDeCheck(tin) {
  // Split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });

  // Fill array with strings of number positions
  var occurrences = [];
  for (var i = 0; i < digits.length - 1; i++) {
    occurrences.push('');
    for (var j = 0; j < digits.length - 1; j++) {
      if (digits[i] === digits[j]) {
        occurrences[i] += j;
      }
    }
  }

  // Remove digits with one occurrence and test for only one duplicate/triplicate
  occurrences = occurrences.filter(function (a) {
    return a.length > 1;
  });
  if (occurrences.length !== 2 && occurrences.length !== 3) {
    return false;
  }

  // In case of triplicate value only two digits are allowed next to each other
  if (occurrences[0].length === 3) {
    var trip_locations = occurrences[0].split('').map(function (a) {
      return parseInt(a, 10);
    });
    var recurrent = 0; // Amount of neighbor occurrences
    for (var _i = 0; _i < trip_locations.length - 1; _i++) {
      if (trip_locations[_i] + 1 === trip_locations[_i + 1]) {
        recurrent += 1;
      }
    }
    if (recurrent === 2) {
      return false;
    }
  }
  return algorithms.iso7064Check(tin);
}

/*
 * dk-DK validation function
 * (CPR-nummer (personnummer), persons only)
 * Checks if birth date (first six digits) is valid and assigned to century (seventh) digit,
 * and calculates check (last) digit
 */
function dkDkCheck(tin) {
  tin = tin.replace(/\W/, '');

  // Extract year, check if valid for given century digit and add century
  var year = parseInt(tin.slice(4, 6), 10);
  var century_digit = tin.slice(6, 7);
  switch (century_digit) {
    case '0':
    case '1':
    case '2':
    case '3':
      year = "19".concat(year);
      break;
    case '4':
    case '9':
      if (year < 37) {
        year = "20".concat(year);
      } else {
        year = "19".concat(year);
      }
      break;
    default:
      if (year < 37) {
        year = "20".concat(year);
      } else if (year > 58) {
        year = "18".concat(year);
      } else {
        return false;
      }
      break;
  }
  // Add missing zero if needed
  if (year.length === 3) {
    year = [year.slice(0, 2), '0', year.slice(2)].join('');
  }
  // Check date validity
  var date = "".concat(year, "/").concat(tin.slice(2, 4), "/").concat(tin.slice(0, 2));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });
  var checksum = 0;
  var weight = 4;
  // Multiply by weight and add to checksum
  for (var i = 0; i < 9; i++) {
    checksum += digits[i] * weight;
    weight -= 1;
    if (weight === 1) {
      weight = 7;
    }
  }
  checksum %= 11;
  if (checksum === 1) {
    return false;
  }
  return checksum === 0 ? digits[9] === 0 : digits[9] === 11 - checksum;
}

/*
 * el-CY validation function
 * (Arithmos Forologikou Mitroou (AFM/\u0391\u03a6\u039c), persons only)
 * Verify TIN validity by calculating ASCII value of check (last) character
 */
function elCyCheck(tin) {
  // split digits into an array for further processing
  var digits = tin.slice(0, 8).split('').map(function (a) {
    return parseInt(a, 10);
  });
  var checksum = 0;
  // add digits in even places
  for (var i = 1; i < digits.length; i += 2) {
    checksum += digits[i];
  }

  // add digits in odd places
  for (var _i2 = 0; _i2 < digits.length; _i2 += 2) {
    if (digits[_i2] < 2) {
      checksum += 1 - digits[_i2];
    } else {
      checksum += 2 * (digits[_i2] - 2) + 5;
      if (digits[_i2] > 4) {
        checksum += 2;
      }
    }
  }
  return String.fromCharCode(checksum % 26 + 65) === tin.charAt(8);
}

/*
 * el-GR validation function
 * (Arithmos Forologikou Mitroou (AFM/\u0391\u03a6\u039c), persons/entities)
 * Verify TIN validity by calculating check (last) digit
 * Algorithm not in DG TAXUD document- sourced from:
 * - `http://epixeirisi.gr/%CE%9A%CE%A1%CE%99%CE%A3%CE%99%CE%9C%CE%91-%CE%98%CE%95%CE%9C%CE%91%CE%A4%CE%91-%CE%A6%CE%9F%CE%A1%CE%9F%CE%9B%CE%9F%CE%93%CE%99%CE%91%CE%A3-%CE%9A%CE%91%CE%99-%CE%9B%CE%9F%CE%93%CE%99%CE%A3%CE%A4%CE%99%CE%9A%CE%97%CE%A3/23791/%CE%91%CF%81%CE%B9%CE%B8%CE%BC%CF%8C%CF%82-%CE%A6%CE%BF%CF%81%CE%BF%CE%BB%CE%BF%CE%B3%CE%B9%CE%BA%CE%BF%CF%8D-%CE%9C%CE%B7%CF%84%CF%81%CF%8E%CE%BF%CF%85`
 */
function elGrCheck(tin) {
  // split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });
  var checksum = 0;
  for (var i = 0; i < 8; i++) {
    checksum += digits[i] * Math.pow(2, 8 - i);
  }
  return checksum % 11 % 10 === digits[8];
}

/*
 * en-GB validation function (should go here if needed)
 * (National Insurance Number (NINO) or Unique Taxpayer Reference (UTR),
 * persons/entities respectively)
 */

/*
 * en-IE validation function
 * (Personal Public Service Number (PPS No), persons only)
 * Verify TIN validity by calculating check (second to last) character
 */
function enIeCheck(tin) {
  var checksum = algorithms.reverseMultiplyAndSum(tin.split('').slice(0, 7).map(function (a) {
    return parseInt(a, 10);
  }), 8);
  if (tin.length === 9 && tin[8] !== 'W') {
    checksum += (tin[8].charCodeAt(0) - 64) * 9;
  }
  checksum %= 23;
  if (checksum === 0) {
    return tin[7].toUpperCase() === 'W';
  }
  return tin[7].toUpperCase() === String.fromCharCode(64 + checksum);
}

// Valid US IRS campus prefixes
var enUsCampusPrefix = {
  andover: ['10', '12'],
  atlanta: ['60', '67'],
  austin: ['50', '53'],
  brookhaven: ['01', '02', '03', '04', '05', '06', '11', '13', '14', '16', '21', '22', '23', '25', '34', '51', '52', '54', '55', '56', '57', '58', '59', '65'],
  cincinnati: ['30', '32', '35', '36', '37', '38', '61'],
  fresno: ['15', '24'],
  internet: ['20', '26', '27', '45', '46', '47'],
  kansas: ['40', '44'],
  memphis: ['94', '95'],
  ogden: ['80', '90'],
  philadelphia: ['33', '39', '41', '42', '43', '46', '48', '62', '63', '64', '66', '68', '71', '72', '73', '74', '75', '76', '77', '81', '82', '83', '84', '85', '86', '87', '88', '91', '92', '93', '98', '99'],
  sba: ['31']
};

// Return an array of all US IRS campus prefixes
function enUsGetPrefixes() {
  var prefixes = [];
  for (var location in enUsCampusPrefix) {
    // https://github.com/gotwarlost/istanbul/blob/master/ignoring-code-for-coverage.md#ignoring-code-for-coverage-purposes
    // istanbul ignore else
    if (enUsCampusPrefix.hasOwnProperty(location)) {
      prefixes.push.apply(prefixes, _toConsumableArray(enUsCampusPrefix[location]));
    }
  }
  return prefixes;
}

/*
 * en-US validation function
 * Verify that the TIN starts with a valid IRS campus prefix
 */
function enUsCheck(tin) {
  return enUsGetPrefixes().indexOf(tin.slice(0, 2)) !== -1;
}

/*
 * es-AR validation function
 * Clave \u00danica de Identificaci\u00f3n Tributaria (CUIT/CUIL)
 * Sourced from:
 * - https://servicioscf.afip.gob.ar/publico/abc/ABCpaso2.aspx?id_nivel1=3036&id_nivel2=3040&p=Conceptos%20b%C3%A1sicos
 * - https://es.wikipedia.org/wiki/Clave_%C3%9Anica_de_Identificaci%C3%B3n_Tributaria
 */

function esArCheck(tin) {
  var accum = 0;
  var digits = tin.split('');
  var digit = parseInt(digits.pop(), 10);
  for (var i = 0; i < digits.length; i++) {
    accum += digits[9 - i] * (2 + i % 6);
  }
  var verif = 11 - accum % 11;
  if (verif === 11) {
    verif = 0;
  } else if (verif === 10) {
    verif = 9;
  }
  return digit === verif;
}

/*
 * es-ES validation function
 * (Documento Nacional de Identidad (DNI)
 * or N\u00famero de Identificaci\u00f3n de Extranjero (NIE), persons only)
 * Verify TIN validity by calculating check (last) character
 */
function esEsCheck(tin) {
  // Split characters into an array for further processing
  var chars = tin.toUpperCase().split('');

  // Replace initial letter if needed
  if (isNaN(parseInt(chars[0], 10)) && chars.length > 1) {
    var lead_replace = 0;
    switch (chars[0]) {
      case 'Y':
        lead_replace = 1;
        break;
      case 'Z':
        lead_replace = 2;
        break;
      default:
    }
    chars.splice(0, 1, lead_replace);
    // Fill with zeros if smaller than proper
  } else {
    while (chars.length < 9) {
      chars.unshift(0);
    }
  }

  // Calculate checksum and check according to lookup
  var lookup = ['T', 'R', 'W', 'A', 'G', 'M', 'Y', 'F', 'P', 'D', 'X', 'B', 'N', 'J', 'Z', 'S', 'Q', 'V', 'H', 'L', 'C', 'K', 'E'];
  chars = chars.join('');
  var checksum = parseInt(chars.slice(0, 8), 10) % 23;
  return chars[8] === lookup[checksum];
}

/*
 * et-EE validation function
 * (Isikukood (IK), persons only)
 * Checks if birth date (century digit and six following) is valid and calculates check (last) digit
 * Material not in DG TAXUD document sourced from:
 * - `https://www.oecd.org/tax/automatic-exchange/crs-implementation-and-assistance/tax-identification-numbers/Estonia-TIN.pdf`
 */
function etEeCheck(tin) {
  // Extract year and add century
  var full_year = tin.slice(1, 3);
  var century_digit = tin.slice(0, 1);
  switch (century_digit) {
    case '1':
    case '2':
      full_year = "18".concat(full_year);
      break;
    case '3':
    case '4':
      full_year = "19".concat(full_year);
      break;
    default:
      full_year = "20".concat(full_year);
      break;
  }
  // Check date validity
  var date = "".concat(full_year, "/").concat(tin.slice(3, 5), "/").concat(tin.slice(5, 7));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });
  var checksum = 0;
  var weight = 1;
  // Multiply by weight and add to checksum
  for (var i = 0; i < 10; i++) {
    checksum += digits[i] * weight;
    weight += 1;
    if (weight === 10) {
      weight = 1;
    }
  }
  // Do again if modulo 11 of checksum is 10
  if (checksum % 11 === 10) {
    checksum = 0;
    weight = 3;
    for (var _i3 = 0; _i3 < 10; _i3++) {
      checksum += digits[_i3] * weight;
      weight += 1;
      if (weight === 10) {
        weight = 1;
      }
    }
    if (checksum % 11 === 10) {
      return digits[10] === 0;
    }
  }
  return checksum % 11 === digits[10];
}

/*
 * fi-FI validation function
 * (Henkil\u00f6tunnus (HETU), persons only)
 * Checks if birth date (first six digits plus century symbol) is valid
 * and calculates check (last) digit
 */
function fiFiCheck(tin) {
  // Extract year and add century
  var full_year = tin.slice(4, 6);
  var century_symbol = tin.slice(6, 7);
  switch (century_symbol) {
    case '+':
      full_year = "18".concat(full_year);
      break;
    case '-':
      full_year = "19".concat(full_year);
      break;
    default:
      full_year = "20".concat(full_year);
      break;
  }
  // Check date validity
  var date = "".concat(full_year, "/").concat(tin.slice(2, 4), "/").concat(tin.slice(0, 2));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Calculate check character
  var checksum = parseInt(tin.slice(0, 6) + tin.slice(7, 10), 10) % 31;
  if (checksum < 10) {
    return checksum === parseInt(tin.slice(10), 10);
  }
  checksum -= 10;
  var letters_lookup = ['A', 'B', 'C', 'D', 'E', 'F', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y'];
  return letters_lookup[checksum] === tin.slice(10);
}

/*
 * fr/nl-BE validation function
 * (Num\u00e9ro national (N.N.), persons only)
 * Checks if birth date (first six digits) is valid and calculates check (last two) digits
 */
function frBeCheck(tin) {
  // Zero month/day value is acceptable
  if (tin.slice(2, 4) !== '00' || tin.slice(4, 6) !== '00') {
    // Extract date from first six digits of TIN
    var date = "".concat(tin.slice(0, 2), "/").concat(tin.slice(2, 4), "/").concat(tin.slice(4, 6));
    if (!(0, _isDate.default)(date, 'YY/MM/DD')) {
      return false;
    }
  }
  var checksum = 97 - parseInt(tin.slice(0, 9), 10) % 97;
  var checkdigits = parseInt(tin.slice(9, 11), 10);
  if (checksum !== checkdigits) {
    checksum = 97 - parseInt("2".concat(tin.slice(0, 9)), 10) % 97;
    if (checksum !== checkdigits) {
      return false;
    }
  }
  return true;
}

/*
 * fr-FR validation function
 * (Num\u00e9ro fiscal de r\u00e9f\u00e9rence (num\u00e9ro SPI), persons only)
 * Verify TIN validity by calculating check (last three) digits
 */
function frFrCheck(tin) {
  tin = tin.replace(/\s/g, '');
  var checksum = parseInt(tin.slice(0, 10), 10) % 511;
  var checkdigits = parseInt(tin.slice(10, 13), 10);
  return checksum === checkdigits;
}

/*
 * fr/lb-LU validation function
 * (num\u00e9ro d\u2019identification personnelle, persons only)
 * Verify birth date validity and run Luhn and Verhoeff checks
 */
function frLuCheck(tin) {
  // Extract date and check validity
  var date = "".concat(tin.slice(0, 4), "/").concat(tin.slice(4, 6), "/").concat(tin.slice(6, 8));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Run Luhn check
  if (!algorithms.luhnCheck(tin.slice(0, 12))) {
    return false;
  }
  // Remove Luhn check digit and run Verhoeff check
  return algorithms.verhoeffCheck("".concat(tin.slice(0, 11)).concat(tin[12]));
}

/*
 * hr-HR validation function
 * (Osobni identifikacijski broj (OIB), persons/entities)
 * Verify TIN validity by calling iso7064Check(digits)
 */
function hrHrCheck(tin) {
  return algorithms.iso7064Check(tin);
}

/*
 * hu-HU validation function
 * (Ad\u00f3azonos\u00edt\u00f3 jel, persons only)
 * Verify TIN validity by calculating check (last) digit
 */
function huHuCheck(tin) {
  // split digits into an array for further processing
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });
  var checksum = 8;
  for (var i = 1; i < 9; i++) {
    checksum += digits[i] * (i + 1);
  }
  return checksum % 11 === digits[9];
}

/*
 * lt-LT validation function (should go here if needed)
 * (Asmens kodas, persons/entities respectively)
 * Current validation check is alias of etEeCheck- same format applies
 */

/*
 * it-IT first/last name validity check
 * Accepts it-IT TIN-encoded names as a three-element character array and checks their validity
 * Due to lack of clarity between resources ("Are only Italian consonants used?
 * What happens if a person has X in their name?" etc.) only two test conditions
 * have been implemented:
 * Vowels may only be followed by other vowels or an X character
 * and X characters after vowels may only be followed by other X characters.
 */
function itItNameCheck(name) {
  // true at the first occurrence of a vowel
  var vowelflag = false;

  // true at the first occurrence of an X AFTER vowel
  // (to properly handle last names with X as consonant)
  var xflag = false;
  for (var i = 0; i < 3; i++) {
    if (!vowelflag && /[AEIOU]/.test(name[i])) {
      vowelflag = true;
    } else if (!xflag && vowelflag && name[i] === 'X') {
      xflag = true;
    } else if (i > 0) {
      if (vowelflag && !xflag) {
        if (!/[AEIOU]/.test(name[i])) {
          return false;
        }
      }
      if (xflag) {
        if (!/X/.test(name[i])) {
          return false;
        }
      }
    }
  }
  return true;
}

/*
 * it-IT validation function
 * (Codice fiscale (TIN-IT), persons only)
 * Verify name, birth date and codice catastale validity
 * and calculate check character.
 * Material not in DG-TAXUD document sourced from:
 * `https://en.wikipedia.org/wiki/Italian_fiscal_code`
 */
function itItCheck(tin) {
  // Capitalize and split characters into an array for further processing
  var chars = tin.toUpperCase().split('');

  // Check first and last name validity calling itItNameCheck()
  if (!itItNameCheck(chars.slice(0, 3))) {
    return false;
  }
  if (!itItNameCheck(chars.slice(3, 6))) {
    return false;
  }

  // Convert letters in number spaces back to numbers if any
  var number_locations = [6, 7, 9, 10, 12, 13, 14];
  var number_replace = {
    L: '0',
    M: '1',
    N: '2',
    P: '3',
    Q: '4',
    R: '5',
    S: '6',
    T: '7',
    U: '8',
    V: '9'
  };
  for (var _i4 = 0, _number_locations = number_locations; _i4 < _number_locations.length; _i4++) {
    var i = _number_locations[_i4];
    if (chars[i] in number_replace) {
      chars.splice(i, 1, number_replace[chars[i]]);
    }
  }

  // Extract month and day, and check date validity
  var month_replace = {
    A: '01',
    B: '02',
    C: '03',
    D: '04',
    E: '05',
    H: '06',
    L: '07',
    M: '08',
    P: '09',
    R: '10',
    S: '11',
    T: '12'
  };
  var month = month_replace[chars[8]];
  var day = parseInt(chars[9] + chars[10], 10);
  if (day > 40) {
    day -= 40;
  }
  if (day < 10) {
    day = "0".concat(day);
  }
  var date = "".concat(chars[6]).concat(chars[7], "/").concat(month, "/").concat(day);
  if (!(0, _isDate.default)(date, 'YY/MM/DD')) {
    return false;
  }

  // Calculate check character by adding up even and odd characters as numbers
  var checksum = 0;
  for (var _i5 = 1; _i5 < chars.length - 1; _i5 += 2) {
    var char_to_int = parseInt(chars[_i5], 10);
    if (isNaN(char_to_int)) {
      char_to_int = chars[_i5].charCodeAt(0) - 65;
    }
    checksum += char_to_int;
  }
  var odd_convert = {
    // Maps of characters at odd places
    A: 1,
    B: 0,
    C: 5,
    D: 7,
    E: 9,
    F: 13,
    G: 15,
    H: 17,
    I: 19,
    J: 21,
    K: 2,
    L: 4,
    M: 18,
    N: 20,
    O: 11,
    P: 3,
    Q: 6,
    R: 8,
    S: 12,
    T: 14,
    U: 16,
    V: 10,
    W: 22,
    X: 25,
    Y: 24,
    Z: 23,
    0: 1,
    1: 0
  };
  for (var _i6 = 0; _i6 < chars.length - 1; _i6 += 2) {
    var _char_to_int = 0;
    if (chars[_i6] in odd_convert) {
      _char_to_int = odd_convert[chars[_i6]];
    } else {
      var multiplier = parseInt(chars[_i6], 10);
      _char_to_int = 2 * multiplier + 1;
      if (multiplier > 4) {
        _char_to_int += 2;
      }
    }
    checksum += _char_to_int;
  }
  if (String.fromCharCode(65 + checksum % 26) !== chars[15]) {
    return false;
  }
  return true;
}

/*
 * lv-LV validation function
 * (Personas kods (PK), persons only)
 * Check validity of birth date and calculate check (last) digit
 * Support only for old format numbers (not starting with '32', issued before 2017/07/01)
 * Material not in DG TAXUD document sourced from:
 * `https://boot.ritakafija.lv/forums/index.php?/topic/88314-personas-koda-algoritms-%C4%8Deksumma/`
 */
function lvLvCheck(tin) {
  tin = tin.replace(/\W/, '');
  // Extract date from TIN
  var day = tin.slice(0, 2);
  if (day !== '32') {
    // No date/checksum check if new format
    var month = tin.slice(2, 4);
    if (month !== '00') {
      // No date check if unknown month
      var full_year = tin.slice(4, 6);
      switch (tin[6]) {
        case '0':
          full_year = "18".concat(full_year);
          break;
        case '1':
          full_year = "19".concat(full_year);
          break;
        default:
          full_year = "20".concat(full_year);
          break;
      }
      // Check date validity
      var date = "".concat(full_year, "/").concat(tin.slice(2, 4), "/").concat(day);
      if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
        return false;
      }
    }

    // Calculate check digit
    var checksum = 1101;
    var multip_lookup = [1, 6, 3, 7, 9, 10, 5, 8, 4, 2];
    for (var i = 0; i < tin.length - 1; i++) {
      checksum -= parseInt(tin[i], 10) * multip_lookup[i];
    }
    return parseInt(tin[10], 10) === checksum % 11;
  }
  return true;
}

/*
 * mt-MT validation function
 * (Identity Card Number or Unique Taxpayer Reference, persons/entities)
 * Verify Identity Card Number structure (no other tests found)
 */
function mtMtCheck(tin) {
  if (tin.length !== 9) {
    // No tests for UTR
    var chars = tin.toUpperCase().split('');
    // Fill with zeros if smaller than proper
    while (chars.length < 8) {
      chars.unshift(0);
    }
    // Validate format according to last character
    switch (tin[7]) {
      case 'A':
      case 'P':
        if (parseInt(chars[6], 10) === 0) {
          return false;
        }
        break;
      default:
        {
          var first_part = parseInt(chars.join('').slice(0, 5), 10);
          if (first_part > 32000) {
            return false;
          }
          var second_part = parseInt(chars.join('').slice(5, 7), 10);
          if (first_part === second_part) {
            return false;
          }
        }
    }
  }
  return true;
}

/*
 * nl-NL validation function
 * (Burgerservicenummer (BSN) or Rechtspersonen Samenwerkingsverbanden Informatie Nummer (RSIN),
 * persons/entities respectively)
 * Verify TIN validity by calculating check (last) digit (variant of MOD 11)
 */
function nlNlCheck(tin) {
  return algorithms.reverseMultiplyAndSum(tin.split('').slice(0, 8).map(function (a) {
    return parseInt(a, 10);
  }), 9) % 11 === parseInt(tin[8], 10);
}

/*
 * pl-PL validation function
 * (Powszechny Elektroniczny System Ewidencji Ludno\u015bci (PESEL)
 * or Numer identyfikacji podatkowej (NIP), persons/entities)
 * Verify TIN validity by validating birth date (PESEL) and calculating check (last) digit
 */
function plPlCheck(tin) {
  // NIP
  if (tin.length === 10) {
    // Calculate last digit by multiplying with lookup
    var lookup = [6, 5, 7, 2, 3, 4, 5, 6, 7];
    var _checksum = 0;
    for (var i = 0; i < lookup.length; i++) {
      _checksum += parseInt(tin[i], 10) * lookup[i];
    }
    _checksum %= 11;
    if (_checksum === 10) {
      return false;
    }
    return _checksum === parseInt(tin[9], 10);
  }

  // PESEL
  // Extract full year using month
  var full_year = tin.slice(0, 2);
  var month = parseInt(tin.slice(2, 4), 10);
  if (month > 80) {
    full_year = "18".concat(full_year);
    month -= 80;
  } else if (month > 60) {
    full_year = "22".concat(full_year);
    month -= 60;
  } else if (month > 40) {
    full_year = "21".concat(full_year);
    month -= 40;
  } else if (month > 20) {
    full_year = "20".concat(full_year);
    month -= 20;
  } else {
    full_year = "19".concat(full_year);
  }
  // Add leading zero to month if needed
  if (month < 10) {
    month = "0".concat(month);
  }
  // Check date validity
  var date = "".concat(full_year, "/").concat(month, "/").concat(tin.slice(4, 6));
  if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }

  // Calculate last digit by multiplying with odd one-digit numbers except 5
  var checksum = 0;
  var multiplier = 1;
  for (var _i7 = 0; _i7 < tin.length - 1; _i7++) {
    checksum += parseInt(tin[_i7], 10) * multiplier % 10;
    multiplier += 2;
    if (multiplier > 10) {
      multiplier = 1;
    } else if (multiplier === 5) {
      multiplier += 2;
    }
  }
  checksum = 10 - checksum % 10;
  return checksum === parseInt(tin[10], 10);
}

/*
* pt-BR validation function
* (Cadastro de Pessoas F\u00edsicas (CPF, persons)
* Cadastro Nacional de Pessoas Jur\u00eddicas (CNPJ, entities)
* Both inputs will be validated
*/

function ptBrCheck(tin) {
  if (tin.length === 11) {
    var _sum;
    var remainder;
    _sum = 0;
    if (
    // Reject known invalid CPFs
    tin === '11111111111' || tin === '22222222222' || tin === '33333333333' || tin === '44444444444' || tin === '55555555555' || tin === '66666666666' || tin === '77777777777' || tin === '88888888888' || tin === '99999999999' || tin === '00000000000') return false;
    for (var i = 1; i <= 9; i++) _sum += parseInt(tin.substring(i - 1, i), 10) * (11 - i);
    remainder = _sum * 10 % 11;
    if (remainder === 10) remainder = 0;
    if (remainder !== parseInt(tin.substring(9, 10), 10)) return false;
    _sum = 0;
    for (var _i8 = 1; _i8 <= 10; _i8++) _sum += parseInt(tin.substring(_i8 - 1, _i8), 10) * (12 - _i8);
    remainder = _sum * 10 % 11;
    if (remainder === 10) remainder = 0;
    if (remainder !== parseInt(tin.substring(10, 11), 10)) return false;
    return true;
  }
  if (
  // Reject know invalid CNPJs
  tin === '00000000000000' || tin === '11111111111111' || tin === '22222222222222' || tin === '33333333333333' || tin === '44444444444444' || tin === '55555555555555' || tin === '66666666666666' || tin === '77777777777777' || tin === '88888888888888' || tin === '99999999999999') {
    return false;
  }
  var length = tin.length - 2;
  var identifiers = tin.substring(0, length);
  var verificators = tin.substring(length);
  var sum = 0;
  var pos = length - 7;
  for (var _i9 = length; _i9 >= 1; _i9--) {
    sum += identifiers.charAt(length - _i9) * pos;
    pos -= 1;
    if (pos < 2) {
      pos = 9;
    }
  }
  var result = sum % 11 < 2 ? 0 : 11 - sum % 11;
  if (result !== parseInt(verificators.charAt(0), 10)) {
    return false;
  }
  length += 1;
  identifiers = tin.substring(0, length);
  sum = 0;
  pos = length - 7;
  for (var _i0 = length; _i0 >= 1; _i0--) {
    sum += identifiers.charAt(length - _i0) * pos;
    pos -= 1;
    if (pos < 2) {
      pos = 9;
    }
  }
  result = sum % 11 < 2 ? 0 : 11 - sum % 11;
  if (result !== parseInt(verificators.charAt(1), 10)) {
    return false;
  }
  return true;
}

/*
 * pt-PT validation function
 * (N\u00famero de identifica\u00e7\u00e3o fiscal (NIF), persons/entities)
 * Verify TIN validity by calculating check (last) digit (variant of MOD 11)
 */
function ptPtCheck(tin) {
  var checksum = 11 - algorithms.reverseMultiplyAndSum(tin.split('').slice(0, 8).map(function (a) {
    return parseInt(a, 10);
  }), 9) % 11;
  if (checksum > 9) {
    return parseInt(tin[8], 10) === 0;
  }
  return checksum === parseInt(tin[8], 10);
}

/*
 * ro-RO validation function
 * (Cod Numeric Personal (CNP) or Cod de \u00eenregistrare fiscal\u0103 (CIF),
 * persons only)
 * Verify CNP validity by calculating check (last) digit (test not found for CIF)
 * Material not in DG TAXUD document sourced from:
 * `https://en.wikipedia.org/wiki/National_identification_number#Romania`
 */
function roRoCheck(tin) {
  if (tin.slice(0, 4) !== '9000') {
    // No test found for this format
    // Extract full year using century digit if possible
    var full_year = tin.slice(1, 3);
    switch (tin[0]) {
      case '1':
      case '2':
        full_year = "19".concat(full_year);
        break;
      case '3':
      case '4':
        full_year = "18".concat(full_year);
        break;
      case '5':
      case '6':
        full_year = "20".concat(full_year);
        break;
      default:
    }

    // Check date validity
    var date = "".concat(full_year, "/").concat(tin.slice(3, 5), "/").concat(tin.slice(5, 7));
    if (date.length === 8) {
      if (!(0, _isDate.default)(date, 'YY/MM/DD')) {
        return false;
      }
    } else if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
      return false;
    }

    // Calculate check digit
    var digits = tin.split('').map(function (a) {
      return parseInt(a, 10);
    });
    var multipliers = [2, 7, 9, 1, 4, 6, 3, 5, 8, 2, 7, 9];
    var checksum = 0;
    for (var i = 0; i < multipliers.length; i++) {
      checksum += digits[i] * multipliers[i];
    }
    if (checksum % 11 === 10) {
      return digits[12] === 1;
    }
    return digits[12] === checksum % 11;
  }
  return true;
}

/*
 * sk-SK validation function
 * (Rodn\u00e9 \u010d\u00edslo (R\u010c) or bezv\u00fdznamov\u00e9 identifika\u010dn\u00e9 \u010d\u00edslo (BI\u010c), persons only)
 * Checks validity of pre-1954 birth numbers (rodn\u00e9 \u010d\u00edslo) only
 * Due to the introduction of the pseudo-random BI\u010c it is not possible to test
 * post-1954 birth numbers without knowing whether they are BI\u010c or R\u010c beforehand
 */
function skSkCheck(tin) {
  if (tin.length === 9) {
    tin = tin.replace(/\W/, '');
    if (tin.slice(6) === '000') {
      return false;
    } // Three-zero serial not assigned before 1954

    // Extract full year from TIN length
    var full_year = parseInt(tin.slice(0, 2), 10);
    if (full_year > 53) {
      return false;
    }
    if (full_year < 10) {
      full_year = "190".concat(full_year);
    } else {
      full_year = "19".concat(full_year);
    }

    // Extract month from TIN and normalize
    var month = parseInt(tin.slice(2, 4), 10);
    if (month > 50) {
      month -= 50;
    }
    if (month < 10) {
      month = "0".concat(month);
    }

    // Check date validity
    var date = "".concat(full_year, "/").concat(month, "/").concat(tin.slice(4, 6));
    if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
      return false;
    }
  }
  return true;
}

/*
 * sl-SI validation function
 * (Dav\u010dna \u0161tevilka, persons/entities)
 * Verify TIN validity by calculating check (last) digit (variant of MOD 11)
 */
function slSiCheck(tin) {
  var checksum = 11 - algorithms.reverseMultiplyAndSum(tin.split('').slice(0, 7).map(function (a) {
    return parseInt(a, 10);
  }), 8) % 11;
  if (checksum === 10) {
    return parseInt(tin[7], 10) === 0;
  }
  return checksum === parseInt(tin[7], 10);
}

/*
 * sv-SE validation function
 * (Personnummer or samordningsnummer, persons only)
 * Checks validity of birth date and calls luhnCheck() to validate check (last) digit
 */
function svSeCheck(tin) {
  // Make copy of TIN and normalize to two-digit year form
  var tin_copy = tin.slice(0);
  if (tin.length > 11) {
    tin_copy = tin_copy.slice(2);
  }

  // Extract date of birth
  var full_year = '';
  var month = tin_copy.slice(2, 4);
  var day = parseInt(tin_copy.slice(4, 6), 10);
  if (tin.length > 11) {
    full_year = tin.slice(0, 4);
  } else {
    full_year = tin.slice(0, 2);
    if (tin.length === 11 && day < 60) {
      // Extract full year from centenarian symbol
      // Should work just fine until year 10000 or so
      var current_year = new Date().getFullYear().toString();
      var current_century = parseInt(current_year.slice(0, 2), 10);
      current_year = parseInt(current_year, 10);
      if (tin[6] === '-') {
        if (parseInt("".concat(current_century).concat(full_year), 10) > current_year) {
          full_year = "".concat(current_century - 1).concat(full_year);
        } else {
          full_year = "".concat(current_century).concat(full_year);
        }
      } else {
        full_year = "".concat(current_century - 1).concat(full_year);
        if (current_year - parseInt(full_year, 10) < 100) {
          return false;
        }
      }
    }
  }

  // Normalize day and check date validity
  if (day > 60) {
    day -= 60;
  }
  if (day < 10) {
    day = "0".concat(day);
  }
  var date = "".concat(full_year, "/").concat(month, "/").concat(day);
  if (date.length === 8) {
    if (!(0, _isDate.default)(date, 'YY/MM/DD')) {
      return false;
    }
  } else if (!(0, _isDate.default)(date, 'YYYY/MM/DD')) {
    return false;
  }
  return algorithms.luhnCheck(tin.replace(/\W/, ''));
}

/**
 * uk-UA validation function
 * Verify TIN validity by calculating check (last) digit (variant of MOD 11)
 */
function ukUaCheck(tin) {
  // Calculate check digit
  var digits = tin.split('').map(function (a) {
    return parseInt(a, 10);
  });
  var multipliers = [-1, 5, 7, 9, 4, 6, 10, 5, 7];
  var checksum = 0;
  for (var i = 0; i < multipliers.length; i++) {
    checksum += digits[i] * multipliers[i];
  }
  return checksum % 11 === 10 ? digits[9] === 0 : digits[9] === checksum % 11;
}

// Locale lookup objects

/*
 * Tax id regex formats for various locales
 *
 * Where not explicitly specified in DG-TAXUD document both
 * uppercase and lowercase letters are acceptable.
 */
var taxIdFormat = {
  'bg-BG': /^\d{10}$/,
  'cs-CZ': /^\d{6}\/{0,1}\d{3,4}$/,
  'de-AT': /^\d{9}$/,
  'de-DE': /^[1-9]\d{10}$/,
  'dk-DK': /^\d{6}-{0,1}\d{4}$/,
  'el-CY': /^[09]\d{7}[A-Z]$/,
  'el-GR': /^([0-4]|[7-9])\d{8}$/,
  'en-CA': /^\d{9}$/,
  'en-GB': /^\d{10}$|^(?!GB|NK|TN|ZZ)(?![DFIQUV])[A-Z](?![DFIQUVO])[A-Z]\d{6}[ABCD ]$/i,
  'en-IE': /^\d{7}[A-W][A-IW]{0,1}$/i,
  'en-US': /^\d{2}[- ]{0,1}\d{7}$/,
  'es-AR': /(20|23|24|27|30|33|34)[0-9]{8}[0-9]/,
  'es-ES': /^(\d{0,8}|[XYZKLM]\d{7})[A-HJ-NP-TV-Z]$/i,
  'et-EE': /^[1-6]\d{6}(00[1-9]|0[1-9][0-9]|[1-6][0-9]{2}|70[0-9]|710)\d$/,
  'fi-FI': /^\d{6}[-+A]\d{3}[0-9A-FHJ-NPR-Y]$/i,
  'fr-BE': /^\d{11}$/,
  'fr-FR': /^[0-3]\d{12}$|^[0-3]\d\s\d{2}(\s\d{3}){3}$/,
  // Conforms both to official spec and provided example
  'fr-LU': /^\d{13}$/,
  'hr-HR': /^\d{11}$/,
  'hu-HU': /^8\d{9}$/,
  'it-IT': /^[A-Z]{6}[L-NP-V0-9]{2}[A-EHLMPRST][L-NP-V0-9]{2}[A-ILMZ][L-NP-V0-9]{3}[A-Z]$/i,
  'lv-LV': /^\d{6}-{0,1}\d{5}$/,
  // Conforms both to DG TAXUD spec and original research
  'mt-MT': /^\d{3,7}[APMGLHBZ]$|^([1-8])\1\d{7}$/i,
  'nl-NL': /^\d{9}$/,
  'pl-PL': /^\d{10,11}$/,
  'pt-BR': /(?:^\d{11}$)|(?:^\d{14}$)/,
  'pt-PT': /^\d{9}$/,
  'ro-RO': /^\d{13}$/,
  'sk-SK': /^\d{6}\/{0,1}\d{3,4}$/,
  'sl-SI': /^[1-9]\d{7}$/,
  'sv-SE': /^(\d{6}[-+]{0,1}\d{4}|(18|19|20)\d{6}[-+]{0,1}\d{4})$/,
  'uk-UA': /^\d{10}$/
};
// taxIdFormat locale aliases
taxIdFormat['lb-LU'] = taxIdFormat['fr-LU'];
taxIdFormat['lt-LT'] = taxIdFormat['et-EE'];
taxIdFormat['nl-BE'] = taxIdFormat['fr-BE'];
taxIdFormat['fr-CA'] = taxIdFormat['en-CA'];

// Algorithmic tax id check functions for various locales
var taxIdCheck = {
  'bg-BG': bgBgCheck,
  'cs-CZ': csCzCheck,
  'de-AT': deAtCheck,
  'de-DE': deDeCheck,
  'dk-DK': dkDkCheck,
  'el-CY': elCyCheck,
  'el-GR': elGrCheck,
  'en-CA': isCanadianSIN,
  'en-IE': enIeCheck,
  'en-US': enUsCheck,
  'es-AR': esArCheck,
  'es-ES': esEsCheck,
  'et-EE': etEeCheck,
  'fi-FI': fiFiCheck,
  'fr-BE': frBeCheck,
  'fr-FR': frFrCheck,
  'fr-LU': frLuCheck,
  'hr-HR': hrHrCheck,
  'hu-HU': huHuCheck,
  'it-IT': itItCheck,
  'lv-LV': lvLvCheck,
  'mt-MT': mtMtCheck,
  'nl-NL': nlNlCheck,
  'pl-PL': plPlCheck,
  'pt-BR': ptBrCheck,
  'pt-PT': ptPtCheck,
  'ro-RO': roRoCheck,
  'sk-SK': skSkCheck,
  'sl-SI': slSiCheck,
  'sv-SE': svSeCheck,
  'uk-UA': ukUaCheck
};
// taxIdCheck locale aliases
taxIdCheck['lb-LU'] = taxIdCheck['fr-LU'];
taxIdCheck['lt-LT'] = taxIdCheck['et-EE'];
taxIdCheck['nl-BE'] = taxIdCheck['fr-BE'];
taxIdCheck['fr-CA'] = taxIdCheck['en-CA'];

// Regexes for locales where characters should be omitted before checking format
var allsymbols = /[-\\\/!@#$%\^&\*\(\)\+\=\[\]]+/g;
var sanitizeRegexes = {
  'de-AT': allsymbols,
  'de-DE': /[\/\\]/g,
  'fr-BE': allsymbols
};
// sanitizeRegexes locale aliases
sanitizeRegexes['nl-BE'] = sanitizeRegexes['fr-BE'];

/*
 * Validator function
 * Return true if the passed string is a valid tax identification number
 * for the specified locale.
 * Throw an error exception if the locale is not supported.
 */
function isTaxID(str) {
  var locale = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'en-US';
  (0, _assertString.default)(str);
  // Copy TIN to avoid replacement if sanitized
  var strcopy = str.slice(0);
  if (locale in taxIdFormat) {
    if (locale in sanitizeRegexes) {
      strcopy = strcopy.replace(sanitizeRegexes[locale], '');
    }
    if (!taxIdFormat[locale].test(strcopy)) {
      return false;
    }
    if (locale in taxIdCheck) {
      return taxIdCheck[locale](strcopy);
    }
    // Fallthrough; not all locales have algorithmic checks
    return true;
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7761:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = void 0;
var _toDate = _interopRequireDefault(__webpack_require__(3752));
var _toFloat = _interopRequireDefault(__webpack_require__(1371));
var _toInt = _interopRequireDefault(__webpack_require__(855));
var _toBoolean = _interopRequireDefault(__webpack_require__(3214));
var _equals = _interopRequireDefault(__webpack_require__(700));
var _contains = _interopRequireDefault(__webpack_require__(9220));
var _matches = _interopRequireDefault(__webpack_require__(2576));
var _isEmail = _interopRequireDefault(__webpack_require__(9517));
var _isURL = _interopRequireDefault(__webpack_require__(7844));
var _isMACAddress = _interopRequireDefault(__webpack_require__(3196));
var _isIP = _interopRequireDefault(__webpack_require__(5372));
var _isIPRange = _interopRequireDefault(__webpack_require__(7115));
var _isFQDN = _interopRequireDefault(__webpack_require__(7658));
var _isDate = _interopRequireDefault(__webpack_require__(9013));
var _isTime = _interopRequireDefault(__webpack_require__(8796));
var _isBoolean = _interopRequireDefault(__webpack_require__(1697));
var _isLocale = _interopRequireDefault(__webpack_require__(7071));
var _isAbaRouting = _interopRequireDefault(__webpack_require__(4325));
var _isAlpha = _interopRequireWildcard(__webpack_require__(6529));
var _isAlphanumeric = _interopRequireWildcard(__webpack_require__(8986));
var _isNumeric = _interopRequireDefault(__webpack_require__(1666));
var _isPassportNumber = _interopRequireWildcard(__webpack_require__(3442));
var _isPort = _interopRequireDefault(__webpack_require__(3906));
var _isLowercase = _interopRequireDefault(__webpack_require__(7612));
var _isUppercase = _interopRequireDefault(__webpack_require__(5577));
var _isIMEI = _interopRequireDefault(__webpack_require__(8461));
var _isAscii = _interopRequireDefault(__webpack_require__(4294));
var _isFullWidth = _interopRequireDefault(__webpack_require__(9666));
var _isHalfWidth = _interopRequireDefault(__webpack_require__(9534));
var _isVariableWidth = _interopRequireDefault(__webpack_require__(1449));
var _isMultibyte = _interopRequireDefault(__webpack_require__(2830));
var _isSemVer = _interopRequireDefault(__webpack_require__(9841));
var _isSurrogatePair = _interopRequireDefault(__webpack_require__(3459));
var _isInt = _interopRequireDefault(__webpack_require__(6084));
var _isFloat = _interopRequireWildcard(__webpack_require__(995));
var _isDecimal = _interopRequireDefault(__webpack_require__(5926));
var _isHexadecimal = _interopRequireDefault(__webpack_require__(2002));
var _isOctal = _interopRequireDefault(__webpack_require__(9546));
var _isDivisibleBy = _interopRequireDefault(__webpack_require__(3735));
var _isHexColor = _interopRequireDefault(__webpack_require__(4641));
var _isRgbColor = _interopRequireDefault(__webpack_require__(5467));
var _isHSL = _interopRequireDefault(__webpack_require__(7086));
var _isISRC = _interopRequireDefault(__webpack_require__(1954));
var _isIBAN = _interopRequireWildcard(__webpack_require__(3641));
var _isBIC = _interopRequireDefault(__webpack_require__(5259));
var _isMD = _interopRequireDefault(__webpack_require__(9745));
var _isHash = _interopRequireDefault(__webpack_require__(3973));
var _isJWT = _interopRequireDefault(__webpack_require__(9172));
var _isJSON = _interopRequireDefault(__webpack_require__(5751));
var _isEmpty = _interopRequireDefault(__webpack_require__(2056));
var _isLength = _interopRequireDefault(__webpack_require__(9285));
var _isByteLength = _interopRequireDefault(__webpack_require__(6255));
var _isULID = _interopRequireDefault(__webpack_require__(9877));
var _isUUID = _interopRequireDefault(__webpack_require__(5186));
var _isMongoId = _interopRequireDefault(__webpack_require__(1252));
var _isAfter = _interopRequireDefault(__webpack_require__(1195));
var _isBefore = _interopRequireDefault(__webpack_require__(6658));
var _isIn = _interopRequireDefault(__webpack_require__(9266));
var _isLuhnNumber = _interopRequireDefault(__webpack_require__(3609));
var _isCreditCard = _interopRequireDefault(__webpack_require__(1062));
var _isIdentityCard = _interopRequireDefault(__webpack_require__(2645));
var _isEAN = _interopRequireDefault(__webpack_require__(7717));
var _isISIN = _interopRequireDefault(__webpack_require__(2678));
var _isISBN = _interopRequireDefault(__webpack_require__(9717));
var _isISSN = _interopRequireDefault(__webpack_require__(604));
var _isTaxID = _interopRequireDefault(__webpack_require__(7741));
var _isMobilePhone = _interopRequireWildcard(__webpack_require__(5251));
var _isEthereumAddress = _interopRequireDefault(__webpack_require__(82));
var _isCurrency = _interopRequireDefault(__webpack_require__(6782));
var _isBtcAddress = _interopRequireDefault(__webpack_require__(5748));
var _isISO = __webpack_require__(5777);
var _isISO2 = _interopRequireDefault(__webpack_require__(8033));
var _isISO3 = _interopRequireDefault(__webpack_require__(6169));
var _isRFC = _interopRequireDefault(__webpack_require__(1578));
var _isISO4 = _interopRequireDefault(__webpack_require__(3405));
var _isISO31661Alpha = _interopRequireDefault(__webpack_require__(8447));
var _isISO31661Alpha2 = _interopRequireDefault(__webpack_require__(3832));
var _isISO31661Numeric = _interopRequireDefault(__webpack_require__(3158));
var _isISO5 = _interopRequireDefault(__webpack_require__(8342));
var _isBase = _interopRequireDefault(__webpack_require__(7673));
var _isBase2 = _interopRequireDefault(__webpack_require__(6617));
var _isBase3 = _interopRequireDefault(__webpack_require__(8274));
var _isDataURI = _interopRequireDefault(__webpack_require__(3583));
var _isMagnetURI = _interopRequireDefault(__webpack_require__(7349));
var _isMailtoURI = _interopRequireDefault(__webpack_require__(2337));
var _isMimeType = _interopRequireDefault(__webpack_require__(4633));
var _isLatLong = _interopRequireDefault(__webpack_require__(5830));
var _isPostalCode = _interopRequireWildcard(__webpack_require__(3939));
var _ltrim = _interopRequireDefault(__webpack_require__(2309));
var _rtrim = _interopRequireDefault(__webpack_require__(2483));
var _trim = _interopRequireDefault(__webpack_require__(317));
var _escape = _interopRequireDefault(__webpack_require__(9790));
var _unescape = _interopRequireDefault(__webpack_require__(7677));
var _stripLow = _interopRequireDefault(__webpack_require__(561));
var _whitelist = _interopRequireDefault(__webpack_require__(1996));
var _blacklist = _interopRequireDefault(__webpack_require__(410));
var _isWhitelisted = _interopRequireDefault(__webpack_require__(629));
var _normalizeEmail = _interopRequireDefault(__webpack_require__(1128));
var _isSlug = _interopRequireDefault(__webpack_require__(9234));
var _isLicensePlate = _interopRequireDefault(__webpack_require__(676));
var _isStrongPassword = _interopRequireDefault(__webpack_require__(7179));
var _isVAT = _interopRequireDefault(__webpack_require__(5366));
function _interopRequireWildcard(e, t) { if ("function" == typeof WeakMap) var r = new WeakMap(), n = new WeakMap(); return (_interopRequireWildcard = function _interopRequireWildcard(e, t) { if (!t && e && e.__esModule) return e; var o, i, f = { __proto__: null, default: e }; if (null === e || "object" != _typeof(e) && "function" != typeof e) return f; if (o = t ? n : r) { if (o.has(e)) return o.get(e); o.set(e, f); } for (var _t in e) "default" !== _t && {}.hasOwnProperty.call(e, _t) && ((i = (o = Object.defineProperty) && Object.getOwnPropertyDescriptor(e, _t)) && (i.get || i.set) ? o(f, _t, i) : f[_t] = e[_t]); return f; })(e, t); }
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var version = '13.15.15';
var validator = {
  version: version,
  toDate: _toDate.default,
  toFloat: _toFloat.default,
  toInt: _toInt.default,
  toBoolean: _toBoolean.default,
  equals: _equals.default,
  contains: _contains.default,
  matches: _matches.default,
  isEmail: _isEmail.default,
  isURL: _isURL.default,
  isMACAddress: _isMACAddress.default,
  isIP: _isIP.default,
  isIPRange: _isIPRange.default,
  isFQDN: _isFQDN.default,
  isBoolean: _isBoolean.default,
  isIBAN: _isIBAN.default,
  isBIC: _isBIC.default,
  isAbaRouting: _isAbaRouting.default,
  isAlpha: _isAlpha.default,
  isAlphaLocales: _isAlpha.locales,
  isAlphanumeric: _isAlphanumeric.default,
  isAlphanumericLocales: _isAlphanumeric.locales,
  isNumeric: _isNumeric.default,
  isPassportNumber: _isPassportNumber.default,
  passportNumberLocales: _isPassportNumber.locales,
  isPort: _isPort.default,
  isLowercase: _isLowercase.default,
  isUppercase: _isUppercase.default,
  isAscii: _isAscii.default,
  isFullWidth: _isFullWidth.default,
  isHalfWidth: _isHalfWidth.default,
  isVariableWidth: _isVariableWidth.default,
  isMultibyte: _isMultibyte.default,
  isSemVer: _isSemVer.default,
  isSurrogatePair: _isSurrogatePair.default,
  isInt: _isInt.default,
  isIMEI: _isIMEI.default,
  isFloat: _isFloat.default,
  isFloatLocales: _isFloat.locales,
  isDecimal: _isDecimal.default,
  isHexadecimal: _isHexadecimal.default,
  isOctal: _isOctal.default,
  isDivisibleBy: _isDivisibleBy.default,
  isHexColor: _isHexColor.default,
  isRgbColor: _isRgbColor.default,
  isHSL: _isHSL.default,
  isISRC: _isISRC.default,
  isMD5: _isMD.default,
  isHash: _isHash.default,
  isJWT: _isJWT.default,
  isJSON: _isJSON.default,
  isEmpty: _isEmpty.default,
  isLength: _isLength.default,
  isLocale: _isLocale.default,
  isByteLength: _isByteLength.default,
  isULID: _isULID.default,
  isUUID: _isUUID.default,
  isMongoId: _isMongoId.default,
  isAfter: _isAfter.default,
  isBefore: _isBefore.default,
  isIn: _isIn.default,
  isLuhnNumber: _isLuhnNumber.default,
  isCreditCard: _isCreditCard.default,
  isIdentityCard: _isIdentityCard.default,
  isEAN: _isEAN.default,
  isISIN: _isISIN.default,
  isISBN: _isISBN.default,
  isISSN: _isISSN.default,
  isMobilePhone: _isMobilePhone.default,
  isMobilePhoneLocales: _isMobilePhone.locales,
  isPostalCode: _isPostalCode.default,
  isPostalCodeLocales: _isPostalCode.locales,
  isEthereumAddress: _isEthereumAddress.default,
  isCurrency: _isCurrency.default,
  isBtcAddress: _isBtcAddress.default,
  isISO6346: _isISO.isISO6346,
  isFreightContainerID: _isISO.isFreightContainerID,
  isISO6391: _isISO2.default,
  isISO8601: _isISO3.default,
  isISO15924: _isISO4.default,
  isRFC3339: _isRFC.default,
  isISO31661Alpha2: _isISO31661Alpha.default,
  isISO31661Alpha3: _isISO31661Alpha2.default,
  isISO31661Numeric: _isISO31661Numeric.default,
  isISO4217: _isISO5.default,
  isBase32: _isBase.default,
  isBase58: _isBase2.default,
  isBase64: _isBase3.default,
  isDataURI: _isDataURI.default,
  isMagnetURI: _isMagnetURI.default,
  isMailtoURI: _isMailtoURI.default,
  isMimeType: _isMimeType.default,
  isLatLong: _isLatLong.default,
  ltrim: _ltrim.default,
  rtrim: _rtrim.default,
  trim: _trim.default,
  escape: _escape.default,
  unescape: _unescape.default,
  stripLow: _stripLow.default,
  whitelist: _whitelist.default,
  blacklist: _blacklist.default,
  isWhitelisted: _isWhitelisted.default,
  normalizeEmail: _normalizeEmail.default,
  toString: toString,
  isSlug: _isSlug.default,
  isStrongPassword: _isStrongPassword.default,
  isTaxID: _isTaxID.default,
  isDate: _isDate.default,
  isTime: _isTime.default,
  isLicensePlate: _isLicensePlate.default,
  isVAT: _isVAT.default,
  ibanLocales: _isIBAN.locales
};
var _default = exports["default"] = validator;
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 7844:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isURL;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _checkHost = _interopRequireDefault(__webpack_require__(1572));
var _includesString = _interopRequireDefault(__webpack_require__(4636));
var _isFQDN = _interopRequireDefault(__webpack_require__(7658));
var _isIP = _interopRequireDefault(__webpack_require__(5372));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
/*
options for isURL method

protocols - valid protocols can be modified with this option.
require_tld - If set to false isURL will not check if the URL's host includes a top-level domain.
require_protocol - if set to true isURL will return false if protocol is not present in the URL.
require_host - if set to false isURL will not check if host is present in the URL.
require_port - if set to true isURL will check if port is present in the URL.
require_valid_protocol - isURL will check if the URL's protocol is present in the protocols option.
allow_underscores - if set to true, the validator will allow underscores in the URL.
host_whitelist - if set to an array of strings or regexp, and the domain matches none of the strings
                 defined in it, the validation fails.
host_blacklist - if set to an array of strings or regexp, and the domain matches any of the strings
                 defined in it, the validation fails.
allow_trailing_dot - if set to true, the validator will allow the domain to end with
                     a `.` character.
allow_protocol_relative_urls - if set to true protocol relative URLs will be allowed.
allow_fragments - if set to false isURL will return false if fragments are present.
allow_query_components - if set to false isURL will return false if query components are present.
disallow_auth - if set to true, the validator will fail if the URL contains an authentication
                component, e.g. `http://username:password@example.com`
validate_length - if set to false isURL will skip string length validation. `max_allowed_length`
                  will be ignored if this is set as `false`.
max_allowed_length - if set, isURL will not allow URLs longer than the specified value (default is
                     2084 that IE maximum URL length).

*/

var default_url_options = {
  protocols: ['http', 'https', 'ftp'],
  require_tld: true,
  require_protocol: false,
  require_host: true,
  require_port: false,
  require_valid_protocol: true,
  allow_underscores: false,
  allow_trailing_dot: false,
  allow_protocol_relative_urls: false,
  allow_fragments: true,
  allow_query_components: true,
  validate_length: true,
  max_allowed_length: 2084
};
var wrapped_ipv6 = /^\[([^\]]+)\](?::([0-9]+))?$/;
function isURL(url, options) {
  (0, _assertString.default)(url);
  if (!url || /[\s<>]/.test(url)) {
    return false;
  }
  if (url.indexOf('mailto:') === 0) {
    return false;
  }
  options = (0, _merge.default)(options, default_url_options);
  if (options.validate_length && url.length > options.max_allowed_length) {
    return false;
  }
  if (!options.allow_fragments && (0, _includesString.default)(url, '#')) {
    return false;
  }
  if (!options.allow_query_components && ((0, _includesString.default)(url, '?') || (0, _includesString.default)(url, '&'))) {
    return false;
  }
  var protocol, auth, host, hostname, port, port_str, split, ipv6;
  split = url.split('#');
  url = split.shift();
  split = url.split('?');
  url = split.shift();
  split = url.split('://');
  if (split.length > 1) {
    protocol = split.shift().toLowerCase();
    if (options.require_valid_protocol && options.protocols.indexOf(protocol) === -1) {
      return false;
    }
  } else if (options.require_protocol) {
    return false;
  } else if (url.slice(0, 2) === '//') {
    if (!options.allow_protocol_relative_urls) {
      return false;
    }
    split[0] = url.slice(2);
  }
  url = split.join('://');
  if (url === '') {
    return false;
  }
  split = url.split('/');
  url = split.shift();
  if (url === '' && !options.require_host) {
    return true;
  }
  split = url.split('@');
  if (split.length > 1) {
    if (options.disallow_auth) {
      return false;
    }
    if (split[0] === '') {
      return false;
    }
    auth = split.shift();
    if (auth.indexOf(':') >= 0 && auth.split(':').length > 2) {
      return false;
    }
    var _auth$split = auth.split(':'),
      _auth$split2 = _slicedToArray(_auth$split, 2),
      user = _auth$split2[0],
      password = _auth$split2[1];
    if (user === '' && password === '') {
      return false;
    }
  }
  hostname = split.join('@');
  port_str = null;
  ipv6 = null;
  var ipv6_match = hostname.match(wrapped_ipv6);
  if (ipv6_match) {
    host = '';
    ipv6 = ipv6_match[1];
    port_str = ipv6_match[2] || null;
  } else {
    split = hostname.split(':');
    host = split.shift();
    if (split.length) {
      port_str = split.join(':');
    }
  }
  if (port_str !== null && port_str.length > 0) {
    port = parseInt(port_str, 10);
    if (!/^[0-9]+$/.test(port_str) || port <= 0 || port > 65535) {
      return false;
    }
  } else if (options.require_port) {
    return false;
  }
  if (options.host_whitelist) {
    return (0, _checkHost.default)(host, options.host_whitelist);
  }
  if (host === '' && !options.require_host) {
    return true;
  }
  if (!(0, _isIP.default)(host) && !(0, _isFQDN.default)(host, options) && (!ipv6 || !(0, _isIP.default)(ipv6, 6))) {
    return false;
  }
  host = host || ipv6;
  if (options.host_blacklist && (0, _checkHost.default)(host, options.host_blacklist)) {
    return false;
  }
  return true;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8033:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISO6391;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var isISO6391Set = new Set(['aa', 'ab', 'ae', 'af', 'ak', 'am', 'an', 'ar', 'as', 'av', 'ay', 'az', 'az', 'ba', 'be', 'bg', 'bh', 'bi', 'bm', 'bn', 'bo', 'br', 'bs', 'ca', 'ce', 'ch', 'co', 'cr', 'cs', 'cu', 'cv', 'cy', 'da', 'de', 'dv', 'dz', 'ee', 'el', 'en', 'eo', 'es', 'et', 'eu', 'fa', 'ff', 'fi', 'fj', 'fo', 'fr', 'fy', 'ga', 'gd', 'gl', 'gn', 'gu', 'gv', 'ha', 'he', 'hi', 'ho', 'hr', 'ht', 'hu', 'hy', 'hz', 'ia', 'id', 'ie', 'ig', 'ii', 'ik', 'io', 'is', 'it', 'iu', 'ja', 'jv', 'ka', 'kg', 'ki', 'kj', 'kk', 'kl', 'km', 'kn', 'ko', 'kr', 'ks', 'ku', 'kv', 'kw', 'ky', 'la', 'lb', 'lg', 'li', 'ln', 'lo', 'lt', 'lu', 'lv', 'mg', 'mh', 'mi', 'mk', 'ml', 'mn', 'mr', 'ms', 'mt', 'my', 'na', 'nb', 'nd', 'ne', 'ng', 'nl', 'nn', 'no', 'nr', 'nv', 'ny', 'oc', 'oj', 'om', 'or', 'os', 'pa', 'pi', 'pl', 'ps', 'pt', 'qu', 'rm', 'rn', 'ro', 'ru', 'rw', 'sa', 'sc', 'sd', 'se', 'sg', 'si', 'sk', 'sl', 'sm', 'sn', 'so', 'sq', 'sr', 'ss', 'st', 'su', 'sv', 'sw', 'ta', 'te', 'tg', 'th', 'ti', 'tk', 'tl', 'tn', 'to', 'tr', 'ts', 'tt', 'tw', 'ty', 'ug', 'uk', 'ur', 'uz', 've', 'vi', 'vo', 'wa', 'wo', 'xh', 'yi', 'yo', 'za', 'zh', 'zu']);
function isISO6391(str) {
  (0, _assertString.default)(str);
  return isISO6391Set.has(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8274:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isBase64;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var base64WithPadding = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=|[A-Za-z0-9+/]{4})$/;
var base64WithoutPadding = /^[A-Za-z0-9+/]+$/;
var base64UrlWithPadding = /^(?:[A-Za-z0-9_-]{4})*(?:[A-Za-z0-9_-]{2}==|[A-Za-z0-9_-]{3}=|[A-Za-z0-9_-]{4})$/;
var base64UrlWithoutPadding = /^[A-Za-z0-9_-]+$/;
function isBase64(str, options) {
  var _options;
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, {
    urlSafe: false,
    padding: !((_options = options) !== null && _options !== void 0 && _options.urlSafe)
  });
  if (str === '') return true;
  var regex;
  if (options.urlSafe) {
    regex = options.padding ? base64UrlWithPadding : base64UrlWithoutPadding;
  } else {
    regex = options.padding ? base64WithPadding : base64WithoutPadding;
  }
  return (!options.padding || str.length % 4 === 0) && regex.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8342:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.CurrencyCodes = void 0;
exports["default"] = isISO4217;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// from https://en.wikipedia.org/wiki/ISO_4217
var validISO4217CurrencyCodes = new Set(['AED', 'AFN', 'ALL', 'AMD', 'ANG', 'AOA', 'ARS', 'AUD', 'AWG', 'AZN', 'BAM', 'BBD', 'BDT', 'BGN', 'BHD', 'BIF', 'BMD', 'BND', 'BOB', 'BOV', 'BRL', 'BSD', 'BTN', 'BWP', 'BYN', 'BZD', 'CAD', 'CDF', 'CHE', 'CHF', 'CHW', 'CLF', 'CLP', 'CNY', 'COP', 'COU', 'CRC', 'CUP', 'CVE', 'CZK', 'DJF', 'DKK', 'DOP', 'DZD', 'EGP', 'ERN', 'ETB', 'EUR', 'FJD', 'FKP', 'GBP', 'GEL', 'GHS', 'GIP', 'GMD', 'GNF', 'GTQ', 'GYD', 'HKD', 'HNL', 'HTG', 'HUF', 'IDR', 'ILS', 'INR', 'IQD', 'IRR', 'ISK', 'JMD', 'JOD', 'JPY', 'KES', 'KGS', 'KHR', 'KMF', 'KPW', 'KRW', 'KWD', 'KYD', 'KZT', 'LAK', 'LBP', 'LKR', 'LRD', 'LSL', 'LYD', 'MAD', 'MDL', 'MGA', 'MKD', 'MMK', 'MNT', 'MOP', 'MRU', 'MUR', 'MVR', 'MWK', 'MXN', 'MXV', 'MYR', 'MZN', 'NAD', 'NGN', 'NIO', 'NOK', 'NPR', 'NZD', 'OMR', 'PAB', 'PEN', 'PGK', 'PHP', 'PKR', 'PLN', 'PYG', 'QAR', 'RON', 'RSD', 'RUB', 'RWF', 'SAR', 'SBD', 'SCR', 'SDG', 'SEK', 'SGD', 'SHP', 'SLE', 'SLL', 'SOS', 'SRD', 'SSP', 'STN', 'SVC', 'SYP', 'SZL', 'THB', 'TJS', 'TMT', 'TND', 'TOP', 'TRY', 'TTD', 'TWD', 'TZS', 'UAH', 'UGX', 'USD', 'USN', 'UYI', 'UYU', 'UYW', 'UZS', 'VED', 'VES', 'VND', 'VUV', 'WST', 'XAF', 'XAG', 'XAU', 'XBA', 'XBB', 'XBC', 'XBD', 'XCD', 'XDR', 'XOF', 'XPD', 'XPF', 'XPT', 'XSU', 'XTS', 'XUA', 'XXX', 'YER', 'ZAR', 'ZMW', 'ZWL']);
function isISO4217(str) {
  (0, _assertString.default)(str);
  return validISO4217CurrencyCodes.has(str.toUpperCase());
}
var CurrencyCodes = exports.CurrencyCodes = validISO4217CurrencyCodes;

/***/ }),

/***/ 8447:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.CountryCodes = void 0;
exports["default"] = isISO31661Alpha2;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
var validISO31661Alpha2CountriesCodes = new Set(['AD', 'AE', 'AF', 'AG', 'AI', 'AL', 'AM', 'AO', 'AQ', 'AR', 'AS', 'AT', 'AU', 'AW', 'AX', 'AZ', 'BA', 'BB', 'BD', 'BE', 'BF', 'BG', 'BH', 'BI', 'BJ', 'BL', 'BM', 'BN', 'BO', 'BQ', 'BR', 'BS', 'BT', 'BV', 'BW', 'BY', 'BZ', 'CA', 'CC', 'CD', 'CF', 'CG', 'CH', 'CI', 'CK', 'CL', 'CM', 'CN', 'CO', 'CR', 'CU', 'CV', 'CW', 'CX', 'CY', 'CZ', 'DE', 'DJ', 'DK', 'DM', 'DO', 'DZ', 'EC', 'EE', 'EG', 'EH', 'ER', 'ES', 'ET', 'FI', 'FJ', 'FK', 'FM', 'FO', 'FR', 'GA', 'GB', 'GD', 'GE', 'GF', 'GG', 'GH', 'GI', 'GL', 'GM', 'GN', 'GP', 'GQ', 'GR', 'GS', 'GT', 'GU', 'GW', 'GY', 'HK', 'HM', 'HN', 'HR', 'HT', 'HU', 'ID', 'IE', 'IL', 'IM', 'IN', 'IO', 'IQ', 'IR', 'IS', 'IT', 'JE', 'JM', 'JO', 'JP', 'KE', 'KG', 'KH', 'KI', 'KM', 'KN', 'KP', 'KR', 'KW', 'KY', 'KZ', 'LA', 'LB', 'LC', 'LI', 'LK', 'LR', 'LS', 'LT', 'LU', 'LV', 'LY', 'MA', 'MC', 'MD', 'ME', 'MF', 'MG', 'MH', 'MK', 'ML', 'MM', 'MN', 'MO', 'MP', 'MQ', 'MR', 'MS', 'MT', 'MU', 'MV', 'MW', 'MX', 'MY', 'MZ', 'NA', 'NC', 'NE', 'NF', 'NG', 'NI', 'NL', 'NO', 'NP', 'NR', 'NU', 'NZ', 'OM', 'PA', 'PE', 'PF', 'PG', 'PH', 'PK', 'PL', 'PM', 'PN', 'PR', 'PS', 'PT', 'PW', 'PY', 'QA', 'RE', 'RO', 'RS', 'RU', 'RW', 'SA', 'SB', 'SC', 'SD', 'SE', 'SG', 'SH', 'SI', 'SJ', 'SK', 'SL', 'SM', 'SN', 'SO', 'SR', 'SS', 'ST', 'SV', 'SX', 'SY', 'SZ', 'TC', 'TD', 'TF', 'TG', 'TH', 'TJ', 'TK', 'TL', 'TM', 'TN', 'TO', 'TR', 'TT', 'TV', 'TW', 'TZ', 'UA', 'UG', 'UM', 'US', 'UY', 'UZ', 'VA', 'VC', 'VE', 'VG', 'VI', 'VN', 'VU', 'WF', 'WS', 'YE', 'YT', 'ZA', 'ZM', 'ZW']);
function isISO31661Alpha2(str) {
  (0, _assertString.default)(str);
  return validISO31661Alpha2CountriesCodes.has(str.toUpperCase());
}
var CountryCodes = exports.CountryCodes = validISO31661Alpha2CountriesCodes;

/***/ }),

/***/ 8461:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIMEI;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var imeiRegexWithoutHyphens = /^[0-9]{15}$/;
var imeiRegexWithHyphens = /^\d{2}-\d{6}-\d{6}-\d{1}$/;
function isIMEI(str, options) {
  (0, _assertString.default)(str);
  options = options || {};

  // default regex for checking imei is the one without hyphens

  var imeiRegex = imeiRegexWithoutHyphens;
  if (options.allow_hyphens) {
    imeiRegex = imeiRegexWithHyphens;
  }
  if (!imeiRegex.test(str)) {
    return false;
  }
  str = str.replace(/-/g, '');
  var sum = 0,
    mul = 2,
    l = 14;
  for (var i = 0; i < l; i++) {
    var digit = str.substring(l - i - 1, l - i);
    var tp = parseInt(digit, 10) * mul;
    if (tp >= 10) {
      sum += tp % 10 + 1;
    } else {
      sum += tp;
    }
    if (mul === 1) {
      mul += 1;
    } else {
      mul -= 1;
    }
  }
  var chk = (10 - sum % 10) % 10;
  if (chk !== parseInt(str.substring(14, 15), 10)) {
    return false;
  }
  return true;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8644:
/***/ ((module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = void 0;
var includes = function includes(arr, val) {
  return arr.some(function (arrVal) {
    return val === arrVal;
  });
};
var _default = exports["default"] = includes;
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8796:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isTime;
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var default_time_options = {
  hourFormat: 'hour24',
  mode: 'default'
};
var formats = {
  hour24: {
    default: /^([01]?[0-9]|2[0-3]):([0-5][0-9])$/,
    withSeconds: /^([01]?[0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9])$/,
    withOptionalSeconds: /^([01]?[0-9]|2[0-3]):([0-5][0-9])(?::([0-5][0-9]))?$/
  },
  hour12: {
    default: /^(0?[1-9]|1[0-2]):([0-5][0-9]) (A|P)M$/,
    withSeconds: /^(0?[1-9]|1[0-2]):([0-5][0-9]):([0-5][0-9]) (A|P)M$/,
    withOptionalSeconds: /^(0?[1-9]|1[0-2]):([0-5][0-9])(?::([0-5][0-9]))? (A|P)M$/
  }
};
function isTime(input, options) {
  options = (0, _merge.default)(options, default_time_options);
  if (typeof input !== 'string') return false;
  return formats[options.hourFormat][options.mode].test(input);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 8986:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isAlphanumeric;
exports.locales = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _alpha = __webpack_require__(3237);
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isAlphanumeric(_str) {
  var locale = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'en-US';
  var options = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};
  (0, _assertString.default)(_str);
  var str = _str;
  var ignore = options.ignore;
  if (ignore) {
    if (ignore instanceof RegExp) {
      str = str.replace(ignore, '');
    } else if (typeof ignore === 'string') {
      str = str.replace(new RegExp("[".concat(ignore.replace(/[-[\]{}()*+?.,\\^$|#\\s]/g, '\\$&'), "]"), 'g'), ''); // escape regex for ignore
    } else {
      throw new Error('ignore should be instance of a String or RegExp');
    }
  }
  if (locale in _alpha.alphanumeric) {
    return _alpha.alphanumeric[locale].test(str);
  }
  throw new Error("Invalid locale '".concat(locale, "'"));
}
var locales = exports.locales = Object.keys(_alpha.alphanumeric);

/***/ }),

/***/ 9013:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isDate;
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
var default_date_options = {
  format: 'YYYY/MM/DD',
  delimiters: ['/', '-'],
  strictMode: false
};
function isValidFormat(format) {
  return /(^(y{4}|y{2})[.\/-](m{1,2})[.\/-](d{1,2})$)|(^(m{1,2})[.\/-](d{1,2})[.\/-]((y{4}|y{2})$))|(^(d{1,2})[.\/-](m{1,2})[.\/-]((y{4}|y{2})$))/gi.test(format);
}
function zip(date, format) {
  var zippedArr = [],
    len = Math.max(date.length, format.length);
  for (var i = 0; i < len; i++) {
    zippedArr.push([date[i], format[i]]);
  }
  return zippedArr;
}
function isDate(input, options) {
  if (typeof options === 'string') {
    // Allow backward compatibility for old format isDate(input [, format])
    options = (0, _merge.default)({
      format: options
    }, default_date_options);
  } else {
    options = (0, _merge.default)(options, default_date_options);
  }
  if (typeof input === 'string' && isValidFormat(options.format)) {
    if (options.strictMode && input.length !== options.format.length) return false;
    var formatDelimiter = options.delimiters.find(function (delimiter) {
      return options.format.indexOf(delimiter) !== -1;
    });
    var dateDelimiter = options.strictMode ? formatDelimiter : options.delimiters.find(function (delimiter) {
      return input.indexOf(delimiter) !== -1;
    });
    var dateAndFormat = zip(input.split(dateDelimiter), options.format.toLowerCase().split(formatDelimiter));
    var dateObj = {};
    var _iterator = _createForOfIteratorHelper(dateAndFormat),
      _step;
    try {
      for (_iterator.s(); !(_step = _iterator.n()).done;) {
        var _step$value = _slicedToArray(_step.value, 2),
          dateWord = _step$value[0],
          formatWord = _step$value[1];
        if (!dateWord || !formatWord || dateWord.length !== formatWord.length) {
          return false;
        }
        dateObj[formatWord.charAt(0)] = dateWord;
      }
    } catch (err) {
      _iterator.e(err);
    } finally {
      _iterator.f();
    }
    var fullYear = dateObj.y;

    // Check if the year starts with a hyphen
    if (fullYear.startsWith('-')) {
      return false; // Hyphen before year is not allowed
    }
    if (dateObj.y.length === 2) {
      var parsedYear = parseInt(dateObj.y, 10);
      if (isNaN(parsedYear)) {
        return false;
      }
      var currentYearLastTwoDigits = new Date().getFullYear() % 100;
      if (parsedYear < currentYearLastTwoDigits) {
        fullYear = "20".concat(dateObj.y);
      } else {
        fullYear = "19".concat(dateObj.y);
      }
    }
    var month = dateObj.m;
    if (dateObj.m.length === 1) {
      month = "0".concat(dateObj.m);
    }
    var day = dateObj.d;
    if (dateObj.d.length === 1) {
      day = "0".concat(dateObj.d);
    }
    return new Date("".concat(fullYear, "-").concat(month, "-").concat(day, "T00:00:00.000Z")).getUTCDate() === +dateObj.d;
  }
  if (!options.strictMode) {
    return Object.prototype.toString.call(input) === '[object Date]' && isFinite(input);
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9172:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isJWT;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _isBase = _interopRequireDefault(__webpack_require__(8274));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isJWT(str) {
  (0, _assertString.default)(str);
  var dotSplit = str.split('.');
  var len = dotSplit.length;
  if (len !== 3) {
    return false;
  }
  return dotSplit.reduce(function (acc, currElem) {
    return acc && (0, _isBase.default)(currElem, {
      urlSafe: true
    });
  }, true);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9220:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = contains;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _toString = _interopRequireDefault(__webpack_require__(5772));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var defaultContainsOptions = {
  ignoreCase: false,
  minOccurrences: 1
};
function contains(str, elem, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, defaultContainsOptions);
  if (options.ignoreCase) {
    return str.toLowerCase().split((0, _toString.default)(elem).toLowerCase()).length > options.minOccurrences;
  }
  return str.split((0, _toString.default)(elem)).length > options.minOccurrences;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9234:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isSlug;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var charsetRegex = /^[^\s-_](?!.*?[-_]{2,})[a-z0-9-\\][^\s]*[^-_\s]$/;
function isSlug(str) {
  (0, _assertString.default)(str);
  return charsetRegex.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9266:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isIn;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _toString = _interopRequireDefault(__webpack_require__(5772));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function isIn(str, options) {
  (0, _assertString.default)(str);
  var i;
  if (Object.prototype.toString.call(options) === '[object Array]') {
    var array = [];
    for (i in options) {
      // https://github.com/gotwarlost/istanbul/blob/master/ignoring-code-for-coverage.md#ignoring-code-for-coverage-purposes
      // istanbul ignore else
      if ({}.hasOwnProperty.call(options, i)) {
        array[i] = (0, _toString.default)(options[i]);
      }
    }
    return array.indexOf(str) >= 0;
  } else if (_typeof(options) === 'object') {
    return options.hasOwnProperty(str);
  } else if (options && typeof options.indexOf === 'function') {
    return options.indexOf(str) >= 0;
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9285:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isLength;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
/* eslint-disable prefer-rest-params */
function isLength(str, options) {
  (0, _assertString.default)(str);
  var min;
  var max;
  if (_typeof(options) === 'object') {
    min = options.min || 0;
    max = options.max;
  } else {
    // backwards compatibility: isLength(str, min [, max])
    min = arguments[1] || 0;
    max = arguments[2];
  }
  var presentationSequences = str.match(/(\uFE0F|\uFE0E)/g) || [];
  var surrogatePairs = str.match(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g) || [];
  var len = str.length - presentationSequences.length - surrogatePairs.length;
  var isInsideRange = len >= min && (typeof max === 'undefined' || len <= max);
  if (isInsideRange && Array.isArray(options === null || options === void 0 ? void 0 : options.discreteLengths)) {
    return options.discreteLengths.some(function (discreteLen) {
      return discreteLen === len;
    });
  }
  return isInsideRange;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9517:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isEmail;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _checkHost = _interopRequireDefault(__webpack_require__(1572));
var _isByteLength = _interopRequireDefault(__webpack_require__(6255));
var _isFQDN = _interopRequireDefault(__webpack_require__(7658));
var _isIP = _interopRequireDefault(__webpack_require__(5372));
var _merge = _interopRequireDefault(__webpack_require__(3610));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var default_email_options = {
  allow_display_name: false,
  allow_underscores: false,
  require_display_name: false,
  allow_utf8_local_part: true,
  require_tld: true,
  blacklisted_chars: '',
  ignore_max_length: false,
  host_blacklist: [],
  host_whitelist: []
};

/* eslint-disable max-len */
/* eslint-disable no-control-regex */
var splitNameAddress = /^([^\x00-\x1F\x7F-\x9F\cX]+)</i;
var emailUserPart = /^[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~]+$/i;
var gmailUserPart = /^[a-z\d]+$/;
var quotedEmailUser = /^([\s\x01-\x08\x0b\x0c\x0e-\x1f\x7f\x21\x23-\x5b\x5d-\x7e]|(\\[\x01-\x09\x0b\x0c\x0d-\x7f]))*$/i;
var emailUserUtf8Part = /^[a-z\d!#\$%&'\*\+\-\/=\?\^_`{\|}~\u00A1-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]+$/i;
var quotedEmailUserUtf8 = /^([\s\x01-\x08\x0b\x0c\x0e-\x1f\x7f\x21\x23-\x5b\x5d-\x7e\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]|(\\[\x01-\x09\x0b\x0c\x0d-\x7f\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))*$/i;
var defaultMaxEmailLength = 254;
/* eslint-enable max-len */
/* eslint-enable no-control-regex */

/**
 * Validate display name according to the RFC2822: https://tools.ietf.org/html/rfc2822#appendix-A.1.2
 * @param {String} display_name
 */
function validateDisplayName(display_name) {
  var display_name_without_quotes = display_name.replace(/^"(.+)"$/, '$1');
  // display name with only spaces is not valid
  if (!display_name_without_quotes.trim()) {
    return false;
  }

  // check whether display name contains illegal character
  var contains_illegal = /[\.";<>]/.test(display_name_without_quotes);
  if (contains_illegal) {
    // if contains illegal characters,
    // must to be enclosed in double-quotes, otherwise it's not a valid display name
    if (display_name_without_quotes === display_name) {
      return false;
    }

    // the quotes in display name must start with character symbol \
    var all_start_with_back_slash = display_name_without_quotes.split('"').length === display_name_without_quotes.split('\\"').length;
    if (!all_start_with_back_slash) {
      return false;
    }
  }
  return true;
}
function isEmail(str, options) {
  (0, _assertString.default)(str);
  options = (0, _merge.default)(options, default_email_options);
  if (options.require_display_name || options.allow_display_name) {
    var display_email = str.match(splitNameAddress);
    if (display_email) {
      var display_name = display_email[1];

      // Remove display name and angle brackets to get email address
      // Can be done in the regex but will introduce a ReDOS (See  #1597 for more info)
      str = str.replace(display_name, '').replace(/(^<|>$)/g, '');

      // sometimes need to trim the last space to get the display name
      // because there may be a space between display name and email address
      // eg. myname <address@gmail.com>
      // the display name is `myname` instead of `myname `, so need to trim the last space
      if (display_name.endsWith(' ')) {
        display_name = display_name.slice(0, -1);
      }
      if (!validateDisplayName(display_name)) {
        return false;
      }
    } else if (options.require_display_name) {
      return false;
    }
  }
  if (!options.ignore_max_length && str.length > defaultMaxEmailLength) {
    return false;
  }
  var parts = str.split('@');
  var domain = parts.pop();
  var lower_domain = domain.toLowerCase();
  if (options.host_blacklist.length > 0 && (0, _checkHost.default)(lower_domain, options.host_blacklist)) {
    return false;
  }
  if (options.host_whitelist.length > 0 && !(0, _checkHost.default)(lower_domain, options.host_whitelist)) {
    return false;
  }
  var user = parts.join('@');
  if (options.domain_specific_validation && (lower_domain === 'gmail.com' || lower_domain === 'googlemail.com')) {
    /*
    Previously we removed dots for gmail addresses before validating.
    This was removed because it allows `multiple..dots@gmail.com`
    to be reported as valid, but it is not.
    Gmail only normalizes single dots, removing them from here is pointless,
    should be done in normalizeEmail
    */
    user = user.toLowerCase();

    // Removing sub-address from username before gmail validation
    var username = user.split('+')[0];

    // Dots are not included in gmail length restriction
    if (!(0, _isByteLength.default)(username.replace(/\./g, ''), {
      min: 6,
      max: 30
    })) {
      return false;
    }
    var _user_parts = username.split('.');
    for (var i = 0; i < _user_parts.length; i++) {
      if (!gmailUserPart.test(_user_parts[i])) {
        return false;
      }
    }
  }
  if (options.ignore_max_length === false && (!(0, _isByteLength.default)(user, {
    max: 64
  }) || !(0, _isByteLength.default)(domain, {
    max: 254
  }))) {
    return false;
  }
  if (!(0, _isFQDN.default)(domain, {
    require_tld: options.require_tld,
    ignore_max_length: options.ignore_max_length,
    allow_underscores: options.allow_underscores
  })) {
    if (!options.allow_ip_domain) {
      return false;
    }
    if (!(0, _isIP.default)(domain)) {
      if (!domain.startsWith('[') || !domain.endsWith(']')) {
        return false;
      }
      var noBracketdomain = domain.slice(1, -1);
      if (noBracketdomain.length === 0 || !(0, _isIP.default)(noBracketdomain)) {
        return false;
      }
    }
  }
  if (options.blacklisted_chars) {
    if (user.search(new RegExp("[".concat(options.blacklisted_chars, "]+"), 'g')) !== -1) return false;
  }
  if (user[0] === '"' && user[user.length - 1] === '"') {
    user = user.slice(1, user.length - 1);
    return options.allow_utf8_local_part ? quotedEmailUserUtf8.test(user) : quotedEmailUser.test(user);
  }
  var pattern = options.allow_utf8_local_part ? emailUserUtf8Part : emailUserPart;
  var user_parts = user.split('.');
  for (var _i = 0; _i < user_parts.length; _i++) {
    if (!pattern.test(user_parts[_i])) {
      return false;
    }
  }
  return true;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9534:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isHalfWidth;
exports.halfWidth = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var halfWidth = exports.halfWidth = /[\u0020-\u007E\uFF61-\uFF9F\uFFA0-\uFFDC\uFFE8-\uFFEE0-9a-zA-Z]/;
function isHalfWidth(str) {
  (0, _assertString.default)(str);
  return halfWidth.test(str);
}

/***/ }),

/***/ 9546:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isOctal;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var octal = /^(0o)?[0-7]+$/i;
function isOctal(str) {
  (0, _assertString.default)(str);
  return octal.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9666:
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isFullWidth;
exports.fullWidth = void 0;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var fullWidth = exports.fullWidth = /[^\u0020-\u007E\uFF61-\uFF9F\uFFA0-\uFFDC\uFFE8-\uFFEE0-9a-zA-Z]/;
function isFullWidth(str) {
  (0, _assertString.default)(str);
  return fullWidth.test(str);
}

/***/ }),

/***/ 9717:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isISBN;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var possibleIsbn10 = /^(?:[0-9]{9}X|[0-9]{10})$/;
var possibleIsbn13 = /^(?:[0-9]{13})$/;
var factor = [1, 3];
function isISBN(isbn, options) {
  (0, _assertString.default)(isbn);

  // For backwards compatibility:
  // isISBN(str [, version]), i.e. `options` could be used as argument for the legacy `version`
  var version = String((options === null || options === void 0 ? void 0 : options.version) || options);
  if (!(options !== null && options !== void 0 && options.version || options)) {
    return isISBN(isbn, {
      version: 10
    }) || isISBN(isbn, {
      version: 13
    });
  }
  var sanitizedIsbn = isbn.replace(/[\s-]+/g, '');
  var checksum = 0;
  if (version === '10') {
    if (!possibleIsbn10.test(sanitizedIsbn)) {
      return false;
    }
    for (var i = 0; i < version - 1; i++) {
      checksum += (i + 1) * sanitizedIsbn.charAt(i);
    }
    if (sanitizedIsbn.charAt(9) === 'X') {
      checksum += 10 * 10;
    } else {
      checksum += 10 * sanitizedIsbn.charAt(9);
    }
    if (checksum % 11 === 0) {
      return true;
    }
  } else if (version === '13') {
    if (!possibleIsbn13.test(sanitizedIsbn)) {
      return false;
    }
    for (var _i = 0; _i < 12; _i++) {
      checksum += factor[_i % 2] * sanitizedIsbn.charAt(_i);
    }
    if (sanitizedIsbn.charAt(12) - (10 - checksum % 10) % 10 === 0) {
      return true;
    }
  }
  return false;
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9745:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isMD5;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
var md5 = /^[a-f0-9]{32}$/;
function isMD5(str) {
  (0, _assertString.default)(str);
  return md5.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9790:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = escape;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function escape(str) {
  (0, _assertString.default)(str);
  return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#x27;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\//g, '&#x2F;').replace(/\\/g, '&#x5C;').replace(/`/g, '&#96;');
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9841:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isSemVer;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
var _multilineRegex = _interopRequireDefault(__webpack_require__(5730));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
/**
 * Regular Expression to match
 * semantic versioning (SemVer)
 * built from multi-line, multi-parts regexp
 * Reference: https://semver.org/
 */
var semanticVersioningRegex = (0, _multilineRegex.default)(['^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)', '(?:-((?:0|[1-9]\\d*|\\d*[a-z-][0-9a-z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-z-][0-9a-z-]*))*))', '?(?:\\+([0-9a-z-]+(?:\\.[0-9a-z-]+)*))?$'], 'i');
function isSemVer(str) {
  (0, _assertString.default)(str);
  return semanticVersioningRegex.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ }),

/***/ 9877:
/***/ ((module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports["default"] = isULID;
var _assertString = _interopRequireDefault(__webpack_require__(3399));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
function isULID(str) {
  (0, _assertString.default)(str);
  return /^[0-7][0-9A-HJKMNP-TV-Z]{25}$/i.test(str);
}
module.exports = exports.default;
module.exports["default"] = exports.default;

/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		var cachedModule = __webpack_module_cache__[moduleId];
/******/ 		if (cachedModule !== undefined) {
/******/ 			return cachedModule.exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
var __webpack_exports__ = {};
// This entry needs to be wrapped in an IIFE because it needs to be in strict mode.
(() => {
"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   runTest: () => (/* binding */ runTest)
/* harmony export */ });
/* harmony import */ var validator__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(7761);
// Original Source: https://raw.githubusercontent.com/validatorjs/validator.js/refs/heads/master/test/validators.test.js
// Version: https://github.com/validatorjs/validator.js/tree/13.15.15



let assertionCount = 0;

function assert(condition, ...args) {
  assertionCount++;
  if (!condition) throw new Error(`Assertion failure: ${args}`);
}

function describe(name, body) {
  body();
}

function it(name, body) {
  body();
}

function test({ validator, args = [], valid, invalid }) {
  const validatorMethod = validator__WEBPACK_IMPORTED_MODULE_0__[validator];
  valid?.forEach((validValue) => {
    assert(validatorMethod(validValue, ...args), validValue, ...args);
  });
  invalid?.forEach((validValue) => {
    assert(!validatorMethod(validValue, ...args), validValue, ...args);
  });
}

function runTest() {
  assertionCount = 0;

  describe("Validators", () => {
    it("should validate email addresses", () => {
      test({
        validator: "isEmail",
        valid: [
          "foo@bar.com",
          "x@x.au",
          "foo@bar.com.au",
          "foo+bar@bar.com",
          "hans.m\u7aefller@test.com",
          "hans@m\u7aefller.com",
          "test|123@m\u7aefller.com",
          "test123+ext@gmail.com",
          "some.name.midd.leNa.me.and.locality+extension@GoogleMail.com",
          '"foobar"@example.com',
          '"  foo  m\u7aefller "@example.com',
          '"foo\\@bar"@example.com',
          `${"a".repeat(64)}@${"a".repeat(63)}.com`,
          `${"a".repeat(64)}@${"a".repeat(63)}.com`,
          `${"a".repeat(31)}@gmail.com`,
          "test@gmail.com",
          "test.1@gmail.com",
          "test@1337.com",
        ],
        invalid: [
          "invalidemail@",
          "invalid.com",
          "@invalid.com",
          "foo@bar.com.",
          "foo@_bar.com",
          "somename@\uff47\uff4d\uff41\uff49\uff4c.com",
          "foo@bar.co.uk.",
          "z@co.c",
          "\uff47\uff4d\uff41\uff49\uff4c\uff47\uff4d\uff41\uff49\uff4c\uff47\uff4d\uff41\uff49\uff4c\uff47\uff4d\uff41\uff49\uff4c\uff47\uff4d\uff41\uff49\uff4c@gmail.com",
          `${"a".repeat(64)}@${"a".repeat(251)}.com`,
          `${"a".repeat(65)}@${"a".repeat(250)}.com`,
          `${"a".repeat(64)}@${"a".repeat(64)}.com`,
          `${"a".repeat(64)}@${"a".repeat(63)}.${"a".repeat(63)}.${"a".repeat(
            63
          )}.${"a".repeat(58)}.com`,
          "test1@invalid.co\u2006m",
          "test2@invalid.co\u2002m",
          "test3@invalid.co\u2004m",
          "test4@invalid.co\u2005m",
          "test5@invalid.co\u2006m",
          "test6@invalid.co\u2007m",
          "test7@invalid.co\u2008m",
          "test8@invalid.co\u2009m",
          "test9@invalid.co\u200am",
          "test10@invalid.co\u202fm",
          "test11@invalid.co\u205fm",
          "test12@invalid.co\u3000m",
          "test13@invalid.co\u3000m",
          "multiple..dots@stillinvalid.com",
          "test123+invalid! sub_address@gmail.com",
          "gmail...ignores...dots...@gmail.com",
          "ends.with.dot.@gmail.com",
          "multiple..dots@gmail.com",
          'wrong()[]",:;<>@@gmail.com',
          '"wrong()[]",:;<>@@gmail.com',
          "username@domain.com\ufffd",
          "username@domain.com\u00a9",
          "nbsp\u00a0test@test.com",
          "nbsp_test@te\u00a0st.com",
          "nbsp_test@test.co\u00a0m",
          '"foobar@gmail.com',
          '"foo"bar@gmail.com',
          'foo"bar"@gmail.com',
        ],
      });
    });

    it("should validate email addresses with domain specific validation", () => {
      test({
        validator: "isEmail",
        args: [{ domain_specific_validation: true }],
        valid: [
          "foobar@gmail.com",
          "foo.bar@gmail.com",
          "foo.bar@googlemail.com",
          `${"a".repeat(30)}@gmail.com`,
        ],
        invalid: [
          `${"a".repeat(31)}@gmail.com`,
          "test@gmail.com",
          "test.1@gmail.com",
          ".foobar@gmail.com",
        ],
      });
    });

    it("should validate email addresses with underscores in the domain", () => {
      test({
        validator: "isEmail",
        args: [{ allow_underscores: true }],
        valid: ["foobar@my_sarisari_store.typepad.com"],
        invalid: [],
      });
    });

    it("should validate email addresses without UTF8 characters in local part", () => {
      test({
        validator: "isEmail",
        args: [{ allow_utf8_local_part: false }],
        valid: [
          "foo@bar.com",
          "x@x.au",
          "foo@bar.com.au",
          "foo+bar@bar.com",
          "hans@m\u7aefller.com",
          "test|123@m\u7aefller.com",
          "test123+ext@gmail.com",
          "some.name.midd.leNa.me+extension@GoogleMail.com",
          '"foobar"@example.com',
          '"foo\\@bar"@example.com',
          '"  foo  bar  "@example.com',
        ],
        invalid: [
          "invalidemail@",
          "invalid.com",
          "@invalid.com",
          "foo@bar.com.",
          "foo@bar.co.uk.",
          "somename@\uff47\uff4d\uff41\uff49\uff4c.com",
          "hans.m\u7aefller@test.com",
          "z@co.c",
          "t\u00fcst@invalid.com",
          "nbsp\u00a0test@test.com",
        ],
      });
    });

    it("should validate email addresses with display names", () => {
      test({
        validator: "isEmail",
        args: [{ allow_display_name: true }],
        valid: [
          "foo@bar.com",
          "x@x.au",
          "foo@bar.com.au",
          "foo+bar@bar.com",
          "hans.m\u7aefller@test.com",
          "hans@m\u7aefller.com",
          "test|123@m\u7aefller.com",
          "test123+ext@gmail.com",
          "some.name.midd.leNa.me+extension@GoogleMail.com",
          "Some Name <foo@bar.com>",
          "Some Name <x@x.au>",
          "Some Name <foo@bar.com.au>",
          "Some Name <foo+bar@bar.com>",
          "Some Name <hans.m\u7aefller@test.com>",
          "Some Name <hans@m\u7aefller.com>",
          "Some Name <test|123@m\u7aefller.com>",
          "Some Name <test123+ext@gmail.com>",
          "'Foo Bar, Esq'<foo@bar.com>",
          "Some Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Some Middle Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Name<some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Some Name <foo@gmail.com>",
          "Name\ud83c\udf53With\ud83c\udf51Emoji\ud83d\udeb4\u200d\u2640\ufe0f\ud83c\udfc6<test@aftership.com>",
          "\ud83c\udf47\ud83c\udf57\ud83c\udf51<only_emoji@aftership.com>",
          '"<displayNameInBrackets>"<jh@gmail.com>',
          '"\\"quotes\\""<jh@gmail.com>',
          '"name;"<jh@gmail.com>',
          '"name;" <jh@gmail.com>',
        ],
        invalid: [
          "invalidemail@",
          "invalid.com",
          "@invalid.com",
          "foo@bar.com.",
          "foo@bar.co.uk.",
          "Some Name <invalidemail@>",
          "Some Name <invalid.com>",
          "Some Name <@invalid.com>",
          "Some Name <foo@bar.com.>",
          "Some Name <foo@bar.co.uk.>",
          "Some Name foo@bar.co.uk.>",
          "Some Name <foo@bar.co.uk.",
          "Some Name < foo@bar.co.uk >",
          "Name foo@bar.co.uk",
          "Some Name <some..name@gmail.com>",
          "Some Name<emoji_in_address\ud83c\udf48@aftership.com>",
          "invisibleCharacter\u001F<jh@gmail.com>",
          "<displayNameInBrackets><jh@gmail.com>",
          '\\"quotes\\"<jh@gmail.com>',
          '""quotes""<jh@gmail.com>',
          "name;<jh@gmail.com>",
          "    <jh@gmail.com>",
          '"    "<jh@gmail.com>',
        ],
      });
    });

    it("should validate email addresses with required display names", () => {
      test({
        validator: "isEmail",
        args: [{ require_display_name: true }],
        valid: [
          "Some Name <foo@bar.com>",
          "Some Name <x@x.au>",
          "Some Name <foo@bar.com.au>",
          "Some Name <foo+bar@bar.com>",
          "Some Name <hans.m\u7aefller@test.com>",
          "Some Name <hans@m\u7aefller.com>",
          "Some Name <test|123@m\u7aefller.com>",
          "Some Name <test123+ext@gmail.com>",
          "Some Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Some Middle Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Name <some.name.midd.leNa.me+extension@GoogleMail.com>",
          "Name<some.name.midd.leNa.me+extension@GoogleMail.com>",
        ],
        invalid: [
          "some.name.midd.leNa.me+extension@GoogleMail.com",
          "foo@bar.com",
          "x@x.au",
          "foo@bar.com.au",
          "foo+bar@bar.com",
          "hans.m\u7aefller@test.com",
          "hans@m\u7aefller.com",
          "test|123@m\u7aefller.com",
          "test123+ext@gmail.com",
          "invalidemail@",
          "invalid.com",
          "@invalid.com",
          "foo@bar.com.",
          "foo@bar.co.uk.",
          "Some Name <invalidemail@>",
          "Some Name <invalid.com>",
          "Some Name <@invalid.com>",
          "Some Name <foo@bar.com.>",
          "Some Name <foo@bar.co.uk.>",
          "Some Name foo@bar.co.uk.>",
          "Some Name <foo@bar.co.uk.",
          "Some Name < foo@bar.co.uk >",
          "Name foo@bar.co.uk",
        ],
      });
    });

    it("should validate email addresses with allowed IPs", () => {
      test({
        validator: "isEmail",
        args: [{ allow_ip_domain: true }],
        valid: ["email@[123.123.123.123]", "email@255.255.255.255"],
        invalid: [
          "email@0.0.0.256",
          "email@26.0.0.256",
          "email@[266.266.266.266]",
        ],
      });
    });

    it("should not validate email addresses with blacklisted chars in the name", () => {
      test({
        validator: "isEmail",
        args: [{ blacklisted_chars: 'abc"' }],
        valid: ["emil@gmail.com"],
        invalid: [
          "email@gmail.com",
          '"foobr"@example.com',
          '" foo m\u7aefller "@example.com',
          '"foo@br"@example.com',
        ],
      });
    });

    it("should validate really long emails if ignore_max_length is set", () => {
      test({
        validator: "isEmail",
        args: [{ ignore_max_length: false }],
        valid: [],
        invalid: [
          "Deleted-user-id-19430-Team-5051deleted-user-id-19430-team-5051XXXXXX@example.com",
        ],
      });

      test({
        validator: "isEmail",
        args: [{ ignore_max_length: true }],
        valid: [
          "Deleted-user-id-19430-Team-5051deleted-user-id-19430-team-5051XXXXXX@example.com",
        ],
        invalid: [],
      });

      test({
        validator: "isEmail",
        args: [{ ignore_max_length: true }],
        valid: [
          "Deleted-user-id-19430-Team-5051deleted-user-id-19430-team-5051XXXXXX@Deleted-user-id-19430-Team-5051deleted-user-id-19430-team-5051XXXXXX.com",
        ],
        invalid: [],
      });
    });

    it("should not validate email addresses with denylisted domains", () => {
      test({
        validator: "isEmail",
        args: [{ host_blacklist: ["gmail.com", "foo.bar.com"] }],
        valid: ["email@foo.gmail.com"],
        invalid: ["foo+bar@gmail.com", "email@foo.bar.com"],
      });
    });

    it("should allow regular expressions in the host blacklist of isEmail", () => {
      test({
        validator: "isEmail",
        args: [
          {
            host_blacklist: ["bar.com", "foo.com", /\.foo\.com$/],
          },
        ],
        valid: ["email@foobar.com", "email@foo.bar.com", "email@qux.com"],
        invalid: ["email@bar.com", "email@foo.com", "email@a.b.c.foo.com"],
      });
    });

    it("should validate only email addresses with whitelisted domains", () => {
      test({
        validator: "isEmail",
        args: [{ host_whitelist: ["gmail.com", "foo.bar.com"] }],
        valid: ["email@gmail.com", "test@foo.bar.com"],
        invalid: ["foo+bar@test.com", "email@foo.com", "email@bar.com"],
      });
    });

    it("should allow regular expressions in the host whitelist of isEmail", () => {
      test({
        validator: "isEmail",
        args: [
          {
            host_whitelist: ["bar.com", "foo.com", /\.foo\.com$/],
          },
        ],
        valid: ["email@bar.com", "email@foo.com", "email@a.b.c.foo.com"],
        invalid: ["email@foobar.com", "email@foo.bar.com", "email@qux.com"],
      });
    });

    it("should validate URLs", () => {
      test({
        validator: "isURL",
        valid: [
          "foobar.com",
          "www.foobar.com",
          "foobar.com/",
          "valid.au",
          "http://www.foobar.com/",
          "HTTP://WWW.FOOBAR.COM/",
          "https://www.foobar.com/",
          "HTTPS://WWW.FOOBAR.COM/",
          "http://www.foobar.com:23/",
          "http://www.foobar.com:65535/",
          "http://www.foobar.com:5/",
          "https://www.foobar.com/",
          "ftp://www.foobar.com/",
          "http://www.foobar.com/~foobar",
          "http://user:pass@www.foobar.com/",
          "http://user:@www.foobar.com/",
          "http://:pass@www.foobar.com/",
          "http://user@www.foobar.com",
          "http://127.0.0.1/",
          "http://10.0.0.0/",
          "http://189.123.14.13/",
          "http://duckduckgo.com/?q=%2F",
          "http://foobar.com/t$-_.+!*'(),",
          "http://foobar.com/?foo=bar#baz=qux",
          "http://foobar.com?foo=bar",
          "http://foobar.com#baz=qux",
          "http://www.xn--froschgrn-x9a.net/",
          "http://xn--froschgrn-x9a.com/",
          "http://foo--bar.com",
          "http://h\u00f8yfjellet.no",
          "http://xn--j1aac5a4g.xn--j1amh",
          "http://xn------eddceddeftq7bvv7c4ke4c.xn--p1ai",
          "http://\u043a\u0443\u043b\u0456\u043a.\u0443\u043a\u0440",
          "test.com?ref=http://test2.com",
          "http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html",
          "http://[1080:0:0:0:8:800:200C:417A]/index.html",
          "http://[3ffe:2a00:100:7031::1]",
          "http://[1080::8:800:200C:417A]/foo",
          "http://[::192.9.5.5]/ipng",
          "http://[::FFFF:129.144.52.38]:80/index.html",
          "http://[2010:836B:4179::836B:4179]",
          "http://example.com/example.json#/foo/bar",
          "http://1337.com",
        ],
        invalid: [
          "http://localhost:3000/",
          "//foobar.com",
          "xyz://foobar.com",
          "invalid/",
          "invalid.x",
          "invalid.",
          ".com",
          "http://com/",
          "http://300.0.0.1/",
          "mailto:foo@bar.com",
          "rtmp://foobar.com",
          "http://www.xn--.com/",
          "http://xn--.com/",
          "http://www.foobar.com:0/",
          "http://www.foobar.com:70000/",
          "http://www.foobar.com:99999/",
          "http://www.-foobar.com/",
          "http://www.foobar-.com/",
          "http://foobar/# lol",
          "http://foobar/? lol",
          "http://foobar/ lol/",
          "http://lol @foobar.com/",
          "http://lol:lol @foobar.com/",
          "http://lol:lol:lol@foobar.com/",
          "http://lol: @foobar.com/",
          "http://www.foo_bar.com/",
          "http://www.foobar.com/\t",
          "http://@foobar.com",
          "http://:@foobar.com",
          "http://\n@www.foobar.com/",
          "",
          `http://foobar.com/${new Array(2083).join("f")}`,
          "http://*.foo.com",
          "*.foo.com",
          "!.foo.com",
          "http://example.com.",
          "http://localhost:61500this is an invalid url!!!!",
          "////foobar.com",
          "http:////foobar.com",
          "https://example.com/foo/<script>alert('XSS')</script>/",
        ],
      });
    });

    it("should validate URLs with custom protocols", () => {
      test({
        validator: "isURL",
        args: [
          {
            protocols: ["rtmp"],
          },
        ],
        valid: ["rtmp://foobar.com"],
        invalid: ["http://foobar.com"],
      });
    });

    it("should validate file URLs without a host", () => {
      test({
        validator: "isURL",
        args: [
          {
            protocols: ["file"],
            require_host: false,
            require_tld: false,
          },
        ],
        valid: ["file://localhost/foo.txt", "file:///foo.txt", "file:///"],
        invalid: ["http://foobar.com", "file://"],
      });
    });

    it("should validate postgres URLs without a host", () => {
      test({
        validator: "isURL",
        args: [
          {
            protocols: ["postgres"],
            require_host: false,
          },
        ],
        valid: ["postgres://user:pw@/test"],
        invalid: ["http://foobar.com", "postgres://"],
      });
    });

    it("should validate URLs with any protocol", () => {
      test({
        validator: "isURL",
        args: [
          {
            require_valid_protocol: false,
          },
        ],
        valid: ["rtmp://foobar.com", "http://foobar.com", "test://foobar.com"],
        invalid: ["mailto:test@example.com"],
      });
    });

    it("should validate URLs with underscores", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_underscores: true,
          },
        ],
        valid: [
          "http://foo_bar.com",
          "http://pr.example_com.294.example.com/",
          "http://foo__bar.com",
          "http://_.example.com",
        ],
        invalid: [],
      });
    });

    it("should validate URLs that do not have a TLD", () => {
      test({
        validator: "isURL",
        args: [
          {
            require_tld: false,
          },
        ],
        valid: [
          "http://foobar.com/",
          "http://foobar/",
          "http://localhost/",
          "foobar/",
          "foobar",
        ],
        invalid: [],
      });
    });

    it("should validate URLs with a trailing dot option", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_trailing_dot: true,
            require_tld: false,
          },
        ],
        valid: ["http://example.com.", "foobar."],
      });
    });

    it("should validate URLs with column and no port", () => {
      test({
        validator: "isURL",
        valid: ["http://example.com:", "ftp://example.com:"],
        invalid: ["https://example.com:abc"],
      });
    });

    it("should validate sftp protocol URL containing column and no port", () => {
      test({
        validator: "isURL",
        args: [
          {
            protocols: ["sftp"],
          },
        ],
        valid: ["sftp://user:pass@terminal.aws.test.nl:/incoming/things.csv"],
      });
    });

    it("should validate protocol relative URLs", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_protocol_relative_urls: true,
          },
        ],
        valid: ["//foobar.com", "http://foobar.com", "foobar.com"],
        invalid: [
          "://foobar.com",
          "/foobar.com",
          "////foobar.com",
          "http:////foobar.com",
        ],
      });
    });

    it("should not validate URLs with fragments when allow fragments is false", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_fragments: false,
          },
        ],
        valid: ["http://foobar.com", "foobar.com"],
        invalid: ["http://foobar.com#part", "foobar.com#part"],
      });
    });

    it("should not validate URLs with query components when allow query components is false", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_query_components: false,
          },
        ],
        valid: ["http://foobar.com", "foobar.com"],
        invalid: [
          "http://foobar.com?foo=bar",
          "http://foobar.com?foo=bar&bar=foo",
          "foobar.com?foo=bar",
          "foobar.com?foo=bar&bar=foo",
        ],
      });
    });

    it("should not validate protocol relative URLs when require protocol is true", () => {
      test({
        validator: "isURL",
        args: [
          {
            allow_protocol_relative_urls: true,
            require_protocol: true,
          },
        ],
        valid: ["http://foobar.com"],
        invalid: ["//foobar.com", "://foobar.com", "/foobar.com", "foobar.com"],
      });
    });

    it("should let users specify whether URLs require a protocol", () => {
      test({
        validator: "isURL",
        args: [
          {
            require_protocol: true,
          },
        ],
        valid: ["http://foobar.com/"],
        invalid: ["http://localhost/", "foobar.com", "foobar"],
      });
    });

    it("should let users specify a host whitelist", () => {
      test({
        validator: "isURL",
        args: [
          {
            host_whitelist: ["foo.com", "bar.com"],
          },
        ],
        valid: ["http://bar.com/", "http://foo.com/"],
        invalid: ["http://foobar.com", "http://foo.bar.com/", "http://qux.com"],
      });
    });

    it("should allow regular expressions in the host whitelist", () => {
      test({
        validator: "isURL",
        args: [
          {
            host_whitelist: ["bar.com", "foo.com", /\.foo\.com$/],
          },
        ],
        valid: [
          "http://bar.com/",
          "http://foo.com/",
          "http://images.foo.com/",
          "http://cdn.foo.com/",
          "http://a.b.c.foo.com/",
        ],
        invalid: ["http://foobar.com", "http://foo.bar.com/", "http://qux.com"],
      });
    });

    it("should let users specify a host blacklist", () => {
      test({
        validator: "isURL",
        args: [
          {
            host_blacklist: ["foo.com", "bar.com"],
          },
        ],
        valid: ["http://foobar.com", "http://foo.bar.com/", "http://qux.com"],
        invalid: ["http://bar.com/", "http://foo.com/"],
      });
    });

    it("should allow regular expressions in the host blacklist", () => {
      test({
        validator: "isURL",
        args: [
          {
            host_blacklist: ["bar.com", "foo.com", /\.foo\.com$/],
          },
        ],
        valid: ["http://foobar.com", "http://foo.bar.com/", "http://qux.com"],
        invalid: [
          "http://bar.com/",
          "http://foo.com/",
          "http://images.foo.com/",
          "http://cdn.foo.com/",
          "http://a.b.c.foo.com/",
        ],
      });
    });

    it("should allow rejecting urls containing authentication information", () => {
      test({
        validator: "isURL",
        args: [{ disallow_auth: true }],
        valid: ["doe.com"],
        invalid: ["john@doe.com", "john:john@doe.com"],
      });
    });

    it("should accept urls containing authentication information", () => {
      test({
        validator: "isURL",
        args: [{ disallow_auth: false }],
        valid: [
          "user@example.com",
          "user:@example.com",
          "user:password@example.com",
        ],
        invalid: [
          "user:user:password@example.com",
          "@example.com",
          ":@example.com",
          ":example.com",
        ],
      });
    });

    it("should allow user to skip URL length validation", () => {
      test({
        validator: "isURL",
        args: [{ validate_length: false }],
        valid: [
          "http://foobar.com/f",
          `http://foobar.com/${new Array(2083).join("f")}`,
        ],
        invalid: [],
      });
    });

    it("should allow user to configure the maximum URL length", () => {
      test({
        validator: "isURL",
        args: [{ max_allowed_length: 20 }],
        valid: [
          "http://foobar.com/12", // 20 characters
          "http://foobar.com/",
        ],
        invalid: [
          "http://foobar.com/123", // 21 characters
          "http://foobar.com/1234567890",
        ],
      });
    });

    it("should validate URLs with port present", () => {
      test({
        validator: "isURL",
        args: [{ require_port: true }],
        valid: [
          "http://user:pass@www.foobar.com:1",
          "http://user:@www.foobar.com:65535",
          "http://127.0.0.1:23",
          "http://10.0.0.0:256",
          "http://189.123.14.13:256",
          "http://duckduckgo.com:65535?q=%2F",
        ],
        invalid: [
          "http://user:pass@www.foobar.com/",
          "http://user:@www.foobar.com/",
          "http://127.0.0.1/",
          "http://10.0.0.0/",
          "http://189.123.14.13/",
          "http://duckduckgo.com/?q=%2F",
        ],
      });
    });

    it("should validate MAC addresses", () => {
      test({
        validator: "isMACAddress",
        valid: [
          "ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:ab",
          "01:AB:03:04:05:06",
          "A9 C5 D4 9F EB D3",
          "01 02 03 04 05 ab",
          "01-02-03-04-05-ab",
          "0102.0304.05ab",
          "ab:ab:ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:06:07:ab",
          "01:AB:03:04:05:06:07:08",
          "A9 C5 D4 9F EB D3 B6 65",
          "01 02 03 04 05 06 07 ab",
          "01-02-03-04-05-06-07-ab",
          "0102.0304.0506.07ab",
        ],
        invalid: [
          "abc",
          "01:02:03:04:05",
          "01:02:03:04:05:z0",
          "01:02:03:04::ab",
          "1:2:3:4:5:6",
          "AB:CD:EF:GH:01:02",
          "A9C5 D4 9F EB D3",
          "01-02 03:04 05 ab",
          "0102.03:04.05ab",
          "900f/dffs/sdea",
          "01:02:03:04:05:06:07",
          "01:02:03:04:05:06:07:z0",
          "01:02:03:04:05:06::ab",
          "1:2:3:4:5:6:7:8",
          "AB:CD:EF:GH:01:02:03:04",
          "A9C5 D4 9F EB D3 B6 65",
          "01-02 03:04 05 06 07 ab",
          "0102.03:04.0506.07ab",
          "900f/dffs/sdea/54gh",
        ],
      });
      test({
        validator: "isMACAddress",
        args: [
          {
            eui: "48",
          },
        ],
        valid: [
          "ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:ab",
          "01:AB:03:04:05:06",
          "A9 C5 D4 9F EB D3",
          "01 02 03 04 05 ab",
          "01-02-03-04-05-ab",
          "0102.0304.05ab",
        ],
        invalid: [
          "ab:ab:ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:06:07:ab",
          "01:AB:03:04:05:06:07:08",
          "A9 C5 D4 9F EB D3 B6 65",
          "01 02 03 04 05 06 07 ab",
          "01-02-03-04-05-06-07-ab",
          "0102.0304.0506.07ab",
        ],
      });
      test({
        validator: "isMACAddress",
        args: [
          {
            eui: "64",
          },
        ],
        valid: [
          "ab:ab:ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:06:07:ab",
          "01:AB:03:04:05:06:07:08",
          "A9 C5 D4 9F EB D3 B6 65",
          "01 02 03 04 05 06 07 ab",
          "01-02-03-04-05-06-07-ab",
          "0102.0304.0506.07ab",
        ],
        invalid: [
          "ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:ab",
          "01:AB:03:04:05:06",
          "A9 C5 D4 9F EB D3",
          "01 02 03 04 05 ab",
          "01-02-03-04-05-ab",
          "0102.0304.05ab",
        ],
      });
    });

    it("should validate MAC addresses without separator", () => {
      test({
        validator: "isMACAddress",
        args: [
          {
            no_separators: true,
          },
        ],
        valid: [
          "abababababab",
          "FFFFFFFFFFFF",
          "0102030405ab",
          "01AB03040506",
          "abababababababab",
          "FFFFFFFFFFFFFFFF",
          "01020304050607ab",
          "01AB030405060708",
        ],
        invalid: [
          "abc",
          "01:02:03:04:05",
          "01:02:03:04::ab",
          "1:2:3:4:5:6",
          "AB:CD:EF:GH:01:02",
          "ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:ab",
          "01:AB:03:04:05:06",
          "0102030405",
          "01020304ab",
          "123456",
          "ABCDEFGH0102",
          "01:02:03:04:05:06:07",
          "01:02:03:04:05:06::ab",
          "1:2:3:4:5:6:7:8",
          "AB:CD:EF:GH:01:02:03:04",
          "ab:ab:ab:ab:ab:ab:ab:ab",
          "FF:FF:FF:FF:FF:FF:FF:FF",
          "01:02:03:04:05:06:07:ab",
          "01:AB:03:04:05:06:07:08",
          "01020304050607",
          "010203040506ab",
          "12345678",
          "ABCDEFGH01020304",
        ],
      });
      test({
        validator: "isMACAddress",
        args: [
          {
            no_separators: true,
            eui: "48",
          },
        ],
        valid: ["abababababab", "FFFFFFFFFFFF", "0102030405ab", "01AB03040506"],
        invalid: [
          "abababababababab",
          "FFFFFFFFFFFFFFFF",
          "01020304050607ab",
          "01AB030405060708",
        ],
      });
      test({
        validator: "isMACAddress",
        args: [
          {
            no_separators: true,
            eui: "64",
          },
        ],
        valid: [
          "abababababababab",
          "FFFFFFFFFFFFFFFF",
          "01020304050607ab",
          "01AB030405060708",
        ],
        invalid: [
          "abababababab",
          "FFFFFFFFFFFF",
          "0102030405ab",
          "01AB03040506",
        ],
      });
    });

    it("should validate isIPRange", () => {
      test({
        validator: "isIPRange",
        valid: [
          "127.0.0.1/24",
          "0.0.0.0/0",
          "255.255.255.0/32",
          "::/0",
          "::/128",
          "2001::/128",
          "2001:800::/128",
          "::ffff:127.0.0.1/128",
        ],
        invalid: [
          "abc",
          "127.200.230.1/35",
          "127.200.230.1/-1",
          "1.1.1.1/011",
          "1.1.1/24.1",
          "1.1.1.1/01",
          "1.1.1.1/1.1",
          "1.1.1.1/1.",
          "1.1.1.1/1/1",
          "1.1.1.1",
          "::1",
          "::1/164",
          "2001::/240",
          "2001::/-1",
          "2001::/001",
          "2001::/24.1",
          "2001:db8:0000:1:1:1:1:1",
          "::ffff:127.0.0.1",
        ],
      });
      test({
        validator: "isIPRange",
        args: [4],
        valid: [
          "127.0.0.1/1",
          "0.0.0.0/1",
          "255.255.255.255/1",
          "1.2.3.4/1",
          "255.0.0.1/1",
          "0.0.1.1/1",
        ],
        invalid: [
          "abc",
          "::1",
          "2001:db8:0000:1:1:1:1:1",
          "::ffff:127.0.0.1",
          "137.132.10.01",
          "0.256.0.256",
          "255.256.255.256",
        ],
      });
      test({
        validator: "isIPRange",
        args: [6],
        valid: ["::1/1", "2001:db8:0000:1:1:1:1:1/1", "::ffff:127.0.0.1/1"],
        invalid: [
          "abc",
          "127.0.0.1",
          "0.0.0.0",
          "255.255.255.255",
          "1.2.3.4",
          "::ffff:287.0.0.1",
          "::ffff:287.0.0.1/254",
          "%",
          "fe80::1234%",
          "fe80::1234%1%3%4",
          "fe80%fe80%",
        ],
      });
      test({
        validator: "isIPRange",
        args: [10],
        valid: [],
        invalid: [
          "abc",
          "127.0.0.1/1",
          "0.0.0.0/1",
          "255.255.255.255/1",
          "1.2.3.4/1",
          "::1/1",
          "2001:db8:0000:1:1:1:1:1/1",
        ],
      });
    });

    it("should validate FQDN", () => {
      test({
        validator: "isFQDN",
        valid: [
          "domain.com",
          "dom.plato",
          "a.domain.co",
          "foo--bar.com",
          "xn--froschgrn-x9a.com",
          "rebecca.blackfriday",
          "1337.com",
        ],
        invalid: [
          "abc",
          "256.0.0.0",
          "_.com",
          "*.some.com",
          "s!ome.com",
          "domain.com/",
          "/more.com",
          "domain.com\ufffd",
          "domain.co\u00A0m",
          "domain.co\u1680m",
          "domain.co\u2006m",
          "domain.co\u2028m",
          "domain.co\u2029m",
          "domain.co\u202Fm",
          "domain.co\u205Fm",
          "domain.co\u3000m",
          "domain.com\uDC00",
          "domain.co\uEFFFm",
          "domain.co\uFDDAm",
          "domain.co\uFFF4m",
          "domain.com\u00a9",
          "example.0",
          "192.168.0.9999",
          "192.168.0",
        ],
      });
    });
    it("should validate FQDN with trailing dot option", () => {
      test({
        validator: "isFQDN",
        args: [{ allow_trailing_dot: true }],
        valid: ["example.com."],
      });
    });
    it("should invalidate FQDN when not require_tld", () => {
      test({
        validator: "isFQDN",
        args: [{ require_tld: false }],
        invalid: ["example.0", "192.168.0", "192.168.0.9999"],
      });
    });
    it("should validate FQDN when not require_tld but allow_numeric_tld", () => {
      test({
        validator: "isFQDN",
        args: [{ allow_numeric_tld: true, require_tld: false }],
        valid: ["example.0", "192.168.0", "192.168.0.9999"],
      });
    });
    it("should validate FQDN with wildcard option", () => {
      test({
        validator: "isFQDN",
        args: [{ allow_wildcard: true }],
        valid: ["*.example.com", "*.shop.example.com"],
      });
    });
    it("should validate FQDN with required allow_trailing_dot, allow_underscores and allow_numeric_tld options", () => {
      test({
        validator: "isFQDN",
        args: [
          {
            allow_trailing_dot: true,
            allow_underscores: true,
            allow_numeric_tld: true,
          },
        ],
        valid: ["abc.efg.g1h.", "as1s.sad3s.ssa2d."],
      });
    });

    it("should validate alpha strings", () => {
      test({
        validator: "isAlpha",
        valid: ["abc", "ABC", "FoObar"],
        invalid: ["abc1", "  foo  ", "", "\u00c4BC", "F\u00dc\u00fcbar", "J\u00f6n", "Hei\u00df"],
      });
    });

    it("should validate alpha string with ignored characters", () => {
      test({
        validator: "isAlpha",
        args: ["en-US", { ignore: "- /" }], // ignore [space-/]
        valid: ["en-US", "this is a valid alpha string", "us/usa"],
        invalid: [
          "1. this is not a valid alpha string",
          "this$is also not a valid.alpha string",
          "this is also not a valid alpha string.",
        ],
      });

      test({
        validator: "isAlpha",
        args: ["en-US", { ignore: /[\s/-]/g }], // ignore [space -]
        valid: ["en-US", "this is a valid alpha string"],
        invalid: [
          "1. this is not a valid alpha string",
          "this$is also not a valid.alpha string",
          "this is also not a valid alpha string.",
        ],
      });

      test({
        validator: "isAlpha",
        args: ["en-US", { ignore: 1234 }], // invalid ignore matcher
        error: ["alpha"],
      });
    });

    it("should validate Azerbaijani alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["az-AZ"],
        valid: [
          "Az\u0259rbaycan",
          "Bak\u0131",
          "\u00fc\u00f6\u011f\u0131\u0259\u00e7\u015f",
          "sizAz\u0259rbaycanla\u015fd\u0131r\u0131lm\u0131\u015flardans\u0131n\u0131zm\u0131",
          "dahaBirD\u00fczg\u00fcnString",
          "abc\u00e7de\u0259fg\u011fhx\u0131ijkqlmno\u00f6prs\u015ftu\u00fcvyz",
        ],
        invalid: ["r\u0259q\u0259m1", "  foo  ", "", "ab(cd)", "simvol@", "w\u0259kil"],
      });
    });

    it("should validate bulgarian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["bg-BG"],
        valid: ["\u0430\u0431\u0432", "\u0410\u0411\u0412", "\u0436\u0430\u0431\u0430", "\u044f\u0413\u043e\u0414\u0430"],
        invalid: ["abc1", "  foo  ", "", "\u0401\u0427\u041f\u0421", "_\u0430\u0437_\u043e\u0431\u0438\u0447\u0430\u043c_\u043e\u0431\u0443\u0432\u043a\u0438_", "\u0435\u0445\u043e!"],
      });
    });

    it("should validate Bengali alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["bn-BD"],
        valid: ["\u0985\u09df\u09be\u0993\u09b0", "\u09ab\u0997\u09ab\u09a6\u09cd\u09b0\u09a4", "\u09ab\u09a6\u09cd\u09ae\u09cd\u09af\u09a4\u09ad", "\u09ac\u09c7\u09b0\u09c7\u0993\u09ad\u099a\u09a8\u09ad\u09a8", "\u0986\u09ae\u09be\u09b0\u09ac\u09be\u09b8\u0997\u09be"],
        invalid: ["\u09a6\u09be\u09b8\u09e8\u09e9\u09ea", "  \u09a6\u09cd\u0997\u09ab\u09b9\u09cd\u09a8\u09ad  ", "", "(\u0997\u09ab\u09a6)"],
      });
    });

    it("should validate czech alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["cs-CZ"],
        valid: ["\u017elu\u0165ou\u010dk\u00fd", "K\u016e\u0147", "P\u011bl", "\u010e\u00e1belsk\u00e9", "\u00f3dy"],
        invalid: ["\u00e1bc1", "  f\u016fj  ", ""],
      });
    });

    it("should validate slovak alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["sk-SK"],
        valid: [
          "m\u00f4j",
          "\u013e\u00fab\u00edm",
          "m\u00e4k\u010de\u0148",
          "st\u0139p",
          "v\u0155ba",
          "\u0148orimberk",
          "\u0165ava",
          "\u017ean\u00e9ta",
          "\u010e\u00e1belsk\u00e9",
          "\u00f3dy",
        ],
        invalid: ["1moj", "\u4f60\u597d\u4e16\u754c", "  \u041f\u0440\u0438\u0432\u0435\u0442 \u043c\u0438\u0440  ", "\u0645\u0631\u062d\u0628\u0627 \u0627\u0644\u0639\u0627 "],
      });
    });

    it("should validate danish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["da-DK"],
        valid: ["a\u00f8\u00e5", "\u00c6re", "\u00d8re", "\u00c5re"],
        invalid: ["\u00e4bc123", "\u00c4BC11", ""],
      });
    });

    it("should validate dutch alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["nl-NL"],
        valid: ["K\u00e1n", "\u00e9\u00e9n", "v\u00f3\u00f3r", "n\u00fa", "h\u00e9\u00e9l"],
        invalid: ["\u00e4ca ", "abc\u00df", "\u00d8re"],
      });
    });

    it("should validate german alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["de-DE"],
        valid: ["\u00e4bc", "\u00c4BC", "F\u00f6\u00d6b\u00e4r", "Hei\u00df"],
        invalid: ["\u00e4bc1", "  f\u00f6\u00f6  ", ""],
      });
    });

    it("should validate hungarian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["hu-HU"],
        valid: ["\u00e1rv\u00edzt\u0171r\u0151t\u00fck\u00f6rf\u00far\u00f3g\u00e9p", "\u00c1RV\u00cdZT\u0170R\u0150T\u00dcK\u00d6RF\u00daR\u00d3G\u00c9P"],
        invalid: ["\u00e4bc1", "  f\u00e4\u00f6  ", "Hei\u00df", ""],
      });
    });

    it("should validate portuguese alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["pt-PT"],
        valid: ["pal\u00edndromo", "\u00f3rg\u00e3o", "qw\u00e9rty\u00fa\u00e3o", "\u00e0\u00e4\u00e3c\u00eb\u00fc\u00ef\u00c4\u00cf\u00dc"],
        invalid: ["12abc", "Hei\u00df", "\u00d8re", "\u00e6\u00f8\u00e5", ""],
      });
    });

    it("should validate italian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["it-IT"],
        valid: [
          "\u00e0\u00e9\u00e8\u00ec\u00ee\u00f3\u00f2\u00f9",
          "correnti",
          "DEFINIZIONE",
          "compilazione",
          "metr\u00f3",
          "p\u00e8sca",
          "P\u00c9SCA",
          "gen\u00ee",
        ],
        invalid: ["\u00e4bc123", "\u00c4BC11", "\u00e6\u00f8\u00e5", ""],
      });
    });

    it("should validate Japanese alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["ja-JP"],
        valid: [
          "\u3042\u3044\u3046\u3048\u304a",
          "\u304c\u304e\u3050\u3052\u3054",
          "\u3041\u3043\u3045\u3047\u3049",
          "\u30a2\u30a4\u30a6\u30a8\u30aa",
          "\u30a1\u30a3\u30a5\u30a7",
          "\uff71\uff72\uff73\uff74\uff75",
          "\u543e\u8f29\u306f\u732b\u3067\u3042\u308b",
          "\u81e5\u85aa\u5617\u80c6",
          "\u65b0\u4e16\u7d00\u30a8\u30f4\u30a1\u30f3\u30b2\u30ea\u30aa\u30f3",
          "\u5929\u56fd\u3068\u5730\u7344",
          "\u4e03\u4eba\u306e\u4f8d",
          "\u30b7\u30f3\u30fb\u30a6\u30eb\u30c8\u30e9\u30de\u30f3",
        ],
        invalid: ["\u3042\u3044\u3046123", "abc\u3042\u3044\u3046", "\uff11\uff19\uff18\uff14"],
      });
    });

    it("should validate kazakh alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["kk-KZ"],
        valid: [
          "\u0421\u04d9\u043b\u0435\u043c",
          "\u049b\u0430\u043d\u0430\u0493\u0430\u0442\u0442\u0430\u043d\u0434\u044b\u0440\u044b\u043b\u043c\u0430\u0493\u0430\u043d\u0434\u044b\u049b\u0442\u0430\u0440\u044b\u04a3\u044b\u0437\u0434\u0430\u043d",
          "\u041a\u0435\u0448\u0456\u0440\u0456\u04a3\u0456\u0437",
          "\u04e8\u043a\u0456\u043d\u0456\u0448\u043a\u0435",
          "\u049a\u0430\u0439\u0442\u0430\u043b\u0430\u04a3\u044b\u0437\u0448\u044b",
          "\u0430\u0493\u044b\u043b\u0448\u044b\u043d\u0448\u0430",
          "\u0442\u04af\u0441\u0456\u043d\u0431\u0435\u0434\u0456\u043c",
        ],
        invalid: ["\u041a\u0435\u0448\u0456\u0440\u0456\u04a3\u0456\u04371", "  \u041a\u0435\u0442 \u0431\u0430\u0440  ", "\u0645\u0631\u062d\u0628\u0627 \u0627\u0644\u0639\u0627"],
      });
    });

    it("should validate Vietnamese alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["vi-VN"],
        valid: ["thi\u1ebfn", "nghi\u00eang", "xin", "ch\u00e0o", "th\u1ebf", "gi\u1edbi"],
        invalid: ["th\u1ea7y3", "Ba g\u00e0", ""],
      });
    });

    it("should validate arabic alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["ar"],
        valid: ["\u0623\u0628\u062a", "\u0627\u064e\u0628\u0650\u062a\u064e\u062b\u0651\u062c\u064b"],
        invalid: [
          "\u0661\u0662\u0663\u0623\u0628\u062a",
          "\u0661\u0662\u0663",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "F\u00dc\u00fcbar",
          "J\u00f6n",
          "Hei\u00df",
        ],
      });
    });

    it("should validate farsi alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["fa-IR"],
        valid: ["\u067e\u062f\u0631", "\u0645\u0627\u062f\u0631", "\u0628\u0631\u0627\u062f\u0631", "\u062e\u0648\u0627\u0647\u0631"],
        invalid: [
          "\u0641\u0627\u0631\u0633\u06cc\u06f1\u06f2\u06f3",
          "\u06f1\u06f6\u06f4",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "F\u00dc\u00fcbar",
          "J\u00f6n",
          "Hei\u00df",
        ],
      });
    });

    it("should validate finnish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["fi-FI"],
        valid: ["\u00e4iti", "\u00d6ljy", "\u00c5ke", "test\u00d6"],
        invalid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ", "\u00e4\u00f6\u00e5123", ""],
      });
    });

    it("should validate kurdish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["ku-IQ"],
        valid: ["\u0626\u0624\u06a4\u06af\u06ce", "\u06a9\u0648\u0631\u062f\u0633\u062a\u0627\u0646"],
        invalid: [
          "\u0626\u0624\u06a4\u06af\u06ce\u0661\u0662\u0663",
          "\u0661\u0662\u0663",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "F\u00dc\u00fcbar",
          "J\u00f6n",
          "Hei\u00df",
        ],
      });
    });

    it("should validate norwegian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["nb-NO"],
        valid: ["a\u00f8\u00e5", "\u00c6re", "\u00d8re", "\u00c5re"],
        invalid: ["\u00e4bc123", "\u00c4BC11", ""],
      });
    });

    it("should validate polish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["pl-PL"],
        valid: [
          "kresk\u0105",
          "zamkni\u0119te",
          "zwyk\u0142e",
          "kropk\u0105",
          "przyj\u0119\u0142y",
          "\u015bwi\u0119ty",
          "Pozw\u00f3l",
        ],
        invalid: ["12\u0159i\u010f ", "bl\u00e9!!", "f\u00f6\u00f6!2!"],
      });
    });

    it("should validate serbian cyrillic alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["sr-RS"],
        valid: ["\u0428\u045b\u0436\u0402\u0459\u0415", "\u0427\u041f\u0421\u0422\u040b\u040f"],
        invalid: ["\u0159i\u010f ", "bl\u00e933!!", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate serbian latin alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["sr-RS@latin"],
        valid: ["\u0160Aab\u010d\u0161\u0111\u0107\u017e", "\u0160ATRO\u0106\u010d\u0111\u0161"],
        invalid: ["12\u0159i\u010f ", "bl\u00e9!!", "f\u00f6\u00f6!2!"],
      });
    });

    it("should validate spanish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["es-ES"],
        valid: ["\u00e1bc\u00f3", "\u00c1BC\u00d3", "dorm\u00eds", "volv\u00e9s", "espa\u00f1ol"],
        invalid: ["\u00e4ca ", "abc\u00df", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate swedish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["sv-SE"],
        valid: ["religi\u00f6s", "stj\u00e4la", "v\u00e4stg\u00f6te", "\u00c5re"],
        invalid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ", "religi\u00f6s23", ""],
      });
    });

    it("should validate defined arabic locales alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["ar-SY"],
        valid: ["\u0623\u0628\u062a", "\u0627\u064e\u0628\u0650\u062a\u064e\u062b\u0651\u062c\u064b"],
        invalid: [
          "\u0661\u0662\u0663\u0623\u0628\u062a",
          "\u0661\u0662\u0663",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "F\u00dc\u00fcbar",
          "J\u00f6n",
          "Hei\u00df",
        ],
      });
    });

    it("should validate turkish alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["tr-TR"],
        valid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ"],
        invalid: [
          "0A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ1",
          "  A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ  ",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "Hei\u00df",
        ],
      });
    });

    it("should validate urkrainian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["uk-UA"],
        valid: ["\u0410\u0411\u0412\u0413\u0490\u0414\u0415\u0404\u0416\u0417\u0418I\u0407\u0419\u041a\u041b\u041c\u041d\u041e\u041f\u0420\u0421\u0422\u0423\u0424\u0425\u0426\u0428\u0429\u042c\u042e\u042f"],
        invalid: [
          "0A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ1",
          "  A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ  ",
          "abc1",
          "  foo  ",
          "",
          "\u00c4BC",
          "Hei\u00df",
          "\u042b\u044b\u042a\u044a\u042d\u044d",
        ],
      });
    });

    it("should validate greek alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["el-GR"],
        valid: [
          "\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c2\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9",
          "\u0391\u0392\u0393\u0394\u0395\u0396\u0397\u0398\u0399\u039a\u039b\u039c\u039d\u039e\u039f\u03a0\u03a1\u03a3\u03a4\u03a5\u03a6\u03a7\u03a8\u03a9",
          "\u03ac\u03ad\u03ae\u03af\u03b0\u03ca\u03cb\u03cc\u03cd\u03ce",
          "\u0386\u0388\u0389\u038a\u03aa\u03ab\u038e\u038f",
        ],
        invalid: [
          "0A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ1",
          "  A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ  ",
          "\u00c4BC",
          "Hei\u00df",
          "\u042b\u044b\u042a\u044a\u042d\u044d",
          "120",
          "j\u03b1ck\u03b3",
        ],
      });
    });

    it("should validate Hebrew alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["he"],
        valid: ["\u05d1\u05d3\u05d9\u05e7\u05d4", "\u05e9\u05dc\u05d5\u05dd"],
        invalid: ["\u05d1\u05d3\u05d9\u05e7\u05d4123", "  foo  ", "abc1", ""],
      });
    });

    it("should validate Hindi alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["hi-IN"],
        valid: [
          "\u0905\u0924\u0905\u092a\u0928\u093e\u0905\u092a\u0928\u0940\u0905\u092a\u0928\u0947\u0905\u092d\u0940\u0905\u0902\u0926\u0930\u0906\u0926\u093f\u0906\u092a\u0907\u0924\u094d\u092f\u093e\u0926\u093f\u0907\u0928\u0907\u0928\u0915\u093e\u0907\u0928\u094d\u0939\u0940\u0902\u0907\u0928\u094d\u0939\u0947\u0902\u0907\u0928\u094d\u0939\u094b\u0902\u0907\u0938\u0907\u0938\u0915\u093e\u0907\u0938\u0915\u0940\u0907\u0938\u0915\u0947\u0907\u0938\u092e\u0947\u0902\u0907\u0938\u0940\u0907\u0938\u0947\u0909\u0928\u0909\u0928\u0915\u093e\u0909\u0928\u0915\u0940\u0909\u0928\u0915\u0947\u0909\u0928\u0915\u094b\u0909\u0928\u094d\u0939\u0940\u0902\u0909\u0928\u094d\u0939\u0947\u0902\u0909\u0928\u094d\u0939\u094b\u0902\u0909\u0938\u0909\u0938\u0915\u0947\u0909\u0938\u0940\u0909\u0938\u0947\u090f\u0915\u090f\u0935\u0902\u090f\u0938\u0910\u0938\u0947\u0914\u0930\u0915\u0908\u0915\u0930\u0915\u0930\u0924\u093e\u0915\u0930\u0924\u0947\u0915\u0930\u0928\u093e\u0915\u0930\u0928\u0947\u0915\u0930\u0947\u0902\u0915\u0939\u0924\u0947\u0915\u0939\u093e\u0915\u093e\u0915\u093e\u095e\u0940\u0915\u093f\u0915\u093f\u0924\u0928\u093e\u0915\u093f\u0928\u094d\u0939\u0947\u0902\u0915\u093f\u0928\u094d\u0939\u094b\u0902\u0915\u093f\u092f\u093e\u0915\u093f\u0930\u0915\u093f\u0938\u0915\u093f\u0938\u0940\u0915\u093f\u0938\u0947\u0915\u0940\u0915\u0941\u091b\u0915\u0941\u0932\u0915\u0947\u0915\u094b\u0915\u094b\u0908\u0915\u094c\u0928\u0915\u094c\u0928\u0938\u093e\u0917\u092f\u093e\u0918\u0930\u091c\u092c\u091c\u0939\u093e\u0901\u091c\u093e\u091c\u093f\u0924\u0928\u093e\u091c\u093f\u0928\u091c\u093f\u0928\u094d\u0939\u0947\u0902\u091c\u093f\u0928\u094d\u0939\u094b\u0902\u091c\u093f\u0938\u091c\u093f\u0938\u0947\u091c\u0940\u0927\u0930\u091c\u0948\u0938\u093e\u091c\u0948\u0938\u0947\u091c\u094b\u0924\u0915\u0924\u092c\u0924\u0930\u0939\u0924\u093f\u0928\u0924\u093f\u0928\u094d\u0939\u0947\u0902\u0924\u093f\u0928\u094d\u0939\u094b\u0902\u0924\u093f\u0938\u0924\u093f\u0938\u0947\u0924\u094b\u0925\u093e\u0925\u0940\u0925\u0947\u0926\u092c\u093e\u0930\u093e\u0926\u093f\u092f\u093e\u0926\u0941\u0938\u0930\u093e\u0926\u0942\u0938\u0930\u0947\u0926\u094b\u0926\u094d\u0935\u093e\u0930\u093e\u0928\u0928\u0915\u0947\u0928\u0939\u0940\u0902\u0928\u093e\u0928\u093f\u0939\u093e\u092f\u0924\u0928\u0940\u091a\u0947\u0928\u0947\u092a\u0930\u092a\u0939\u0932\u0947\u092a\u0942\u0930\u093e\u092a\u0947\u092b\u093f\u0930\u092c\u0928\u0940\u092c\u0939\u0940\u092c\u0939\u0941\u0924\u092c\u093e\u0926\u092c\u093e\u0932\u093e\u092c\u093f\u0932\u0915\u0941\u0932\u092d\u0940\u092d\u0940\u0924\u0930\u092e\u0917\u0930\u092e\u093e\u0928\u094b\u092e\u0947\u092e\u0947\u0902\u092f\u0926\u093f\u092f\u0939\u092f\u0939\u093e\u0901\u092f\u0939\u0940\u092f\u093e\u092f\u093f\u0939\u092f\u0947\u0930\u0916\u0947\u0902\u0930\u0939\u093e\u0930\u0939\u0947\u0931\u094d\u0935\u093e\u0938\u093e\u0932\u093f\u090f\u0932\u093f\u092f\u0947\u0932\u0947\u0915\u093f\u0928\u0935\u0935\u095a\u0948\u0930\u0939\u0935\u0930\u094d\u0917\u0935\u0939\u0935\u0939\u093e\u0901\u0935\u0939\u0940\u0902\u0935\u093e\u0932\u0947\u0935\u0941\u0939\u0935\u0947\u0935\u094b\u0938\u0915\u0924\u093e\u0938\u0915\u0924\u0947\u0938\u092c\u0938\u0947\u0938\u092d\u0940\u0938\u093e\u0925\u0938\u093e\u092c\u0941\u0924\u0938\u093e\u092d\u0938\u093e\u0930\u093e\u0938\u0947\u0938\u094b\u0938\u0902\u0917\u0939\u0940\u0939\u0941\u0906\u0939\u0941\u0908\u0939\u0941\u090f\u0939\u0948\u0939\u0948\u0902\u0939\u094b\u0939\u094b\u0924\u093e\u0939\u094b\u0924\u0940\u0939\u094b\u0924\u0947\u0939\u094b\u0928\u093e\u0939\u094b\u0928\u0947",
          "\u0907\u0928\u094d\u0939\u0947\u0902",
        ],
        invalid: ["\u0905\u0924\u0966\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f", "\u0905\u0924 12", " \u0905\u0924 ", "abc1", "abc", ""],
      });
    });

    it("should validate persian alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["fa-IR"],
        valid: ["\u062a\u0633\u062a", "\u0639\u0632\u06cc\u0632\u0645", "\u062d"],
        invalid: ["\u062a\u0633\u062a 1", "  \u0639\u0632\u06cc\u0632\u0645  ", ""],
      });
    });

    it("should validate Thai alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["th-TH"],
        valid: ["\u0e2a\u0e27\u0e31\u0e2a\u0e14\u0e35", "\u0e22\u0e34\u0e19\u0e14\u0e35\u0e15\u0e49\u0e2d\u0e19\u0e23\u0e31\u0e1a \u0e40\u0e17\u0e2a\u0e40\u0e04\u0e2a"],
        invalid: ["\u0e2a\u0e27\u0e31\u0e2a\u0e14\u0e35Hi", "123 \u0e22\u0e34\u0e19\u0e14\u0e35\u0e15\u0e49\u0e2d\u0e19\u0e23\u0e31\u0e1a", "\u0e22\u0e34\u0e19\u0e14\u0e35\u0e15\u0e49\u0e2d\u0e19\u0e23\u0e31\u0e1a-\u0e51\u0e52\u0e53"],
      });
    });

    it("should validate Korea alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["ko-KR"],
        valid: [
          "\u3131",
          "\u3151",
          "\u3131\u3134\u3137\u314f\u3155",
          "\uc138\uc885\ub300\uc655",
          "\ub098\ub78f\ub9d0\uc2f8\ubbf8\ub4d5\uadc1\uc5d0\ub2ec\uc544\ubb38\uc790\uc640\ub85c\uc11c\ub974\uc0ac\ub9db\ub514\uc544\ub2c8\ud560\uc384",
        ],
        invalid: [
          "abc",
          "123",
          "\ud765\uc120\ub300\uc6d0\uad70 \ubb38\ud638\uac1c\ubc29",
          "1592\ub144\uc784\uc9c4\uc65c\ub780",
          "\ub300\ud55c\ubbfc\uad6d!",
        ],
      });
    });

    it("should validate Sinhala alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["si-LK"],
        valid: ["\u0da0\u0dad\u0dd4\u0dbb", "\u0d9a\u0da0\u0da7\u0daf\u0db6", "\u0d8e\u0d8f\u0daf\u0dcf\u0ddb\u0db4\u0dc3\u0dd4\u0d9c\u0ddc"],
        invalid: ["\u0b86\u0b90\u0905\u0924\u0d9a", "\u0d9a\u0da0\u0da7 12", " \u0d8e ", "abc1", "abc", ""],
      });
    });

    it("should validate Esperanto alpha strings", () => {
      test({
        validator: "isAlpha",
        args: ["eo"],
        valid: [
          "saluton",
          "e\u0125o\u015dan\u011do\u0109iu\u0135a\u016dde",
          "E\u0124O\u015cAN\u011cO\u0108IU\u0134A\u016cDE",
          "Esperanto",
          "La\u016dLudovikoZamenhofBongustasFre\u015da\u0108e\u0125aMan\u011da\u0135oKunSpicoj",
        ],
        invalid: ["qwxyz", "1887", "qwxyz 1887"],
      });
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isAlpha",
        args: ["is-NOT"],
        error: ["abc", "ABC"],
      });
    });

    it("should validate alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        valid: ["abc123", "ABC11"],
        invalid: ["abc ", "foo!!", "\u00c4BC", "F\u00dc\u00fcbar", "J\u00f6n"],
      });
    });

    it("should validate alphanumeric string with ignored characters", () => {
      test({
        validator: "isAlphanumeric",
        args: ["en-US", { ignore: "@_- " }], // ignore [@ space _ -]
        valid: [
          "Hello@123",
          "this is a valid alphaNumeric string",
          "En-US @ alpha_numeric",
        ],
        invalid: ["In*Valid", "hello$123", "{invalid}"],
      });

      test({
        validator: "isAlphanumeric",
        args: ["en-US", { ignore: /[\s/-]/g }], // ignore [space -]
        valid: ["en-US", "this is a valid alphaNumeric string"],
        invalid: ["INVALID$ AlphaNum Str", "hello@123", "abc*123"],
      });

      test({
        validator: "isAlphanumeric",
        args: ["en-US", { ignore: 1234 }], // invalid ignore matcher (ignore should be instance of a String or RegExp)
        error: ["alpha"],
      });
    });

    it("should validate defined english aliases", () => {
      test({
        validator: "isAlphanumeric",
        args: ["en-GB"],
        valid: ["abc123", "ABC11"],
        invalid: ["abc ", "foo!!", "\u00c4BC", "F\u00dc\u00fcbar", "J\u00f6n"],
      });
    });

    it("should validate Azerbaijani alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["az-AZ"],
        valid: ["Az\u0259rbaycan", "Bak\u0131", "abc1", "abc\u00e72", "3k\u0259r\u02594k\u0259r\u0259"],
        invalid: ["  foo1  ", "", "ab(cd)", "simvol@", "w\u0259kil"],
      });
    });

    it("should validate bulgarian alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["bg-BG"],
        valid: ["\u0430\u0431\u04321", "4\u0410\u04115\u04126", "\u0436\u0430\u0431\u0430", "\u044f\u0413\u043e\u0414\u04302", "\u0439\u042e\u044f", "123"],
        invalid: [" ", "789  ", "hello000"],
      });
    });

    it("should validate Bengali alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["bn-BD"],
        valid: [
          "\u09a6\u09cd\u0997\u099c\u09cd\u099e\u09b9\u09cd\u09b0\u09a4\u09cd\u09af\u09e7\u09e8\u09e9",
          "\u09a6\u09cd\u0997\u0997\u09ab\u09ee\u09ef\u09e6",
          "\u099a\u09ac\u09e9\u09ec\u09eb\u09ad\u09ac\u099a",
          "\u09e7\u09e8\u09e9\u09ea",
          "\u09e9\u09ea\u09e8\u09e9\u09ea\u09a6\u09ab\u099c\u09cd\u099e\u09a6\u09ab",
        ],
        invalid: [" ", "\u09e7\u09e8\u09e9  ", "hel\u09e9\u09e80"],
      });
    });

    it("should validate czech alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["cs-CZ"],
        valid: ["\u0159i\u0165123", "K\u016e\u014711"],
        invalid: ["\u0159i\u010f ", "bl\u00e9!!"],
      });
    });

    it("should validate slovak alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["sk-SK"],
        valid: [
          "1m\u00f4j",
          "2\u013e\u00fab\u00edm",
          "3m\u00e4k\u010de\u0148",
          "4st\u0139p",
          "5v\u0155ba",
          "6\u0148orimberk",
          "7\u0165ava",
          "8\u017ean\u00e9ta",
          "9\u010e\u00e1belsk\u00e9",
          "10\u00f3dy",
        ],
        invalid: ["1moj!", "\u4f60\u597d\u4e16\u754c", "  \u041f\u0440\u0438\u0432\u0435\u0442 \u043c\u0438\u0440  "],
      });
    });

    it("should validate danish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["da-DK"],
        valid: ["\u00c6\u00d8\u00c5123", "\u00c6re321", "321\u00d8re", "123\u00c5re"],
        invalid: ["\u00e4bc123", "\u00c4BC11", ""],
      });
    });

    it("should validate dutch alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["nl-NL"],
        valid: ["K\u00e1n123", "\u00e9\u00e9n354", "v4\u00f3\u00f3r", "n\u00fa234", "h\u00e954\u00e9l"],
        invalid: ["1\u00e4ca ", "ab3c\u00df", "\u00d8re"],
      });
    });

    it("should validate finnish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["fi-FI"],
        valid: ["\u00e4iti124", "\u00d6LJY1234", "123\u00c5ke", "451\u00e5\u00e523"],
        invalid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ", "foo!!", ""],
      });
    });

    it("should validate german alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["de-DE"],
        valid: ["\u00e4bc123", "\u00c4BC11"],
        invalid: ["\u00e4ca ", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate hungarian alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["hu-HU"],
        valid: ["0\u00e1rv\u00edzt\u0171r\u0151t\u00fck\u00f6rf\u00far\u00f3g\u00e9p123", "0\u00c1RV\u00cdZT\u0170R\u0150T\u00dcK\u00d6RF\u00daR\u00d3G\u00c9P123"],
        invalid: ["1id\u0151\u00far!", "\u00e4bc1", "  f\u00e4\u00f6  ", "Hei\u00df!", ""],
      });
    });

    it("should validate portuguese alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["pt-PT"],
        valid: ["pal\u00edndromo", "2\u00f3rg\u00e3o", "qw\u00e9rty\u00fa\u00e3o9", "\u00e0\u00e4\u00e3c\u00eb4\u00fc\u00ef\u00c4\u00cf\u00dc"],
        invalid: ["!abc", "Hei\u00df", "\u00d8re", "\u00e6\u00f8\u00e5", ""],
      });
    });

    it("should validate italian alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["it-IT"],
        valid: [
          "123\u00e0\u00e9\u00e8\u00ec\u00ee\u00f3\u00f2\u00f9",
          "123correnti",
          "DEFINIZIONE321",
          "compil123azione",
          "met23r\u00f3",
          "p\u00e8s56ca",
          "P\u00c9S45CA",
          "gen45\u00ee",
        ],
        invalid: ["\u00e4bc123", "\u00c4BC11", "\u00e6\u00f8\u00e5", ""],
      });
    });

    it("should validate spanish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["es-ES"],
        valid: ["\u00e1bc\u00f3123", "\u00c1BC\u00d311"],
        invalid: ["\u00e4ca ", "abc\u00df", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate Vietnamese alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["vi-VN"],
        valid: ["Th\u1ea7y3", "3G\u00e0"],
        invalid: ["toang!", "C\u1eadu V\u00e0ng"],
      });
    });

    it("should validate arabic alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["ar"],
        valid: ["\u0623\u0628\u062a123", "\u0623\u0628\u062a\u064e\u064f\u0650\u0661\u0662\u0663"],
        invalid: ["\u00e4ca ", "abc\u00df", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate Hindi alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["hi-IN"],
        valid: [
          "\u0905\u0924\u0905\u092a\u0928\u093e\u0905\u092a\u0928\u0940\u0905\u092a\u0928\u0947\u0905\u092d\u0940\u0905\u0902\u0926\u0930\u0906\u0926\u093f\u0906\u092a\u0907\u0924\u094d\u092f\u093e\u0926\u093f\u0907\u0928\u0907\u0928\u0915\u093e\u0907\u0928\u094d\u0939\u0940\u0902\u0907\u0928\u094d\u0939\u0947\u0902\u0907\u0928\u094d\u0939\u094b\u0902\u0907\u0938\u0907\u0938\u0915\u093e\u0907\u0938\u0915\u0940\u0907\u0938\u0915\u0947\u0907\u0938\u092e\u0947\u0902\u0907\u0938\u0940\u0907\u0938\u0947\u0909\u0928\u0909\u0928\u0915\u093e\u0909\u0928\u0915\u0940\u0909\u0928\u0915\u0947\u0909\u0928\u0915\u094b\u0909\u0928\u094d\u0939\u0940\u0902\u0909\u0928\u094d\u0939\u0947\u0902\u0909\u0928\u094d\u0939\u094b\u0902\u0909\u0938\u0909\u0938\u0915\u0947\u0909\u0938\u0940\u0909\u0938\u0947\u090f\u0915\u090f\u0935\u0902\u090f\u0938\u0910\u0938\u0947\u0914\u0930\u0915\u0908\u0915\u0930\u0915\u0930\u0924\u093e\u0915\u0930\u0924\u0947\u0915\u0930\u0928\u093e\u0915\u0930\u0928\u0947\u0915\u0930\u0947\u0902\u0915\u0939\u0924\u0947\u0915\u0939\u093e\u0915\u093e\u0915\u093e\u095e\u0940\u0915\u093f\u0915\u093f\u0924\u0928\u093e\u0915\u093f\u0928\u094d\u0939\u0947\u0902\u0915\u093f\u0928\u094d\u0939\u094b\u0902\u0915\u093f\u092f\u093e\u0915\u093f\u0930\u0915\u093f\u0938\u0915\u093f\u0938\u0940\u0915\u093f\u0938\u0947\u0915\u0940\u0915\u0941\u091b\u0915\u0941\u0932\u0915\u0947\u0915\u094b\u0915\u094b\u0908\u0915\u094c\u0928\u0915\u094c\u0928\u0938\u093e\u0917\u092f\u093e\u0918\u0930\u091c\u092c\u091c\u0939\u093e\u0901\u091c\u093e\u091c\u093f\u0924\u0928\u093e\u091c\u093f\u0928\u091c\u093f\u0928\u094d\u0939\u0947\u0902\u091c\u093f\u0928\u094d\u0939\u094b\u0902\u091c\u093f\u0938\u091c\u093f\u0938\u0947\u091c\u0940\u0927\u0930\u091c\u0948\u0938\u093e\u091c\u0948\u0938\u0947\u091c\u094b\u0924\u0915\u0924\u092c\u0924\u0930\u0939\u0924\u093f\u0928\u0924\u093f\u0928\u094d\u0939\u0947\u0902\u0924\u093f\u0928\u094d\u0939\u094b\u0902\u0924\u093f\u0938\u0924\u093f\u0938\u0947\u0924\u094b\u0925\u093e\u0925\u0940\u0925\u0947\u0926\u092c\u093e\u0930\u093e\u0926\u093f\u092f\u093e\u0926\u0941\u0938\u0930\u093e\u0926\u0942\u0938\u0930\u0947\u0926\u094b\u0926\u094d\u0935\u093e\u0930\u093e\u0928\u0928\u0915\u0947\u0928\u0939\u0940\u0902\u0928\u093e\u0928\u093f\u0939\u093e\u092f\u0924\u0928\u0940\u091a\u0947\u0928\u0947\u092a\u0930\u092a\u0939\u0932\u0947\u092a\u0942\u0930\u093e\u092a\u0947\u092b\u093f\u0930\u092c\u0928\u0940\u092c\u0939\u0940\u092c\u0939\u0941\u0924\u092c\u093e\u0926\u092c\u093e\u0932\u093e\u092c\u093f\u0932\u0915\u0941\u0932\u092d\u0940\u092d\u0940\u0924\u0930\u092e\u0917\u0930\u092e\u093e\u0928\u094b\u092e\u0947\u092e\u0947\u0902\u092f\u0926\u093f\u092f\u0939\u092f\u0939\u093e\u0901\u092f\u0939\u0940\u092f\u093e\u092f\u093f\u0939\u092f\u0947\u0930\u0916\u0947\u0902\u0930\u0939\u093e\u0930\u0939\u0947\u0931\u094d\u0935\u093e\u0938\u093e\u0932\u093f\u090f\u0932\u093f\u092f\u0947\u0932\u0947\u0915\u093f\u0928\u0935\u0935\u095a\u0948\u0930\u0939\u0935\u0930\u094d\u0917\u0935\u0939\u0935\u0939\u093e\u0901\u0935\u0939\u0940\u0902\u0935\u093e\u0932\u0947\u0935\u0941\u0939\u0935\u0947\u0935\u094b\u0938\u0915\u0924\u093e\u0938\u0915\u0924\u0947\u0938\u092c\u0938\u0947\u0938\u092d\u0940\u0938\u093e\u0925\u0938\u093e\u092c\u0941\u0924\u0938\u093e\u092d\u0938\u093e\u0930\u093e\u0938\u0947\u0938\u094b\u0938\u0902\u0917\u0939\u0940\u0939\u0941\u0906\u0939\u0941\u0908\u0939\u0941\u090f\u0939\u0948\u0939\u0948\u0902\u0939\u094b\u0939\u094b\u0924\u093e\u0939\u094b\u0924\u0940\u0939\u094b\u0924\u0947\u0939\u094b\u0928\u093e\u0939\u094b\u0928\u0947\u0966\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f",
          "\u0907\u0928\u094d\u0939\u0947\u0902\u096a\u096b\u096c\u096d\u096e\u096f",
        ],
        invalid: ["\u0905\u0924 \u0966\u0968\u0969\u096a\u096b\u096c\u096d\u096e\u096f", " \u0969\u096a\u096b\u096c\u096d\u096e\u096f", "12 ", " \u0905\u0924 ", "abc1", "abc", ""],
      });
    });

    it("should validate farsi alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["fa-IR"],
        valid: ["\u067e\u0627\u0631\u0633\u06cc\u06f1\u06f2\u06f3", "\u06f1\u06f4\u06f5\u06f6", "\u0645\u0698\u06af\u0627\u06469"],
        invalid: ["\u00e4ca ", "abc\u00df\u0629", "f\u00f6\u00f6!!", "\u0664\u0665\u0666"],
      });
    });

    it("should validate Japanese alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["ja-JP"],
        valid: [
          "\u3042\u3044\u3046\u3048\u304a123",
          "123\u304c\u304e\u3050\u3052\u3054",
          "\u3041\u3043\u3045\u3047\u3049",
          "\u30a2\u30a4\u30a6\u30a8\u30aa",
          "\u30a1\u30a3\u30a5\u30a7",
          "\uff71\uff72\uff73\uff74\uff75",
          "\uff12\uff10\u4e16\u7d00\u5c11\u5e74",
          "\u83ef\u6c0f\uff14\uff15\uff11\u5ea6",
        ],
        invalid: [" \u3042\u3044\u3046123 ", "abc\u3042\u3044\u3046", "\u751f\u304d\u308d!!"],
      });
    });

    it("should validate kazakh alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["kk-KZ"],
        valid: [
          "\u0421\u04d9\u043b\u0435\u043c777",
          "123\u0411\u04d9\u0441\u0435",
          "\u0441\u043e\u043b\u0430\u0439",
          "\u0416\u0438\u0435\u043d\u0441\u0443",
          "90\u0442\u043e\u049b\u0441\u0430\u043d",
          "\u0436\u0430\u043b\u0493\u044b\u0437",
          "570\u0431\u0435\u0440\u0434\u0456\u043c",
        ],
        invalid: [" \u043a\u0435\u0448\u0456\u0440\u0456\u04a3\u0456\u0437 ", "abc\u0430\u0493\u044b\u043b\u0448\u044b\u043d\u0448\u0430", "\u043c\u04af\u043c\u043a\u0456\u043d!!"],
      });
    });

    it("should validate kurdish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["ku-IQ"],
        valid: ["\u0626\u0624\u06a4\u06af\u06ce\u0661\u0662\u0663"],
        invalid: ["\u00e4ca ", "abc\u00df", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate defined arabic aliases", () => {
      test({
        validator: "isAlphanumeric",
        args: ["ar-SY"],
        valid: ["\u0623\u0628\u062a123", "\u0623\u0628\u062a\u064e\u064f\u0650\u0661\u0662\u0663"],
        invalid: ["abc ", "foo!!", "\u00c4BC", "F\u00dc\u00fcbar", "J\u00f6n"],
      });
    });

    it("should validate norwegian alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["nb-NO"],
        valid: ["\u00c6\u00d8\u00c5123", "\u00c6re321", "321\u00d8re", "123\u00c5re"],
        invalid: ["\u00e4bc123", "\u00c4BC11", ""],
      });
    });

    it("should validate polish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["pl-PL"],
        valid: [
          "kre123sk\u0105",
          "zam21kni\u0119te",
          "zw23yk\u0142e",
          "123",
          "prz23yj\u0119\u0142y",
          "\u015bwi23\u0119ty",
          "Poz1322w\u00f3l",
        ],
        invalid: ["12\u0159i\u010f ", "bl\u00e9!!", "f\u00f6\u00f6!2!"],
      });
    });

    it("should validate serbian cyrillic alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["sr-RS"],
        valid: ["\u0428\u045b\u0436\u0402\u0459\u0415123", "\u0427\u041f\u0421\u0422132\u040b\u040f"],
        invalid: ["\u0159i\u010f ", "bl\u00e9!!", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate serbian latin alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["sr-RS@latin"],
        valid: ["\u0160Aab\u010d\u0161\u0111\u0107\u017e123", "\u0160ATRO11\u0106\u010d\u0111\u0161"],
        invalid: ["\u0159i\u010f ", "bl\u00e9!!", "f\u00f6\u00f6!!"],
      });
    });

    it("should validate swedish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["sv-SE"],
        valid: ["religi\u00f6s13", "st23j\u00e4la", "v\u00e4stg\u00f6te123", "123\u00c5re"],
        invalid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ", "foo!!", ""],
      });
    });

    it("should validate turkish alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["tr-TR"],
        valid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ123"],
        invalid: ["A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ ", "foo!!", "\u00c4BC"],
      });
    });

    it("should validate urkrainian alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["uk-UA"],
        valid: ["\u0410\u0411\u0412\u0413\u0490\u0414\u0415\u0404\u0416\u0417\u0418I\u0407\u0419\u041a\u041b\u041c\u041d\u041e\u041f\u0420\u0421\u0422\u0423\u0424\u0425\u0426\u0428\u0429\u042c\u042e\u042f123"],
        invalid: ["\u00e9eoc ", "foo!!", "\u00c4BC", "\u042b\u044b\u042a\u044a\u042d\u044d"],
      });
    });

    it("should validate greek alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["el-GR"],
        valid: [
          "\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c2\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9",
          "\u0391\u0392\u0393\u0394\u0395\u0396\u0397\u0398\u0399\u039a\u039b\u039c\u039d\u039e\u039f\u03a0\u03a1\u03a3\u03a4\u03a5\u03a6\u03a7\u03a8\u03a9",
          "20\u03b8",
          "1234568960",
        ],
        invalid: [
          "0A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ1",
          "  A\u0130\u0131\u00d6\u00f6\u00c7\u00e7\u015e\u015f\u011e\u011f\u00dc\u00fcZ  ",
          "\u00c4BC",
          "Hei\u00df",
          "\u042b\u044b\u042a\u044a\u042d\u044d",
          "j\u03b1ck\u03b3",
        ],
      });
    });

    it("should validate Hebrew alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["he"],
        valid: ["\u05d0\u05d1\u05d2123", "\u05e9\u05dc\u05d5\u05dd11"],
        invalid: ["\u05d0\u05d1\u05d2 ", "\u05dc\u05d0!!", "abc", "  foo  "],
      });
    });

    it("should validate Thai alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["th-TH"],
        valid: ["\u0e2a\u0e27\u0e31\u0e2a\u0e14\u0e35 \u0e51\u0e52\u0e53", "\u0e22\u0e34\u0e19\u0e14\u0e35\u0e15\u0e49\u0e2d\u0e19\u0e23\u0e31\u0e1a\u0e17\u0e31\u0e49\u0e07 \u0e52 \u0e04\u0e19"],
        invalid: ["1.\u0e2a\u0e27\u0e31\u0e2a\u0e14\u0e35", "\u0e22\u0e34\u0e19\u0e14\u0e35\u0e15\u0e49\u0e2d\u0e19\u0e23\u0e31\u0e1a\u0e17\u0e31\u0e49\u0e07 2 \u0e04\u0e19"],
      });
    });

    it("should validate Korea alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["ko-KR"],
        valid: ["2002", "\ud6c8\ubbfc\uc815\uc74c", "1446\ub144\ud6c8\ubbfc\uc815\uc74c\ubc18\ud3ec"],
        invalid: ["2022!", "2019 \ucf54\ub85c\ub098\uc2dc\uc791", "1.\ub85c\ub818\uc785\uc228"],
      });
    });

    it("should validate Sinhala alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["si-LK"],
        valid: ["\u0da0\u0dad\u0dd4\u0dbb", "\u0d9a\u0da0\u0da712", "\u0d8e\u0d8f\u0daf\u0dcf\u0ddb\u0db4\u0dc3\u0dd4\u0d9c\u0ddc2", "1234"],
        invalid: ["\u0b86\u0b90\u0905\u0924\u0d9a", "\u0d9a\u0da0\u0da7 12", " \u0d8e ", "a1234", "abc", ""],
      });
    });

    it("should validate Esperanto alphanumeric strings", () => {
      test({
        validator: "isAlphanumeric",
        args: ["eo"],
        valid: [
          "saluton",
          "e\u0125o\u015dan\u011do\u0109iu\u0135a\u016dde0123456789",
          "E\u0124O\u015cAN\u011cO\u0108IU\u0134A\u016cDE0123456789",
          "Esperanto1887",
          "La\u016dLudovikoZamenhofBongustasFre\u015da\u0108e\u0125aMan\u011da\u0135oKunSpicoj",
        ],
        invalid: ["qwxyz", "qwxyz 1887"],
      });
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isAlphanumeric",
        args: ["is-NOT"],
        error: ["1234568960", "abc123"],
      });
    });

    it("should validate numeric strings", () => {
      test({
        validator: "isNumeric",
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "123.123",
          "+000000",
        ],
        invalid: [" ", "", "."],
      });
    });

    it("should validate numeric strings without symbols", () => {
      test({
        validator: "isNumeric",
        args: [
          {
            no_symbols: true,
          },
        ],
        valid: ["123", "00123", "0"],
        invalid: ["-0", "+000000", "", "+123", "123.123", "-00123", " ", "."],
      });
    });

    it("should validate numeric strings with locale", () => {
      test({
        validator: "isNumeric",
        args: [
          {
            locale: "fr-FR",
          },
        ],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "123,123",
          "+000000",
        ],
        invalid: [" ", "", ","],
      });
    });

    it("should validate numeric strings with locale", () => {
      test({
        validator: "isNumeric",
        args: [
          {
            locale: "fr-CA",
          },
        ],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "123,123",
          "+000000",
        ],
        invalid: [" ", "", "."],
      });
    });

    it("should validate ports", () => {
      test({
        validator: "isPort",
        valid: ["0", "22", "80", "443", "3000", "8080", "65535"],
        invalid: ["", "-1", "65536", "0080"],
      });
    });

    it("should validate passport number", () => {
      test({
        validator: "isPassportNumber",
        args: ["AM"],
        valid: ["AF0549358"],
        invalid: ["A1054935"],
      });

      test({
        validator: "isPassportNumber",
        args: ["ID"],
        valid: ["C1253473", "B5948378", "A4859472"],
        invalid: ["D39481728", "A-3847362", "324132132"],
      });

      test({
        validator: "isPassportNumber",
        args: ["AR"],
        valid: ["AAC811035"],
        invalid: ["A11811035"],
      });

      test({
        validator: "isPassportNumber",
        args: ["AT"],
        valid: ["P 1630837", "P 4366918"],
        invalid: ["0 1630837"],
      });

      test({
        validator: "isPassportNumber",
        args: ["AU"],
        valid: ["N0995852", "L4819236"],
        invalid: ["1A012345"],
      });

      test({
        validator: "isPassportNumber",
        args: ["AZ"],
        valid: ["A16175905", "A16175958"],
        invalid: ["AZ1234584"],
      });

      test({
        validator: "isPassportNumber",
        args: ["BE"],
        valid: ["EM000000", "LA080402"],
        invalid: ["00123456"],
      });

      test({
        validator: "isPassportNumber",
        args: ["BG"],
        valid: ["346395366", "039903356"],
        invalid: ["ABC123456"],
      });

      test({
        validator: "isPassportNumber",
        args: ["BR"],
        valid: ["FZ973689", "GH231233"],
        invalid: ["ABX29332"],
      });

      test({
        validator: "isPassportNumber",
        args: ["BY"],
        valid: ["MP3899901"],
        invalid: ["345333454", "FG53334542"],
      });

      test({
        validator: "isPassportNumber",
        args: ["CA"],
        valid: ["GA302922", "ZE000509", "A123456AB", "Z556378HG"],
        invalid: [
          "AB0123456",
          "AZ556378H",
          "556378HCX",
          "556378432",
          "5563784",
          "#B12345FD",
          "A43F12354",
        ],
      });

      test({
        validator: "isPassportNumber",
        args: ["CH"],
        valid: ["S1100409", "S5200073", "X4028791"],
        invalid: ["AB123456"],
      });

      test({
        validator: "isPassportNumber",
        args: ["CN"],
        valid: ["G25352389", "E00160027", "EA1234567"],
        invalid: [
          "K0123456",
          "E-1234567",
          "G.1234567",
          "GA1234567",
          "EI1234567",
          "GO1234567",
        ],
      });

      test({
        validator: "isPassportNumber",
        args: ["CY"],
        valid: ["K00000413"],
        invalid: ["K10100"],
      });

      test({
        validator: "isPassportNumber",
        args: ["CZ"],
        valid: ["99003853", "42747260"],
        invalid: ["012345678", "AB123456"],
      });

      test({
        validator: "isPassportNumber",
        args: ["DE"],
        valid: ["C01X00T47", "C26VMVVC3"],
        invalid: ["AS0123456", "A012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["DK"],
        valid: ["900010172"],
        invalid: ["01234567", "K01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["DZ"],
        valid: ["855609385", "154472412", "197025599"],
        invalid: [
          "AS0123456",
          "A012345678",
          "0123456789",
          "12345678",
          "98KK54321",
        ],
      });

      test({
        validator: "isPassportNumber",
        args: ["EE"],
        valid: ["K4218285", "K3295867", "KB0167630", "VD0023777"],
        invalid: ["K01234567", "KB00112233"],
      });

      test({
        validator: "isPassportNumber",
        args: ["ES"],
        valid: ["AF238143", "ZAB000254"],
        invalid: ["AF01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["FI"],
        valid: ["XP8271602", "XD8500003"],
        invalid: ["A01234567", "ABC012345"],
      });

      test({
        validator: "isPassportNumber",
        args: ["FR"],
        valid: ["10CV28144", "60RF19342", "05RP34083"],
        invalid: ["012345678", "AB0123456", "01C234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["GB"],
        valid: ["925076473", "107182890", "104121156"],
        invalid: ["A012345678", "K000000000", "0123456789"],
      });

      test({
        validator: "isPassportNumber",
        args: ["GR"],
        valid: ["AE0000005", "AK0219304"],
        invalid: ["A01234567", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["HR"],
        valid: ["007007007", "138463188"],
        invalid: ["A01234567", "00112233"],
      });

      test({
        validator: "isPassportNumber",
        args: ["HU"],
        valid: ["ZA084505", "BA0006902"],
        invalid: ["A01234567", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["IE"],
        valid: ["D23145890", "X65097105", "XN0019390"],
        invalid: ["XND012345", "0123456789"],
      });

      test({
        validator: "isPassportNumber",
        args: ["IN"],
        valid: ["A-1234567", "A1234567", "X0019390"],
        invalid: ["AB-1234567", "0123456789"],
      });

      test({
        validator: "isPassportNumber",
        args: ["IR"],
        valid: ["J97634522", "A01234567", "Z11977831"],
        invalid: ["A0123456", "A0123456Z", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["IS"],
        valid: ["A2040611", "A1197783"],
        invalid: ["K0000000", "01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["IT"],
        valid: ["YA8335453", "KK0000000"],
        invalid: ["01234567", "KAK001122"],
      });

      test({
        validator: "isPassportNumber",
        args: ["JM"],
        valid: ["A0123456"],
        invalid: ["s0123456", "a01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["JP"],
        valid: ["NH1106002", "TE3180251", "XS1234567"],
        invalid: ["X12345678", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["KR"],
        valid: ["M35772699", "M70689098"],
        invalid: ["X12345678", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["KZ"],
        valid: ["A0123456", "b0123456"],
        invalid: ["01234567", "bb0123456"],
      });

      test({
        validator: "isPassportNumber",
        args: ["LI"],
        valid: ["a01234", "f01234"],
        invalid: ["012345"],
      });

      test({
        validator: "isPassportNumber",
        args: ["LT"],
        valid: ["20200997", "LB311756"],
        invalid: ["LB01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["LU"],
        valid: ["JCU9J4T2", "JC4E7L2H"],
        invalid: ["JCU9J4T", "JC4E7L2H0"],
      });

      test({
        validator: "isPassportNumber",
        args: ["LV"],
        valid: ["LV9000339", "LV4017173"],
        invalid: ["LV01234567", "4017173LV"],
      });
      test({
        validator: "isPassportNumber",
        args: ["LY"],
        valid: ["P79JF34X", "RJ45H4V2"],
        invalid: ["P79JF34", "RJ45H4V2C", "RJ4-H4V2"],
      });

      test({
        validator: "isPassportNumber",
        args: ["MT"],
        valid: ["1026564"],
        invalid: ["01234567", "MT01234"],
      });
      test({
        validator: "isPassportNumber",
        args: ["MZ"],
        valid: ["AB0808212", "08AB12123"],
        invalid: ["1AB011241", "1AB01121", "ABAB01121"],
      });
      test({
        validator: "isPassportNumber",
        args: ["MY"],
        valid: ["A00000000", "H12345678", "K43143233"],
        invalid: ["A1234567", "C01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["MX"],
        valid: ["43986369222", "01234567890"],
        invalid: ["ABC34567890", "34567890"],
      });

      test({
        validator: "isPassportNumber",
        args: ["NL"],
        valid: ["XTR110131", "XR1001R58"],
        invalid: ["XTR11013R", "XR1001R58A"],
      });
      test({
        validator: "isPassportNumber",
        args: ["PK"],
        valid: ["QZ1791293", "XR1001458"],
        invalid: ["XTR11013R", "XR1001R58A"],
      });

      test({
        validator: "isPassportNumber",
        args: ["PH"],
        valid: ["X123456", "XY123456", "XY1234567", "X1234567Y"],
        invalid: ["XY12345", "X12345Z", "XY12345Z"],
      });

      test({
        validator: "isPassportNumber",
        args: ["NZ"],
        valid: [
          "Lf012345",
          "La012345",
          "Ld012345",
          "Lh012345",
          "ea012345",
          "ep012345",
          "n012345",
        ],
        invalid: ["Lp012345", "nd012345", "ed012345", "eh012345", "ef012345"],
      });

      test({
        validator: "isPassportNumber",
        args: ["PL"],
        valid: ["ZS 0000177", "AN 3000011"],
        invalid: ["A1 0000177", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["PT"],
        valid: ["I700044", "K453286"],
        invalid: ["0700044", "K4532861"],
      });

      test({
        validator: "isPassportNumber",
        args: ["RO"],
        valid: ["05485968", "040005646"],
        invalid: ["R05485968", "0511060461"],
      });

      test({
        validator: "isPassportNumber",
        args: ["RU"],
        valid: ["2 32 636829", "012 345321", "439863692"],
        invalid: [
          "A 2R YU46J0",
          "01A 3D5321",
          "SF233D53T",
          "12345678",
          "1234567890",
        ],
      });

      test({
        validator: "isPassportNumber",
        args: ["SE"],
        valid: ["59000001", "56702928"],
        invalid: ["SE012345", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["SL"],
        valid: ["PB0036440", "PB1390281"],
        invalid: ["SL0123456", "P01234567"],
      });

      test({
        validator: "isPassportNumber",
        args: ["SK"],
        valid: ["P0000000"],
        invalid: ["SK012345", "012345678"],
      });

      test({
        validator: "isPassportNumber",
        args: ["TH"],
        valid: ["A123456", "B1234567", "CD123456", "EF1234567"],
        invalid: ["123456", "1234567", "010485371AA"],
      });

      test({
        validator: "isPassportNumber",
        args: ["TR"],
        valid: ["U 06764100", "U 01048537"],
        invalid: ["06764100U", "010485371"],
      });

      test({
        validator: "isPassportNumber",
        args: ["UA"],
        valid: ["EH345655", "EK000001", "AP841503"],
        invalid: ["01234567", "012345EH", "A012345P"],
      });

      test({
        validator: "isPassportNumber",
        args: ["US"],
        valid: ["790369937", "340007237", "A90583942", "E00007734"],
        invalid: [
          "US0123456",
          "0123456US",
          "7903699371",
          "90583942",
          "E000077341",
        ],
      });

      test({
        validator: "isPassportNumber",
        args: ["ZA"],
        valid: ["T12345678", "A12345678", "M12345678", "D12345678"],
        invalid: ["123456789", "Z12345678"],
      });
    });

    it("should validate decimal numbers", () => {
      test({
        validator: "isDecimal",
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0.01",
          ".1",
          "1.0",
          "-.25",
          "-0",
          "0.0000000000001",
        ],
        invalid: [
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "0,0000000000001",
          "0\u066b01",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "0\u066b0000000000001",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: "en-AU" }],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0.01",
          ".1",
          "1.0",
          "-.25",
          "-0",
          "0.0000000000001",
        ],
        invalid: [
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "0,0000000000001",
          "0\u066b01",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "0\u066b0000000000001",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: ["bg-BG"] }],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "-0",
          "0,0000000000001",
        ],
        invalid: [
          "0.0000000000001",
          "0.01",
          ".1",
          "1.0",
          "-.25",
          "0\u066b01",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "0\u066b0000000000001",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: ["cs-CZ"] }],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "-0",
          "0,0000000000001",
        ],
        invalid: [
          "0.0000000000001",
          "0.01",
          ".1",
          "1.0",
          "-.25",
          "0\u066b01",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "0\u066b0000000000001",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: ["ar-JO"] }],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0\u066b01",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "-0",
          "0\u066b0000000000001",
        ],
        invalid: [
          "0,0000000000001",
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "0.0000000000001",
          "0.01",
          ".1",
          "1.0",
          "-.25",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: ["ar-EG"] }],
        valid: ["0.01"],
        invalid: ["0,01"],
      });

      test({
        validator: "isDecimal",
        args: [{ locale: ["en-ZM"] }],
        valid: ["0,01"],
        invalid: ["0.01"],
      });

      test({
        validator: "isDecimal",
        args: [{ force_decimal: true }],
        valid: ["0.01", ".1", "1.0", "-.25", "0.0000000000001"],
        invalid: [
          "-0",
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0,0000000000001",
          "0,01",
          ",1",
          "1,0",
          "-,25",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });

      test({
        validator: "isDecimal",
        args: [{ decimal_digits: "2,3" }],
        valid: [
          "123",
          "00123",
          "-00123",
          "0",
          "-0",
          "+123",
          "0.01",
          "1.043",
          ".15",
          "-.255",
          "-0",
        ],
        invalid: [
          "0.0000000000001",
          "0.0",
          ".1",
          "1.0",
          "-.2564",
          "0.0",
          "\u066b1",
          "1\u066b0",
          "-\u066b25",
          "0\u066b0000000000001",
          "....",
          " ",
          "",
          "-",
          "+",
          ".",
          "0.1a",
          "a",
          "\n",
        ],
      });
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isDecimal",
        args: [{ locale: ["is-NOT"] }],
        error: ["123", "0.01", "0,01"],
      });
    });

    it("should validate lowercase strings", () => {
      test({
        validator: "isLowercase",
        valid: ["abc", "abc123", "this is lowercase.", "tr\u7aeas \u7aefber"],
        invalid: ["fooBar", "123A"],
      });
    });

    it("should validate imei strings", () => {
      test({
        validator: "isIMEI",
        valid: [
          "352099001761481",
          "868932036356090",
          "490154203237518",
          "546918475942169",
          "998227667144730",
          "532729766805999",
        ],
        invalid: ["490154203237517", "3568680000414120", "3520990017614823"],
      });
    });

    it("should validate imei strings with hyphens", () => {
      test({
        validator: "isIMEI",
        args: [{ allow_hyphens: true }],
        valid: [
          "35-209900-176148-1",
          "86-893203-635609-0",
          "49-015420-323751-8",
          "54-691847-594216-9",
          "99-822766-714473-0",
          "53-272976-680599-9",
        ],
        invalid: [
          "49-015420-323751-7",
          "35-686800-0041412-0",
          "35-209900-1761482-3",
        ],
      });
    });

    it("should validate uppercase strings", () => {
      test({
        validator: "isUppercase",
        valid: ["ABC", "ABC123", "ALL CAPS IS FUN.", "   ."],
        invalid: ["fooBar", "123abc"],
      });
    });

    it("should validate integers", () => {
      test({
        validator: "isInt",
        valid: ["13", "123", "0", "123", "-0", "+1", "01", "-01", "000"],
        invalid: ["100e10", "123.123", "   ", ""],
      });
      test({
        validator: "isInt",
        args: [{ allow_leading_zeroes: false }],
        valid: ["13", "123", "0", "123", "-0", "+1"],
        invalid: ["01", "-01", "000", "100e10", "123.123", "   ", ""],
      });
      test({
        validator: "isInt",
        args: [{ allow_leading_zeroes: true }],
        valid: [
          "13",
          "123",
          "0",
          "123",
          "-0",
          "+1",
          "01",
          "-01",
          "000",
          "-000",
          "+000",
        ],
        invalid: ["100e10", "123.123", "   ", ""],
      });
      test({
        validator: "isInt",
        args: [
          {
            min: 10,
          },
        ],
        valid: ["15", "80", "99"],
        invalid: ["9", "6", "3.2", "a"],
      });
      test({
        validator: "isInt",
        args: [
          {
            min: 10,
            max: 15,
          },
        ],
        valid: ["15", "11", "13"],
        invalid: ["9", "2", "17", "3.2", "33", "a"],
      });
      test({
        validator: "isInt",
        args: [
          {
            gt: 10,
            lt: 15,
          },
        ],
        valid: ["14", "11", "13"],
        invalid: ["10", "15", "17", "3.2", "33", "a"],
      });
      test({
        validator: "isInt",
        args: [
          {
            min: undefined,
            max: undefined,
          },
        ],
        valid: ["143", "15", "767777575"],
        invalid: ["10.4", "bar", "10a", "c44"],
      });
      test({
        validator: "isInt",
        args: [
          {
            gt: undefined,
            lt: undefined,
          },
        ],
        valid: ["289373466", "55", "989"],
        invalid: ["10.4", "baz", "66a", "c21"],
      });
      test({
        validator: "isInt",
        args: [
          {
            gt: null,
            max: null,
          },
        ],
        valid: ["1", "886", "84512345"],
        invalid: ["10.4", "h", "1.2", "+"],
      });
      test({
        validator: "isInt",
        args: [
          {
            lt: null,
            min: null,
          },
        ],
        valid: ["289373466", "55", "989"],
        invalid: [",", "+11212+", "fail", "111987234i"],
      });
    });

    it("should validate floats", () => {
      test({
        validator: "isFloat",
        valid: [
          "123",
          "123.",
          "123.123",
          "-123.123",
          "-0.123",
          "+0.123",
          "0.123",
          ".0",
          "-.123",
          "+.123",
          "01.123",
          "-0.22250738585072011e-307",
        ],
        invalid: [
          "+",
          "-",
          "  ",
          "",
          ".",
          ",",
          "foo",
          "20.foo",
          "2020-01-06T14:31:00.135Z",
        ],
      });

      test({
        validator: "isFloat",
        args: [{ locale: "en-AU" }],
        valid: [
          "123",
          "123.",
          "123.123",
          "-123.123",
          "-0.123",
          "+0.123",
          "0.123",
          ".0",
          "-.123",
          "+.123",
          "01.123",
          "-0.22250738585072011e-307",
        ],
        invalid: ["123\u066b123", "123,123", "  ", "", ".", "foo"],
      });

      test({
        validator: "isFloat",
        args: [{ locale: "de-DE" }],
        valid: [
          "123",
          "123,",
          "123,123",
          "-123,123",
          "-0,123",
          "+0,123",
          "0,123",
          ",0",
          "-,123",
          "+,123",
          "01,123",
          "-0,22250738585072011e-307",
        ],
        invalid: ["123.123", "123\u066b123", "  ", "", ".", "foo"],
      });

      test({
        validator: "isFloat",
        args: [{ locale: "ar-JO" }],
        valid: [
          "123",
          "123\u066b",
          "123\u066b123",
          "-123\u066b123",
          "-0\u066b123",
          "+0\u066b123",
          "0\u066b123",
          "\u066b0",
          "-\u066b123",
          "+\u066b123",
          "01\u066b123",
          "-0\u066b22250738585072011e-307",
        ],
        invalid: ["123,123", "123.123", "  ", "", ".", "foo"],
      });

      test({
        validator: "isFloat",
        args: [
          {
            min: 3.7,
          },
        ],
        valid: ["3.888", "3.92", "4.5", "50", "3.7", "3.71"],
        invalid: ["3.6", "3.69", "3", "1.5", "a"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            min: 0.1,
            max: 1.0,
          },
        ],
        valid: ["0.1", "1.0", "0.15", "0.33", "0.57", "0.7"],
        invalid: ["0", "0.0", "a", "1.3", "0.05", "5"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            gt: -5.5,
            lt: 10,
          },
        ],
        valid: ["9.9", "1.0", "0", "-1", "7", "-5.4"],
        invalid: ["10", "-5.5", "a", "-20.3", "20e3", "10.00001"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            min: -5.5,
            max: 10,
            gt: -5.5,
            lt: 10,
          },
        ],
        valid: ["9.99999", "-5.499999"],
        invalid: ["10", "-5.5"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            locale: "de-DE",
            min: 3.1,
          },
        ],
        valid: ["123", "123,", "123,123", "3,1", "3,100001"],
        invalid: [
          "3,09",
          "-,123",
          "+,123",
          "01,123",
          "-0,22250738585072011e-307",
          "-123,123",
          "-0,123",
          "+0,123",
          "0,123",
          ",0",
          "123.123",
          "123\u066b123",
          "  ",
          "",
          ".",
          "foo",
        ],
      });
      test({
        validator: "isFloat",
        args: [
          {
            min: undefined,
            max: undefined,
          },
        ],
        valid: ["123", "123.", "123.123", "-767.767", "+111.111"],
        invalid: ["ab565", "-,123", "+,123", "7866.t", "123,123", "123,"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            gt: undefined,
            lt: undefined,
          },
        ],
        valid: ["14.34343", "11.1", "456"],
        invalid: ["ab565", "-,123", "+,123", "7866.t"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            locale: "ar",
            gt: null,
            max: null,
          },
        ],
        valid: ["13324\u066b", "12321", "444\u066b83874"],
        invalid: ["55.55.55", "1;23", "+-123", "1111111l1", "3.3"],
      });
      test({
        validator: "isFloat",
        args: [
          {
            locale: "ru-RU",
            lt: null,
            min: null,
          },
        ],
        valid: ["11231554,34343", "11,1", "456", ",311"],
        invalid: ["ab565", "-.123", "+.123", "7866.t", "22.3"],
      });
    });

    it("should validate hexadecimal strings", () => {
      test({
        validator: "isHexadecimal",
        valid: [
          "deadBEEF",
          "ff0044",
          "0xff0044",
          "0XfF0044",
          "0x0123456789abcDEF",
          "0X0123456789abcDEF",
          "0hfedCBA9876543210",
          "0HfedCBA9876543210",
          "0123456789abcDEF",
        ],
        invalid: [
          "abcdefg",
          "",
          "..",
          "0xa2h",
          "0xa20x",
          "0x0123456789abcDEFq",
          "0hfedCBA9876543210q",
          "01234q56789abcDEF",
        ],
      });
    });

    it("should validate octal strings", () => {
      test({
        validator: "isOctal",
        valid: ["076543210", "0o01234567"],
        invalid: [
          "abcdefg",
          "012345678",
          "012345670c",
          "00c12345670c",
          "",
          "..",
        ],
      });
    });

    it("should validate hexadecimal color strings", () => {
      test({
        validator: "isHexColor",
        valid: ["#ff0000ff", "#ff0034", "#CCCCCC", "0f38", "fff", "#f00"],
        invalid: ["#ff", "fff0a", "#ff12FG"],
      });
    });

    it("should validate HSL color strings", () => {
      test({
        validator: "isHSL",
        valid: [
          "hsl(360,0000000000100%,000000100%)",
          "hsl(000010, 00000000001%, 00000040%)",
          "HSL(00000,0000000000100%,000000100%)",
          "hsL(0, 0%, 0%)",
          "hSl(  360  , 100%  , 100%   )",
          "Hsl(  00150  , 000099%  , 01%   )",
          "hsl(01080, 03%, 4%)",
          "hsl(-540, 03%, 4%)",
          "hsla(+540, 03%, 4%)",
          "hsla(+540, 03%, 4%, 500)",
          "hsl(+540deg, 03%, 4%, 500)",
          "hsl(+540gRaD, 03%, 4%, 500)",
          "hsl(+540.01e-98rad, 03%, 4%, 500)",
          "hsl(-540.5turn, 03%, 4%, 500)",
          "hsl(+540, 03%, 4%, 500e-01)",
          "hsl(+540, 03%, 4%, 500e+80)",
          "hsl(4.71239rad, 60%, 70%)",
          "hsl(270deg, 60%, 70%)",
          "hsl(200, +.1%, 62%, 1)",
          "hsl(270 60% 70%)",
          "hsl(200, +.1e-9%, 62e10%, 1)",
          "hsl(.75turn, 60%, 70%)",
          // 'hsl(200grad+.1%62%/1)', //supposed to pass, but need to handle delimiters
          "hsl(200grad +.1% 62% / 1)",
          "hsl(270, 60%, 50%, .15)",
          "hsl(270, 60%, 50%, 15%)",
          "hsl(270 60% 50% / .15)",
          "hsl(270 60% 50% / 15%)",
        ],
        invalid: [
          "hsl (360,0000000000100%,000000100%)",
          "hsl(0260, 100 %, 100%)",
          "hsl(0160, 100%, 100%, 100 %)",
          "hsl(-0160, 100%, 100a)",
          "hsl(-0160, 100%, 100)",
          "hsl(-0160 100%, 100%, )",
          "hsl(270 deg, 60%, 70%)",
          "hsl( deg, 60%, 70%)",
          "hsl(, 60%, 70%)",
          "hsl(3000deg, 70%)",
        ],
      });
    });

    it("should validate rgb color strings", () => {
      test({
        validator: "isRgbColor",
        valid: [
          "rgb(0,0,0)",
          "rgb(255,255,255)",
          "rgba(0,0,0,0)",
          "rgba(255,255,255,1)",
          "rgba(255,255,255,.1)",
          "rgba(255,255,255,0.1)",
          "rgba(255,255,255,.12)",
          "rgb(5%,5%,5%)",
          "rgba(5%,5%,5%,.3)",
          "rgba(5%,5%,5%,.32)",
        ],
        invalid: [
          "rgb(0,0,0,)",
          "rgb(0,0,)",
          "rgb(0,0,256)",
          "rgb()",
          "rgba(0,0,0)",
          "rgba(255,255,255,2)",
          "rgba(255,255,255,.123)",
          "rgba(255,255,256,0.1)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "rgba(3,3,3%,.3)",
          "rgba(5%,5%,5%,.321)",
          "rgb(101%,101%,101%)",
          "rgba(3%,3%,101%,0.3)",
          "rgb(101%,101%,101%) additional invalid string part",
          "rgba(3%,3%,101%,0.3) additional invalid string part",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
          "rg ba(0, 251, 22, 0.5)",
          "rgb( 255,255 ,255)",
          "rgba(255, 255, 255, 0.5)",
          "rgba(255, 255, 255, 0.5)",
          "rgb(5%, 5%, 5%)",
        ],
      });

      // test empty options object
      test({
        validator: "isRgbColor",
        args: [{}],
        valid: [
          "rgb(0,0,0)",
          "rgb(255,255,255)",
          "rgba(0,0,0,0)",
          "rgba(255,255,255,1)",
          "rgba(255,255,255,.1)",
          "rgba(255,255,255,.12)",
          "rgba(255,255,255,0.1)",
          "rgb(5%,5%,5%)",
          "rgba(5%,5%,5%,.3)",
        ],
        invalid: [
          "rgb(0,0,0,)",
          "rgb(0,0,)",
          "rgb(0,0,256)",
          "rgb()",
          "rgba(0,0,0)",
          "rgba(255,255,255,2)",
          "rgba(255,255,256,0.1)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "rgba(3,3,3%,.3)",
          "rgb(101%,101%,101%)",
          "rgba(3%,3%,101%,0.3)",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
          "rg ba(0, 251, 22, 0.5)",
          "rgb( 255,255 ,255)",
          "rgba(255, 255, 255, 0.5)",
          "rgba(255, 255, 255, 0.5)",
          "rgb(5%, 5%, 5%)",
        ],
      });
      // test where includePercentValues is given as false
      test({
        validator: "isRgbColor",
        args: [false],
        valid: ["rgb(5,5,5)", "rgba(5,5,5,.3)"],
        invalid: [
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
        ],
      });

      // test where includePercentValues is given as false as part of options object
      test({
        validator: "isRgbColor",
        args: [{ includePercentValues: false }],
        valid: ["rgb(5,5,5)", "rgba(5,5,5,.3)"],
        invalid: [
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "r         g    b(   0,         251,       222     )",
          "rgba(255, 255, 255 ,0.2)",
          "r         g    ba(   0,         251,       222     )",
        ],
      });

      // test where include percent is true explciitly
      test({
        validator: "isRgbColor",
        args: [true],
        valid: [
          "rgb(5,5,5)",
          "rgba(5,5,5,.3)",
          "rgb(0,0,0)",
          "rgb(255,255,255)",
          "rgba(0,0,0,0)",
          "rgba(255,255,255,1)",
          "rgba(255,255,255,.1)",
          "rgba(255,255,255,.12)",
          "rgba(255,255,255,0.1)",
          "rgb(5%,5%,5%)",
          "rgba(5%,5%,5%,.3)",
          "rgb(5%,5%,5%)",
          "rgba(255,255,255,0.5)",
        ],
        invalid: [
          "rgba(255, 255, 255, 0.5)",
          "rgb(5%, 5%, 5%)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
          "rgb(0,0,0,)",
          "rgb(0,0,)",
          "rgb(0,0,256)",
          "rgb()",
          "rgba(0,0,0)",
          "rgba(255,255,255,2)",
          "rgba(255,255,256,0.1)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "rgba(3,3,3%,.3)",
          "rgb(101%,101%,101%)",
          "rgba(3%,3%,101%,0.3)",
        ],
      });

      // test where percent value is false and allowSpaces is true as part of options object
      test({
        validator: "isRgbColor",
        args: [{ includePercentValues: false, allowSpaces: true }],
        valid: [
          "rgb(5,5,5)",
          "rgba(5,5,5,.3)",
          "rgba(255,255,255,0.2)",
          "rgba(255, 255, 255 ,0.2)",
        ],
        invalid: [
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "rgba(5% ,5%, 5%)",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
          "rgb(0,0,)",
          "rgb()",
          "rgb(4,4,5%)",
          "rgb(5%,5%,5%)",
          "rgba(3,3,3%,.3)",
          "rgb(101%, 101%, 101%)",
          "rgba(3%,3%,101%,0.3)",
        ],
      });

      // test where both are true as part of options object
      test({
        validator: "isRgbColor",
        args: [{ includePercentValues: true, allowSpaces: true }],
        valid: [
          "rgb(  5, 5, 5)",
          "rgba(5, 5, 5, .3)",
          "rgb(0, 0, 0)",
          "rgb(255, 255, 255)",
          "rgba(0, 0, 0, 0)",
          "rgba(255, 255, 255, 1)",
          "rgba(255, 255, 255, .1)",
          "rgba(255, 255, 255, 0.1)",
          "rgb(5% ,5% ,5%)",
          "rgba(5%,5%,5%, .3)",
        ],
        invalid: [
          "r         g    b(   0,         251,       222     )",
          "rgb(4,4,5%)",
          "rgb(101%,101%,101%)",
        ],
      });

      // test where allowSpaces is false as part of options object
      test({
        validator: "isRgbColor",
        args: [{ includePercentValues: true, allowSpaces: false }],
        valid: [
          "rgb(5,5,5)",
          "rgba(5,5,5,.3)",
          "rgb(0,0,0)",
          "rgb(255,255,255)",
          "rgba(0,0,0,0)",
          "rgba(255,255,255,1)",
          "rgba(255,255,255,.1)",
          "rgba(255,255,255,.12)",
          "rgba(255,255,255,0.1)",
          "rgb(5%,5%,5%)",
          "rgba(5%,5%,5%,.3)",
        ],
        invalid: [
          "rgb( 255,255 ,255)",
          "rgba(255, 255, 255, 0.5)",
          "rgb(5%, 5%, 5%)",
          "rgba(255, 255, 255, 0.5)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "r         g    b(   0,         251,       222     )",
          "r         g    ba(   0,         251,       222     )",
          "rgb(0,0,0,)",
          "rgb(0,0,)",
          "rgb(0,0,256)",
          "rgb()",
          "rgba(0,0,0)",
          "rgba(255,255,255,2)",
          "rgba(255,255,256,0.1)",
          "rgb(4,4,5%)",
          "rgba(5%,5%,5%)",
          "rgba(3,3,3%,.3)",
          "rgb(101%,101%,101%)",
          "rgba(3%,3%,101%,0.3)",
        ],
      });
    });

    it("should validate ISRC code strings", () => {
      test({
        validator: "isISRC",
        valid: ["USAT29900609", "GBAYE6800011", "USRC15705223", "USCA29500702"],
        invalid: [
          "USAT2990060",
          "SRC15705223",
          "US-CA29500702",
          "USARC15705223",
        ],
      });
    });

    it("should validate md5 strings", () => {
      test({
        validator: "isMD5",
        valid: [
          "d94f3f016ae679c3008de268209132f2",
          "751adbc511ccbe8edf23d486fa4581cd",
          "88dae00e614d8f24cfd5a8b3f8002e93",
          "0bf1c35032a71a14c2f719e5a14c1e96",
        ],
        invalid: [
          "KYT0bf1c35032a71a14c2f719e5a14c1",
          "q94375dj93458w34",
          "39485729348",
          "%&FHKJFvk",
        ],
      });
    });

    it("should validate hash strings", () => {
      ["md5", "md4", "ripemd128", "tiger128"].forEach((algorithm) => {
        test({
          validator: "isHash",
          args: [algorithm],
          valid: [
            "d94f3f016ae679c3008de268209132f2",
            "751adbc511ccbe8edf23d486fa4581cd",
            "88dae00e614d8f24cfd5a8b3f8002e93",
            "0bf1c35032a71a14c2f719e5a14c1e96",
            "d94f3F016Ae679C3008de268209132F2",
            "88DAE00e614d8f24cfd5a8b3f8002E93",
          ],
          invalid: [
            "q94375dj93458w34",
            "39485729348",
            "%&FHKJFvk",
            "KYT0bf1c35032a71a14c2f719e5a1",
          ],
        });
      });

      ["crc32", "crc32b"].forEach((algorithm) => {
        test({
          validator: "isHash",
          args: [algorithm],
          valid: [
            "d94f3f01",
            "751adbc5",
            "88dae00e",
            "0bf1c350",
            "88DAE00e",
            "751aDBc5",
          ],
          invalid: [
            "KYT0bf1c35032a71a14c2f719e5a14c1",
            "q94375dj93458w34",
            "q943",
            "39485729348",
            "%&FHKJFvk",
          ],
        });
      });

      ["sha1", "tiger160", "ripemd160"].forEach((algorithm) => {
        test({
          validator: "isHash",
          args: [algorithm],
          valid: [
            "3ca25ae354e192b26879f651a51d92aa8a34d8d3",
            "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d",
            "beb8c3f30da46be179b8df5f5ecb5e4b10508230",
            "efd5d3b190e893ed317f38da2420d63b7ae0d5ed",
            "AAF4c61ddCC5e8a2dabede0f3b482cd9AEA9434D",
            "3ca25AE354e192b26879f651A51d92aa8a34d8D3",
          ],
          invalid: [
            "KYT0bf1c35032a71a14c2f719e5a14c1",
            "KYT0bf1c35032a71a14c2f719e5a14c1dsjkjkjkjkkjk",
            "q94375dj93458w34",
            "39485729348",
            "%&FHKJFvk",
          ],
        });
      });

      test({
        validator: "isHash",
        args: ["sha256"],
        valid: [
          "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
          "1d996e033d612d9af2b44b70061ee0e868bfd14c2dd90b129e1edeb7953e7985",
          "80f70bfeaed5886e33536bcfa8c05c60afef5a0e48f699a7912d5e399cdcc441",
          "579282cfb65ca1f109b78536effaf621b853c9f7079664a3fbe2b519f435898c",
          "2CF24dba5FB0a30e26E83b2AC5b9E29E1b161e5C1fa7425E73043362938b9824",
          "80F70bFEAed5886e33536bcfa8c05c60aFEF5a0e48f699a7912d5e399cdCC441",
        ],
        invalid: [
          "KYT0bf1c35032a71a14c2f719e5a14c1",
          "KYT0bf1c35032a71a14c2f719e5a14c1dsjkjkjkjkkjk",
          "q94375dj93458w34",
          "39485729348",
          "%&FHKJFvk",
        ],
      });
      test({
        validator: "isHash",
        args: ["sha384"],
        valid: [
          "3fed1f814d28dc5d63e313f8a601ecc4836d1662a19365cbdcf6870f6b56388850b58043f7ebf2418abb8f39c3a42e31",
          "b330f4e575db6e73500bd3b805db1a84b5a034e5d21f0041d91eec85af1dfcb13e40bb1c4d36a72487e048ac6af74b58",
          "bf547c3fc5841a377eb1519c2890344dbab15c40ae4150b4b34443d2212e5b04aa9d58865bf03d8ae27840fef430b891",
          "fc09a3d11368386530f985dacddd026ae1e44e0e297c805c3429d50744e6237eb4417c20ffca8807b071823af13a3f65",
          "3fed1f814d28dc5d63e313f8A601ecc4836d1662a19365CBDCf6870f6b56388850b58043f7ebf2418abb8f39c3a42e31",
          "b330f4E575db6e73500bd3b805db1a84b5a034e5d21f0041d91EEC85af1dfcb13e40bb1c4d36a72487e048ac6af74b58",
        ],
        invalid: [
          "KYT0bf1c35032a71a14c2f719e5a14c1",
          "KYT0bf1c35032a71a14c2f719e5a14c1dsjkjkjkjkkjk",
          "q94375dj93458w34",
          "39485729348",
          "%&FHKJFvk",
        ],
      });
      test({
        validator: "isHash",
        args: ["sha512"],
        valid: [
          "9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043",
          "83c586381bf5ba94c8d9ba8b6b92beb0997d76c257708742a6c26d1b7cbb9269af92d527419d5b8475f2bb6686d2f92a6649b7f174c1d8306eb335e585ab5049",
          "45bc5fa8cb45ee408c04b6269e9f1e1c17090c5ce26ffeeda2af097735b29953ce547e40ff3ad0d120e5361cc5f9cee35ea91ecd4077f3f589b4d439168f91b9",
          "432ac3d29e4f18c7f604f7c3c96369a6c5c61fc09bf77880548239baffd61636d42ed374f41c261e424d20d98e320e812a6d52865be059745fdb2cb20acff0ab",
          "9B71D224bd62f3785D96d46ad3ea3d73319bFBC2890CAAdae2dff72519673CA72323C3d99ba5c11d7c7ACC6e14b8c5DA0c4663475c2E5c3adef46f73bcDEC043",
          "432AC3d29E4f18c7F604f7c3c96369A6C5c61fC09Bf77880548239baffd61636d42ed374f41c261e424d20d98e320e812a6d52865be059745fdb2cb20acff0ab",
        ],
        invalid: [
          "KYT0bf1c35032a71a14c2f719e5a14c1",
          "KYT0bf1c35032a71a14c2f719e5a14c1dsjkjkjkjkkjk",
          "q94375dj93458w34",
          "39485729348",
          "%&FHKJFvk",
        ],
      });
      test({
        validator: "isHash",
        args: ["tiger192"],
        valid: [
          "6281a1f098c5e7290927ed09150d43ff3990a0fe1a48267c",
          "56268f7bc269cf1bc83d3ce42e07a85632394737918f4760",
          "46fc0125a148788a3ac1d649566fc04eb84a746f1a6e4fa7",
          "7731ea1621ae99ea3197b94583d034fdbaa4dce31a67404a",
          "6281A1f098c5e7290927ed09150d43ff3990a0fe1a48267C",
          "46FC0125a148788a3AC1d649566fc04eb84A746f1a6E4fa7",
        ],
        invalid: [
          "KYT0bf1c35032a71a14c2f719e5a14c1",
          "KYT0bf1c35032a71a14c2f719e5a14c1dsjkjkjkjkkjk",
          "q94375dj93458w34",
          "39485729348",
          "%&FHKJFvk",
        ],
      });
    });
    it("should validate JWT tokens", () => {
      test({
        validator: "isJWT",
        valid: [
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJsb2dnZWRJbkFzIjoiYWRtaW4iLCJpYXQiOjE0MjI3Nzk2Mzh9.gzSraSYS8EXBxLN_oWnFSRgCzcmJmMjLiuyu5CSpyHI",
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJsb3JlbSI6Imlwc3VtIn0.ymiJSsMJXR6tMSr8G9usjQ15_8hKPDv_CArLhxw28MI",
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkb2xvciI6InNpdCIsImFtZXQiOlsibG9yZW0iLCJpcHN1bSJdfQ.rRpe04zbWbbJjwM43VnHzAboDzszJtGrNsUxaqQ-GQ8",
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqb2huIjp7ImFnZSI6MjUsImhlaWdodCI6MTg1fSwiamFrZSI6eyJhZ2UiOjMwLCJoZWlnaHQiOjI3MH19.YRLPARDmhGMC3BBk_OhtwwK21PIkVCqQe8ncIRPKo-E",
        ],
        invalid: [
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9",
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NSIsIm5hbWUiOiJKb2huIERvZSIsImlhdCI6MTUxNjIzOTAyMn0",
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NSIsIm5hbWUiOiJKb2huIERvZSIsImlhdCI6MTYxNjY1Mzg3Mn0.eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwiaWF0IjoxNjE2NjUzODcyLCJleHAiOjE2MTY2NTM4ODJ9.a1jLRQkO5TV5y5ERcaPAiM9Xm2gBdRjKrrCpHkGr_8M",
          "$Zs.ewu.su84",
          "ks64$S/9.dy$\u00a7kz.3sd73b",
        ],
        error: [[], {}, null, undefined],
      });
    });

    it("should validate null strings", () => {
      test({
        validator: "isEmpty",
        valid: [""],
        invalid: [" ", "foo", "3"],
      });
      test({
        validator: "isEmpty",
        args: [{ ignore_whitespace: false }],
        valid: [""],
        invalid: [" ", "foo", "3"],
      });
      test({
        validator: "isEmpty",
        args: [{ ignore_whitespace: true }],
        valid: ["", " "],
        invalid: ["foo", "3"],
      });
    });

    it("should validate strings against an expected value", () => {
      test({
        validator: "equals",
        args: ["abc"],
        valid: ["abc"],
        invalid: ["Abc", "123"],
      });
    });

    it("should validate strings contain another string", () => {
      test({
        validator: "contains",
        args: ["foo"],
        valid: ["foo", "foobar", "bazfoo"],
        invalid: ["bar", "fobar"],
      });

      test({
        validator: "contains",
        args: [
          "foo",
          {
            ignoreCase: true,
          },
        ],
        valid: ["Foo", "FOObar", "BAZfoo"],
        invalid: ["bar", "fobar", "baxoof"],
      });

      test({
        validator: "contains",
        args: [
          "foo",
          {
            minOccurrences: 2,
          },
        ],
        valid: ["foofoofoo", "12foo124foo", "fofooofoooofoooo", "foo1foo"],
        invalid: ["foo", "foobar", "Fooofoo", "foofo"],
      });
    });

    it("should validate strings against a pattern", () => {
      test({
        validator: "matches",
        args: [/abc/],
        valid: ["abc", "abcdef", "123abc"],
        invalid: ["acb", "Abc"],
      });
      test({
        validator: "matches",
        args: ["abc"],
        valid: ["abc", "abcdef", "123abc"],
        invalid: ["acb", "Abc"],
      });
      test({
        validator: "matches",
        args: ["abc", "i"],
        valid: ["abc", "abcdef", "123abc", "AbC"],
        invalid: ["acb"],
      });
    });

    it("should validate strings by length (deprecated api)", () => {
      test({
        validator: "isLength",
        args: [2],
        valid: ["abc", "de", "abcd"],
        invalid: ["", "a"],
      });
      test({
        validator: "isLength",
        args: [2, 3],
        valid: ["abc", "de"],
        invalid: ["", "a", "abcd"],
      });
      test({
        validator: "isLength",
        args: [2, 3],
        valid: ["\u5e72\ud867\ude3d", "\ud842\udfb7\u91ce\u5bb6"],
        invalid: ["", "\ud840\udc0b", "\u5343\u7ac8\u901a\u308a"],
      });
      test({
        validator: "isLength",
        args: [0, 0],
        valid: [""],
        invalid: ["a", "ab"],
      });
    });

    it("should validate isLocale codes", () => {
      test({
        validator: "isLocale",
        valid: [
          "uz_Latn_UZ",
          "en",
          "gsw",
          "en-US",
          "es_ES",
          "es-419",
          "sw_KE",
          "am_ET",
          "zh-CHS",
          "ca_ES_VALENCIA",
          "en_US_POSIX",
          "hak-CN",
          "zh-Hant",
          "zh-Hans",
          "sr-Cyrl",
          "sr-Latn",
          "zh-cmn-Hans-CN",
          "cmn-Hans-CN",
          "zh-yue-HK",
          "yue-HK",
          "zh-Hans-CN",
          "sr-Latn-RS",
          "sl-rozaj",
          "sl-rozaj-biske",
          "sl-nedis",
          "de-CH-1901",
          "sl-IT-nedis",
          "hy-Latn-IT-arevela",
          "i-enochian",
          "en-scotland-fonipa",
          "sl-IT-rozaj-biske-1994",
          "de-CH-x-phonebk",
          "az-Arab-x-AZE-derbend",
          "x-whatever",
          "qaa-Qaaa-QM-x-southern",
          "de-Qaaa",
          "sr-Latn-QM",
          "sr-Qaaa-RS",
          "en-US-u-islamcal",
          "zh-CN-a-myext-x-private",
          "en-a-myext-b-another",
        ],
        invalid: ["lo_POP", "12", "12_DD", "de-419-DE", "a-DE"],
      });
    });

    it("should validate strings by byte length (deprecated api)", () => {
      test({
        validator: "isByteLength",
        args: [2],
        valid: ["abc", "de", "abcd", "\uff47\uff4d\uff41\uff49\uff4c"],
        invalid: ["", "a"],
      });
      test({
        validator: "isByteLength",
        args: [2, 3],
        valid: ["abc", "de", "\uff47"],
        invalid: ["", "a", "abcd", "\uff47\uff4d"],
      });
      test({
        validator: "isByteLength",
        args: [0, 0],
        valid: [""],
        invalid: ["\uff47", "a"],
      });
    });

    it("should validate strings by length", () => {
      test({
        validator: "isLength",
        args: [{ min: 2 }],
        valid: ["abc", "de", "abcd"],
        invalid: ["", "a"],
      });
      test({
        validator: "isLength",
        args: [{ min: 2, max: 3 }],
        valid: ["abc", "de"],
        invalid: ["", "a", "abcd"],
      });
      test({
        validator: "isLength",
        args: [{ min: 2, max: 3 }],
        valid: ["\u5e72\ud867\ude3d", "\ud842\udfb7\u91ce\u5bb6"],
        invalid: ["", "\ud840\udc0b", "\u5343\u7ac8\u901a\u308a"],
      });
      test({
        validator: "isLength",
        args: [{ max: 3 }],
        valid: ["abc", "de", "a", ""],
        invalid: ["abcd"],
      });
      test({
        validator: "isLength",
        args: [{ max: 6, discreteLengths: 5 }],
        valid: ["abcd", "vfd", "ff", "", "k"],
        invalid: ["abcdefgh", "hfjdksks"],
      });
      test({
        validator: "isLength",
        args: [{ min: 2, max: 6, discreteLengths: 5 }],
        valid: ["bsa", "vfvd", "ff"],
        invalid: ["", " ", "hfskdunvc"],
      });
      test({
        validator: "isLength",
        args: [{ min: 1, discreteLengths: 2 }],
        valid: [" ", "hello", "bsa"],
        invalid: [""],
      });
      test({
        validator: "isLength",
        args: [{ max: 0 }],
        valid: [""],
        invalid: ["a", "ab"],
      });
      test({
        validator: "isLength",
        args: [{ min: 5, max: 10, discreteLengths: [2, 6, 8, 9] }],
        valid: ["helloguy", "shopping", "validator", "length"],
        invalid: ["abcde", "abcdefg"],
      });
      test({
        validator: "isLength",
        args: [{ discreteLengths: "9" }],
        valid: ["a", "abcd", "abcdefghijkl"],
        invalid: [],
      });
      test({
        validator: "isLength",
        valid: ["a", "", "asds"],
      });
      test({
        validator: "isLength",
        args: [{ max: 8 }],
        valid: ["\ud83d\udc69\ud83e\uddb0\ud83d\udc69\ud83d\udc69\ud83d\udc66\ud83d\udc66\ud83c\udff3\ufe0f\ud83c\udf08", "\u23e9\ufe0e\u23e9\ufe0e\u23ea\ufe0e\u23ea\ufe0e\u23ed\ufe0e\u23ed\ufe0e\u23ee\ufe0e\u23ee\ufe0e"],
      });
    });

    it("should validate strings by byte length", () => {
      test({
        validator: "isByteLength",
        args: [{ min: 2 }],
        valid: ["abc", "de", "abcd", "\uff47\uff4d\uff41\uff49\uff4c"],
        invalid: ["", "a"],
      });
      test({
        validator: "isByteLength",
        args: [{ min: 2, max: 3 }],
        valid: ["abc", "de", "\uff47"],
        invalid: ["", "a", "abcd", "\uff47\uff4d"],
      });
      test({
        validator: "isByteLength",
        args: [{ max: 3 }],
        valid: ["abc", "de", "\uff47", "a", ""],
        invalid: ["abcd", "\uff47\uff4d"],
      });
      test({
        validator: "isByteLength",
        args: [{ max: 0 }],
        valid: [""],
        invalid: ["\uff47", "a"],
      });
    });

    it("should validate ULIDs", () => {
      test({
        validator: "isULID",
        valid: [
          "01HBGW8CWQ5Q6DTT7XP89VV4KT",
          "01HBGW8CWR8MZQMBG6FA2QHMDD",
          "01HBGW8CWS3MEEK12Y9G7SVW4V",
          "01hbgw8cws1tq2njavy9amb0wx",
          "01HBGW8cwS43H4jkQ0A4ZRJ7QV",
        ],
        invalid: [
          "",
          "01HBGW-CWS3MEEK1#Y9G7SVW4V",
          "91HBGW8CWS3MEEK12Y9G7SVW4V",
          "81HBGW8CWS3MEEK12Y9G7SVW4V",
          "934859",
          "01HBGW8CWS3MEEK12Y9G7SVW4VXXX",
          "01UBGW8IWS3MOEK12Y9G7SVW4V",
          "01HBGW8CuS43H4JKQ0A4ZRJ7QV",
        ],
      });
    });

    it("should validate UUIDs", () => {
      test({
        validator: "isUUID",
        valid: [
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "934859",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
        ],
      });
      test({
        validator: "isUUID",
        args: [undefined],
        valid: [
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A117FBC9-4BED-5078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
        invalid: [
          "",
          "A117FBC9-4BED-3078-CF07-9141BA07C9F3",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC94BED3078CF079141BA07C9F3",
          "A11AAAAA-1111-1111-AAAG-111111111111",
        ],
      });
      test({
        validator: "isUUID",
        args: [null],
        valid: [
          "A127FBC9-4BED-3078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A127FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A127FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "912859",
          "A12AAAAA-1111-1111-AAAG-111111111111",
        ],
      });
      test({
        validator: "isUUID",
        args: [1],
        valid: ["E034B584-7D89-11E9-9669-1AECF481A97B"],
        invalid: [
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "AAAAAAAA-1111-2222-AAAG",
          "AAAAAAAA-1111-2222-AAAG-111111111111",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
      });
      test({
        validator: "isUUID",
        args: [2],
        valid: ["A987FBC9-4BED-2078-AF07-9141BA07C9F3"],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "11111",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "A987FBC9-4BED-2078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
      });
      test({
        validator: "isUUID",
        args: [3],
        valid: ["9deb20fe-a6e0-355c-81ea-288b009e4f6d"],
        invalid: [
          "",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
      });
      test({
        validator: "isUUID",
        args: [4],
        valid: [
          "713ae7e3-cb32-45f9-adcb-7c4fa86b90c1",
          "625e63f3-58f5-40b7-83a1-a72ad31acffb",
          "57b73598-8764-4ad0-a76a-679bb6640eb1",
          "9c858901-8a57-4791-81fe-4c455b099bc9",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
      });
      test({
        validator: "isUUID",
        args: [5],
        valid: [
          "987FBC97-4BED-5078-AF07-9141BA07C9F3",
          "987FBC97-4BED-5078-BF07-9141BA07C9F3",
          "987FBC97-4BED-5078-8F07-9141BA07C9F3",
          "987FBC97-4BED-5078-9F07-9141BA07C9F3",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "9c858901-8a57-4791-81fe-4c455b099bc9",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
        ],
      });
      test({
        validator: "isUUID",
        args: [6],
        valid: ["1ef29908-cde1-69d0-be16-bfc8518a95f0"],
        invalid: [
          "987FBC97-4BED-1078-AF07-9141BA07C9F3",
          "987FBC97-4BED-2078-AF07-9141BA07C9F3",
          "987FBC97-4BED-3078-AF07-9141BA07C9F3",
          "987FBC97-4BED-4078-AF07-9141BA07C9F3",
          "987FBC97-4BED-5078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "987FBC97-4BED-8078-AF07-9141BA07C9F3",
        ],
      });
      test({
        validator: "isUUID",
        args: [7],
        valid: ["018C544A-D384-7000-BB74-3B1738ABE43C"],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "713ae7e3-cb32-45f9-adcb-7c4fa86b90c1",
          "625e63f3-58f5-40b7-83a1-a72ad31acffb",
          "57b73598-8764-4ad0-a76a-679bb6640eb1",
          "9c858901-8a57-4791-81fe-4c455b099bc9",
        ],
      });
      test({
        validator: "isUUID",
        args: [8],
        valid: ["018C544A-D384-8000-BB74-3B1738ABE43C"],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-7078-AF07-9141BA07C9F3",
          "713ae7e3-cb32-45f9-adcb-7c4fa86b90c1",
          "625e63f3-58f5-40b7-83a1-a72ad31acffb",
          "57b73598-8764-4ad0-a76a-679bb6640eb1",
          "9c858901-8a57-4791-81fe-4c455b099bc9",
        ],
      });
      test({
        validator: "isUUID",
        args: ["nil"],
        valid: ["00000000-0000-0000-0000-000000000000"],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "934859",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "ffffffff-ffff-ffff-ffff-ffffffffffff",
          "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF",
        ],
      });
      test({
        validator: "isUUID",
        args: ["max"],
        valid: [
          "ffffffff-ffff-ffff-ffff-ffffffffffff",
          "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "934859",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "00000000-0000-0000-0000-000000000000",
        ],
      });
      test({
        validator: "isUUID",
        args: ["loose"],
        valid: [
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
          "AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee",
          "EEEEEEEE-EEEE-EEEE-EEEE-EEEEEEEEEEEE",
          "99999999-9999-9999-9999-999999999999",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "934859",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
        ],
      });
      test({
        validator: "isUUID",
        args: ["all"],
        valid: [
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "00000000-0000-0000-0000-000000000000",
          "ffffffff-ffff-ffff-ffff-ffffffffffff",
          "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF",
        ],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "934859",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
        ],
      });
      test({
        validator: "isUUID",
        args: ["invalid"],
        valid: [],
        invalid: [
          "",
          "xxxA987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3",
          "A987FBC9-4BED-3078-CF07-9141BA07C9F3xxx",
          "A987FBC94BED3078CF079141BA07C9F3",
          "934859",
          "987FBC9-4BED-3078-CF07A-9141BA07C9F3",
          "AAAAAAAA-1111-1111-AAAG-111111111111",
          "9deb20fe-a6e0-355c-81ea-288b009e4f6d",
          "A987FBC9-4BED-4078-8F07-9141BA07C9F3",
          "A987FBC9-4BED-5078-AF07-9141BA07C9F3",
          "A987FBC9-4BED-6078-AF07-9141BA07C9F3",
          "018C544A-D384-7000-BB74-3B1738ABE43C",
          "A987FBC9-4BED-8078-AF07-9141BA07C9F3",
          "00000000-0000-0000-0000-000000000000",
          "ffffffff-ffff-ffff-ffff-ffffffffffff",
          "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF",
        ],
      });
    });

    it("should validate a string that is in another string or array", () => {
      test({
        validator: "isIn",
        args: ["foobar"],
        valid: ["foo", "bar", "foobar", ""],
        invalid: ["foobarbaz", "barfoo"],
      });
      test({
        validator: "isIn",
        args: [["foo", "bar"]],
        valid: ["foo", "bar"],
        invalid: ["foobar", "barfoo", ""],
      });
      test({
        validator: "isIn",
        args: [["1", "2", "3"]],
        valid: ["1", "2", "3"],
        invalid: ["4", ""],
      });
      test({
        validator: "isIn",
        args: [["1", "2", "3", { foo: "bar" }, () => 5, { toString: "test" }]],
        valid: ["1", "2", "3", ""],
        invalid: ["4"],
      });
      test({ validator: "isIn", invalid: ["foo", ""] });
    });

    it("should validate a string that is in another object", () => {
      test({
        validator: "isIn",
        args: [{ foo: 1, bar: 2, foobar: 3 }],
        valid: ["foo", "bar", "foobar"],
        invalid: ["foobarbaz", "barfoo", ""],
      });
      test({
        validator: "isIn",
        args: [{ 1: 3, 2: 0, 3: 1 }],
        valid: ["1", "2", "3"],
        invalid: ["4", ""],
      });
    });

    it("should validate ABA routing number", () => {
      test({
        validator: "isAbaRouting",
        valid: ["322070381", "011103093", "263170175", "124303065"],
        invalid: [
          "426317017",
          "789456124",
          "603558459",
          "qwerty",
          "12430306",
          "382070381",
        ],
      });
    });

    it("should validate IBAN", () => {
      test({
        validator: "isIBAN",
        valid: [
          "SC52BAHL01031234567890123456USD",
          "LC14BOSL123456789012345678901234",
          "MT31MALT01100000000000000000123",
          "SV43ACAT00000000000000123123",
          "EG800002000156789012345180002",
          "BE71 0961 2345 6769",
          "FR76 3000 6000 0112 3456 7890 189",
          "DE91 1000 0000 0123 4567 89",
          "GR96 0810 0010 0000 0123 4567 890",
          "RO09 BCYP 0000 0012 3456 7890",
          "SA44 2000 0001 2345 6789 1234",
          "ES79 2100 0813 6101 2345 6789",
          "CH56 0483 5012 3456 7800 9",
          "GB98 MIDL 0700 9312 3456 78",
          "IL170108000000012612345",
          "IT60X0542811101000000123456",
          "JO71CBJO0000000000001234567890",
          "TR320010009999901234567890",
          "BR1500000000000010932840814P2",
          "LB92000700000000123123456123",
          "IR200170000000339545727003",
          "MZ97123412341234123412341",
          "MA64011519000001205000534921",
          "VG96VPVG0000012345678901",
          "DZ580002100001113000000570",
          "IE29AIBK93115212345678",
          "PS92PALS000000000400123456702",
          "PS92PALS00000000040012345670O",
        ],
        invalid: [
          "XX22YYY1234567890123",
          "FR14 2004 1010 0505 0001 3",
          "FR7630006000011234567890189@",
          "FR7630006000011234567890189\ud83d\ude05",
          "FR763000600001123456!!\ud83e\udd287890189@",
          "VG46H07Y0223060094359858",
          "IE95TE8270900834048660",
          "PS072435171802145240705922007",
        ],
      });
      test({
        validator: "isIBAN",
        args: [{ whitelist: ["DK", "GB"] }],
        valid: ["DK5000400440116243", "GB29NWBK60161331926819"],
        invalid: [
          "BE71 0961 2345 6769",
          "FR76 3000 6000 0112 3456 7890 189",
          "DE91 1000 0000 0123 4567 89",
          "GR96 0810 0010 0000 0123 4567 890",
          "RO09 BCYP 0000 0012 3456 7890",
          "SA44 2000 0001 2345 6789 1234",
          "ES79 2100 0813 6101 2345 6789",
          "XX22YYY1234567890123",
          "FR14 2004 1010 0505 0001 3",
          "FR7630006000011234567890189@",
          "FR7630006000011234567890189\ud83d\ude05",
          "FR763000600001123456!!\ud83e\udd287890189@",
        ],
      });
      test({
        validator: "isIBAN",
        args: [{ whitelist: ["XX", "AA"] }],
        invalid: [
          "DK5000400440116243",
          "GB29NWBK60161331926819",
          "BE71 0961 2345 6769",
          "FR76 3000 6000 0112 3456 7890 189",
          "DE91 1000 0000 0123 4567 89",
          "GR96 0810 0010 0000 0123 4567 890",
          "RO09 BCYP 0000 0012 3456 7890",
          "SA44 2000 0001 2345 6789 1234",
          "ES79 2100 0813 6101 2345 6789",
          "XX22YYY1234567890123",
          "FR14 2004 1010 0505 0001 3",
          "FR7630006000011234567890189@",
          "FR7630006000011234567890189\ud83d\ude05",
          "FR763000600001123456!!\ud83e\udd287890189@",
        ],
      });
      test({
        validator: "isIBAN",
        args: [{ blacklist: ["IT"] }],
        valid: [
          "SC52BAHL01031234567890123456USD",
          "LC14BOSL123456789012345678901234",
          "MT31MALT01100000000000000000123",
          "SV43ACAT00000000000000123123",
          "EG800002000156789012345180002",
          "BE71 0961 2345 6769",
          "FR76 3000 6000 0112 3456 7890 189",
          "DE91 1000 0000 0123 4567 89",
          "GR96 0810 0010 0000 0123 4567 890",
          "RO09 BCYP 0000 0012 3456 7890",
          "SA44 2000 0001 2345 6789 1234",
          "ES79 2100 0813 6101 2345 6789",
          "CH56 0483 5012 3456 7800 9",
          "GB98 MIDL 0700 9312 3456 78",
          "IL170108000000012612345",
          "JO71CBJO0000000000001234567890",
          "TR320010009999901234567890",
          "BR1500000000000010932840814P2",
          "LB92000700000000123123456123",
          "IR200170000000339545727003",
          "MZ97123412341234123412341",
        ],
        invalid: [
          "XX22YYY1234567890123",
          "FR14 2004 1010 0505 0001 3",
          "FR7630006000011234567890189@",
          "FR7630006000011234567890189\ud83d\ude05",
          "FR763000600001123456!!\ud83e\udd287890189@",
          "IT60X0542811101000000123456",
        ],
      });
    });

    it("should validate BIC codes", () => {
      test({
        validator: "isBIC",
        valid: [
          "SBICKEN1345",
          "SBICKEN1",
          "SBICKENY",
          "SBICKEN1YYP",
          "SBICXKN1YYP",
        ],
        invalid: [
          "SBIC23NXXX",
          "S23CKENXXXX",
          "SBICKENXX",
          "SBICKENXX9",
          "SBICKEN13458",
          "SBICKEN",
          "SBICXK",
        ],
      });
    });

    it("should validate that integer strings are divisible by a number", () => {
      test({
        validator: "isDivisibleBy",
        args: [2],
        valid: ["2", "4", "100", "1000"],
        invalid: ["1", "2.5", "101", "foo", "", "2020-01-06T14:31:00.135Z"],
      });
    });

    it("should validate luhn numbers", () => {
      test({
        validator: "isLuhnNumber",
        valid: [
          "0",
          "5421",
          "01234567897",
          "0123456789012345678906",
          "0123456789012345678901234567891",
          "123456789012345678906",
          "375556917985515",
          "36050234196908",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4929 7226 5379 7141",
        ],
        invalid: [
          "",
          "1",
          "5422",
          "foo",
          "prefix6234917882863855",
          "623491788middle2863855",
          "6234917882863855suffix",
        ],
      });
    });

    it("should validate credit cards", () => {
      test({
        validator: "isCreditCard",
        valid: [
          "375556917985515",
          "36050234196908",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "6765780016990268",
          "4716989580001715211",
          "8171999927660000",
          "8171999900000000021",
        ],
        invalid: [
          "foo",
          "foo",
          "5398228707871528",
          "2718760626256571",
          "2721465526338453",
          "2220175103860763",
          "375556917985515999999993",
          "899999996234917882863855",
          "prefix6234917882863855",
          "623491788middle2863855",
          "6234917882863855suffix",
          "4716989580001715213",
        ],
      });
    });

    it("should validate credit cards without a proper provider", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "Plorf" }],
        error: [
          "foo",
          // valid cc #
          "375556917985515",
          "4716-2210-5188-5662",
          "375556917985515999999993",
          "6234917882863855suffix",
        ],
      });
    });

    it("should validate AmEx provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "AmEx" }],
        valid: ["375556917985515"],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "36050234196908",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate Diners Club provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "DinersClub" }],
        valid: ["36050234196908"],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "375556917985515",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate Discover provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "Discover" }],
        valid: ["6011111111111117", "6011000990139424"],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "36050234196908",
          "375556917985515",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate JCB provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "JCB" }],
        valid: ["3530111333300000", "3566002020360505"],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "36050234196908",
          "375556917985515",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate Mastercard provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "Mastercard" }],
        valid: [
          "2222155765072228",
          "2225855203075256",
          "2718760626256570",
          "2720428011723762",
          "5398228707871527",
        ],
        invalid: [
          "foo",
          "36050234196908",
          "375556917985515",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate Union Pay provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "UnionPay" }],
        valid: [
          "6226050967750613",
          "6234917882863855",
          "6234698580215388",
          "6246281879460688",
          "6263892624162870",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "36050234196908",
          "375556917985515",
          "375556917985515999999993",
          "4716461583322103",
          "4716-2210-5188-5662",
          "4716989580001715211",
          "4929 7226 5379 7141",
          "5398228707871527",
          "6234917882863855suffix",
        ],
      });
    });

    it("should validate Visa provided credit cards", () => {
      test({
        validator: "isCreditCard",
        args: [{ provider: "Visa" }],
        valid: [
          "4716-2210-5188-5662",
          "4716461583322103",
          "4716989580001715211",
          "4929 7226 5379 7141",
        ],
        invalid: [
          "foo",
          "2222155765072228",
          "2225855203075256",
          "2720428011723762",
          "2718760626256570",
          "36050234196908",
          "375556917985515",
          "375556917985515999999993",
          "5398228707871527",
          "6234917882863855suffix",
          "6283875070985593",
          "6263892624162870",
          "6234917882863855",
          "6234698580215388",
          "6226050967750613",
          "6246281879460688",
          "6283875070985593",
          "6765780016990268",
          "8171999927660000",
          "8171999900000000021",
        ],
      });
    });

    it("should validate identity cards", () => {
      const fixtures = [
        {
          locale: "PK",
          valid: [
            "45504-4185771-3",
            "39915-6182971-9",
            "21143-6182971-2",
            "34543-2323471-1",
            "72345-2345678-7",
            "63456-8765432-8",
            "55672-1234567-5",
            "21234-9876543-6",
          ],
          invalid: [
            "08000-1234567-5",
            "74321-87654321-1",
            "51234-98765-2",
            "00000-0000000-0",
            "88888-88888888-0",
            "99999-9999999-9",
            "11111",
          ],
        },
        {
          locale: "zh-HK",
          valid: [
            "OV290326[A]",
            "Q803337[0]",
            "Z0977986",
            "W520128(7)",
            "A494866[4]",
            "A494866(4)",
            "Z867821A",
            "ag293013(9)",
            "k348609(5)",
          ],
          invalid: [
            "A1234567890",
            "98765432",
            "O962472(9)",
            "M4578601",
            "X731324[8]",
            "C503134(5)",
            "RH265886(3)",
          ],
        },
        {
          locale: "LK",
          valid: [
            "722222222v",
            "722222222V",
            "993151225x",
            "993151225X",
            "188888388x",
            "935632124V",
            "199931512253",
            "200023125632",
          ],
          invalid: [
            "023125648V",
            "023345621v",
            "021354211X",
            "055321231x",
            "02135465462",
            "199931512253X",
          ],
        },
        {
          locale: "PL",
          valid: [
            "99012229019",
            "09210215408",
            "20313034701",
            "86051575214",
            "77334586883",
            "54007481320",
            "06566860643",
            "77552478861",
          ],
          invalid: [
            "aa",
            "5",
            "195",
            "",
            " ",
            "12345678901",
            "99212229019",
            "09210215402",
            "20313534701",
            "86241579214",
          ],
        },
        {
          locale: "ES",
          valid: [
            "99999999R",
            "12345678Z",
            "01234567L",
            "01234567l",
            "X1234567l",
            "x1234567l",
            "X1234567L",
            "Y1234567X",
            "Z1234567R",
          ],
          invalid: [
            "123456789",
            "12345678A",
            "12345 678Z",
            "12345678-Z",
            "1234*6789",
            "1234*678Z",
            "12345678!",
            "1234567L",
            "A1234567L",
            "X1234567A",
            "Y1234567B",
            "Z1234567C",
          ],
        },
        {
          locale: "FI",
          valid: [
            "131052-308T", // People born in 1900s
            "131052A308T", // People born in 2000s
            "131052+308T", // People born in 1800s
            "131052-313Y",
          ],
          invalid: ["131052308T", "131052-308T ", "131052-308A"],
        },
        {
          locale: "IN",
          valid: ["298448863364", "2984 4886 3364"],
          invalid: [
            "99999999R",
            "12345678Z",
            "01234567L",
            "01234567l",
            "X1234567l",
            "x1234567l",
            "X1234567L",
          ],
        },
        {
          locale: "IR",
          valid: [
            "0499370899",
            "0790419904",
            "0084575948",
            "0963695398",
            "0684159414",
            "0067749828",
            "0650451252",
            "1583250689",
            "4032152314",
            "0076229645",
            "4271467685",
            "0200203241",
          ],
          invalid: [
            "1260293040",
            "0000000001",
            "1999999999",
            "9999999991",
            "AAAAAAAAAA",
            "0684159415",
          ],
        },
        {
          locale: "IT",
          valid: ["CR43675TM", "CA79382RA"],
          invalid: ["CA00000AA", "CB2342TG", "CS123456A", "C1236EC"],
        },
        {
          locale: "NO",
          valid: [
            "09053426694",
            "26028338723",
            "08031470790",
            "12051539514",
            "02077448074",
            "14035638319",
            "13031379673",
            "29126214926",
          ],
          invalid: ["09053426699", "00000000000", "26028338724", "92031470790"],
        },
        {
          locale: "TH",
          valid: ["1101230000001", "1101230000060"],
          invalid: [
            "abc",
            "1101230",
            "11012300000011",
            "aaaaaaaaaaaaa",
            "110123abcd001",
            "1101230000007",
            "0101123450000",
            "0101123450004",
            "9101123450008",
          ],
        },
        {
          locale: "he-IL",
          valid: [
            "219472156",
            "219486610",
            "219488962",
            "219566726",
            "219640216",
            "219645041",
            "334795465",
            "335211686",
            "335240479",
            "335472171",
            "336999842",
            "337090443",
          ],
          invalid: [
            "123456789",
            "12345678A",
            "12345 678Z",
            "12345678-Z",
            "1234*6789",
            "1234*678Z",
            "12345678!",
            "1234567L",
            "A1234567L",
            "X1234567A",
            "Y1234567B",
            "Z1234567C",
            "219772156",
            "219487710",
            "334705465",
            "336000842",
          ],
        },
        {
          locale: "ar-LY",
          valid: [
            "119803455876",
            "120024679875",
            "219624876201",
            "220103480657",
          ],
          invalid: [
            "987654320123",
            "123-456-7890",
            "012345678912",
            "1234567890",
            "AFJBHUYTREWR",
            "C4V6B1X0M5T6",
            "9876543210123",
          ],
        },
        {
          locale: "ar-TN",
          valid: [
            "09958092",
            "09151092",
            "65126506",
            "79378815",
            "58994407",
            "73089789",
            "73260311",
          ],
          invalid: [
            "123456789546",
            "123456789",
            "023456789",
            "12345678A",
            "12345",
            "1234578A",
            "123 578A",
            "12345 678Z",
            "12345678-Z",
            "1234*6789",
            "1234*678Z",
            "GE9800as98",
            "X231071922",
            "1234*678Z",
            "12345678!",
          ],
        },
        {
          locale: "zh-CN",
          valid: [
            "235407195106112745",
            "210203197503102721",
            "520323197806058856",
            "110101491001001",
          ],
          invalid: [
            "160323197806058856",
            "010203197503102721",
            "520323297806058856",
            "520323197802318856",
            "235407195106112742",
            "010101491001001",
            "110101491041001",
            "160101491001001",
            "110101940231001",
            "xx1234567",
            "135407195106112742",
            "123456789546",
            "123456789",
            "023456789",
            "12345678A",
            "12345",
            "1234578A",
            "123 578A",
            "12345 678Z",
            "12345678-Z",
            "1234*6789",
            "1234*678Z",
            "GE9800as98",
            "X231071922",
            "1234*678Z",
            "12345678!",
            "235407207006112742",
          ],
        },
        {
          locale: "zh-TW",
          valid: [
            "B176944193",
            "K101189797",
            "F112866121",
            "A219758834",
            "A244144802",
            "A146047171",
            "Q170219004",
            "Z277018381",
            "X231071923",
          ],
          invalid: [
            "123456789",
            "A185034995",
            "X431071923",
            "GE9800as98",
            "X231071922",
            "1234*678Z",
            "12345678!",
            "1234567L",
            "A1234567L",
            "X1234567A",
            "Y1234567B",
            "Z1234567C",
            "219772156",
            "219487710",
            "334705465",
            "336000842",
          ],
        },
      ];

      let allValid = [];

      // Test fixtures
      fixtures.forEach((fixture) => {
        if (fixture.valid) allValid = allValid.concat(fixture.valid);
        test({
          validator: "isIdentityCard",
          valid: fixture.valid,
          invalid: fixture.invalid,
          args: [fixture.locale],
        });
      });

      // Test generics
      test({
        validator: "isIdentityCard",
        valid: [...allValid],
        invalid: ["foo"],
        args: ["any"],
      });
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isIdentityCard",
        args: ["is-NOT"],
        error: ["99999999R", "12345678Z"],
      });
    });

    it("should validate ISINs", () => {
      test({
        validator: "isISIN",
        valid: [
          "AU0000XVGZA3",
          "DE000BAY0017",
          "BE0003796134",
          "SG1G55870362",
          "GB0001411924",
          "DE000WCH8881",
          "PLLWBGD00016",
          "US0378331005",
        ],
        invalid: ["DE000BAY0018", "PLLWBGD00019", "foo", "5398228707871528"],
      });
    });

    it("should validate EANs", () => {
      test({
        validator: "isEAN",
        valid: [
          "9421023610112",
          "1234567890128",
          "4012345678901",
          "9771234567003",
          "9783161484100",
          "73513537",
          "00012345600012",
          "10012345678902",
          "20012345678909",
        ],
        invalid: ["5901234123451", "079777681629", "0705632085948"],
      });
    });

    it("should validate ISSNs", () => {
      test({
        validator: "isISSN",
        valid: [
          "0378-5955",
          "0000-0000",
          "2434-561X",
          "2434-561x",
          "01896016",
          "20905076",
        ],
        invalid: [
          "0378-5954",
          "0000-0001",
          "0378-123",
          "037-1234",
          "0",
          "2434-561c",
          "1684-5370",
          "19960791",
          "",
        ],
      });
      test({
        validator: "isISSN",
        args: [{ case_sensitive: true }],
        valid: ["2434-561X", "2434561X", "0378-5955", "03785955"],
        invalid: ["2434-561x", "2434561x"],
      });
      test({
        validator: "isISSN",
        args: [{ require_hyphen: true }],
        valid: ["2434-561X", "2434-561x", "0378-5955"],
        invalid: ["2434561X", "2434561x", "03785955"],
      });
      test({
        validator: "isISSN",
        args: [{ case_sensitive: true, require_hyphen: true }],
        valid: ["2434-561X", "0378-5955"],
        invalid: ["2434-561x", "2434561X", "2434561x", "03785955"],
      });
    });

    it("should validate JSON", () => {
      test({
        validator: "isJSON",
        valid: ['{ "key": "value" }', "{}"],
        invalid: [
          '{ key: "value" }',
          "{ 'key': 'value' }",
          "null",
          "1234",
          '"nope"',
        ],
      });
    });

    it("should validate JSON with primitives", () => {
      test({
        validator: "isJSON",
        args: [{ allow_primitives: true }],
        valid: ['{ "key": "value" }', "{}", "null", "false", "true"],
        invalid: [
          '{ key: "value" }',
          "{ 'key': 'value' }",
          '{ "key": value }',
          "1234",
          '"nope"',
        ],
      });
    });

    it("should validate multibyte strings", () => {
      test({
        validator: "isMultibyte",
        valid: [
          "\u3072\u3089\u304c\u306a\u30fb\u30ab\u30bf\u30ab\u30ca\u3001\uff0e\u6f22\u5b57",
          "\u3042\u3044\u3046\u3048\u304a foobar",
          "test\uff20example.com",
          "1234abcDE\uff58\uff59\uff5a",
          "\uff76\uff80\uff76\uff85",
          "\u4e2d\u6587",
        ],
        invalid: ["abc", "abc123", '<>@" *.'],
      });
    });

    it("should validate ascii strings", () => {
      test({
        validator: "isAscii",
        valid: ["foobar", "0987654321", "test@example.com", "1234abcDEF"],
        invalid: ["\uff46\uff4f\uff4fbar", "\uff58\uff59\uff5a\uff10\uff19\uff18", "\uff11\uff12\uff13456", "\uff76\uff80\uff76\uff85"],
      });
    });

    it("should validate full-width strings", () => {
      test({
        validator: "isFullWidth",
        valid: [
          "\u3072\u3089\u304c\u306a\u30fb\u30ab\u30bf\u30ab\u30ca\u3001\uff0e\u6f22\u5b57",
          "\uff13\u30fc\uff10\u3000\uff41\uff20\uff43\uff4f\uff4d",
          "\uff26\uff76\uff80\uff76\uff85\uff9e\uffac",
          "Good\uff1dParts",
        ],
        invalid: ["abc", "abc123", '!"#$%&()<>/+=-_? ~^|.,@`{}[]'],
      });
    });

    it("should validate half-width strings", () => {
      test({
        validator: "isHalfWidth",
        valid: [
          '!"#$%&()<>/+=-_? ~^|.,@`{}[]',
          "l-btn_02--active",
          "abc123\u3044",
          "\uff76\uff80\uff76\uff85\uff9e\uffac\uffe9",
        ],
        invalid: ["\u3042\u3044\u3046\u3048\u304a", "\uff10\uff10\uff11\uff11"],
      });
    });

    it("should validate variable-width strings", () => {
      test({
        validator: "isVariableWidth",
        valid: [
          "\u3072\u3089\u304c\u306a\u30ab\u30bf\u30ab\u30ca\u6f22\u5b57ABCDE",
          "\uff13\u30fc\uff10123",
          "\uff26\uff76\uff80\uff76\uff85\uff9e\uffac",
          "Good\uff1dParts",
        ],
        invalid: [
          "abc",
          "abc123",
          '!"#$%&()<>/+=-_? ~^|.,@`{}[]',
          "\u3072\u3089\u304c\u306a\u30fb\u30ab\u30bf\u30ab\u30ca\u3001\uff0e\u6f22\u5b57",
          "\uff11\uff12\uff13\uff14\uff15\uff16",
          "\uff76\uff80\uff76\uff85\uff9e\uffac",
        ],
      });
    });

    it("should validate surrogate pair strings", () => {
      test({
        validator: "isSurrogatePair",
        valid: ["\ud842\udfb7\u91ce\ud842\udfb7", "\ud867\ude3d", "ABC\u5343\ud856\uddc41-2-3"],
        invalid: ["\u5409\u91ce\u7ac8", "\u9baa", "ABC1-2-3"],
      });
    });

    it("should validate Semantic Versioning Specification (SemVer) strings", () => {
      test({
        validator: "isSemVer",
        valid: [
          "0.0.4",
          "1.2.3",
          "10.20.30",
          "1.1.2-prerelease+meta",
          "1.1.2+meta",
          "1.1.2+meta-valid",
          "1.0.0-alpha",
          "1.0.0-beta",
          "1.0.0-alpha.beta",
          "1.0.0-alpha.beta.1",
          "1.0.0-alpha.1",
          "1.0.0-alpha0.valid",
          "1.0.0-alpha.0valid",
          "1.0.0-alpha-a.b-c-somethinglong+build.1-aef.1-its-okay",
          "1.0.0-rc.1+build.1",
          "2.0.0-rc.1+build.123",
          "1.2.3-beta",
          "10.2.3-DEV-SNAPSHOT",
          "1.2.3-SNAPSHOT-123",
          "1.0.0",
          "2.0.0",
          "1.1.7",
          "2.0.0+build.1848",
          "2.0.1-alpha.1227",
          "1.0.0-alpha+beta",
          "1.2.3----RC-SNAPSHOT.12.9.1--.12+788",
          "1.2.3----R-S.12.9.1--.12+meta",
          "1.2.3----RC-SNAPSHOT.12.9.1--.12",
          "1.0.0+0.build.1-rc.10000aaa-kk-0.1",
          "99999999999999999999999.999999999999999999.99999999999999999",
          "1.0.0-0A.is.legal",
        ],
        invalid: [
          "-invalid+invalid",
          "-invalid.01",
          "alpha",
          "alpha.beta",
          "alpha.beta.1",
          "alpha.1",
          "alpha+beta",
          "alpha_beta",
          "alpha.",
          "alpha..",
          "beta",
          "1.0.0-alpha_beta",
          "-alpha.",
          "1.0.0-alpha..",
          "1.0.0-alpha..1",
          "1.0.0-alpha...1",
          "1.0.0-alpha....1",
          "1.0.0-alpha.....1",
          "1.0.0-alpha......1",
          "1.0.0-alpha.......1",
          "01.1.1",
          "1.01.1",
          "1.1.01",
          "1.2",
          "1.2.3.DEV",
          "1.2-SNAPSHOT",
          "1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788",
          "1.2-RC-SNAPSHOT",
          "-1.0.3-gamma+b7718",
          "+justmeta",
          "9.8.7+meta+meta",
          "9.8.7-whatever+meta+meta",
          "99999999999999999999999.999999999999999999.99999999999999999-",
          "---RC-SNAPSHOT.12.09.1--------------------------------..12",
        ],
      });
    });

    it("should validate base32 strings", () => {
      test({
        validator: "isBase32",
        valid: [
          "ZG======",
          "JBSQ====",
          "JBSWY===",
          "JBSWY3A=",
          "JBSWY3DP",
          "JBSWY3DPEA======",
          "K5SWYY3PNVSSA5DPEBXG6ZA=",
          "K5SWYY3PNVSSA5DPEBXG6===",
        ],
        invalid: [
          "12345",
          "",
          "JBSWY3DPtesting123",
          "ZG=====",
          "Z======",
          "Zm=8JBSWY3DP",
          "=m9vYg==",
          "Zm9vYm/y====",
        ],
      });
    });

    it("should validate base32 strings with crockford alternative", () => {
      test({
        validator: "isBase32",
        args: [{ crockford: true }],
        valid: ["91JPRV3F41BPYWKCCGGG", "60", "64", "B5QQA833C5Q20S3F41MQ8"],
        invalid: [
          "91JPRV3F41BUPYWKCCGGG",
          "B5QQA833C5Q20S3F41MQ8L",
          "60I",
          "B5QQA833OULIC5Q20S3F41MQ8",
        ],
      });
    });

    it("should validate base58 strings", () => {
      test({
        validator: "isBase58",
        valid: [
          "BukQL",
          "3KMUV89zab",
          "91GHkLMNtyo98",
          "YyjKm3H",
          "Mkhss145TRFg",
          "7678765677",
          "abcodpq",
          "AAVHJKLPY",
        ],
        invalid: [
          "0OPLJH",
          "IMKLP23",
          "KLMOmk986",
          "LL1l1985hG",
          "*MP9K",
          "Zm=8JBSWY3DP",
          ")()(=9292929MKL",
        ],
      });
    });

    it("should validate hex-encoded MongoDB ObjectId", () => {
      test({
        validator: "isMongoId",
        valid: ["507f1f77bcf86cd799439011"],
        invalid: [
          "507f1f77bcf86cd7994390",
          "507f1f77bcf86cd79943901z",
          "",
          "507f1f77bcf86cd799439011 ",
        ],
      });
    });

    it("should validate mobile phone number", () => {
      let fixtures = [
        {
          locale: "am-AM",
          valid: [
            "+37433123456",
            "+37441123456",
            "+37443123456",
            "+37444123456",
            "+37455123456",
            "+37477123456",
            "+37488123456",
            "+37491123456",
            "+37493123456",
            "+37494123456",
            "+37495123456",
            "+37496123456",
            "+37498123456",
            "+37499123456",
            "055123456",
            "37455123456",
          ],
          invalid: [
            "12345",
            "+37403498855",
            "+37416498123",
            "05614988556",
            "",
            "37456789000",
            "37486789000",
            "+37431312345",
            "+37430312345",
            "+37460123456",
            "+37410324123",
            "+37422298765",
            "+37431276521",
            "022698763",
            "+37492123456",
          ],
        },
        {
          locale: "ar-AE",
          valid: [
            "+971502674453",
            "+971521247658",
            "+971541255684",
            "+971555454458",
            "+971561498855",
            "+971585215778",
            "971585215778",
            "0585215778",
            "585215778",
          ],
          invalid: [
            "12345",
            "+971511498855",
            "+9715614988556",
            "+9745614988556",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
            "962796477263",
          ],
        },
        {
          locale: "ar-BH",
          valid: [
            "+97335078110",
            "+97339534385",
            "+97366331055",
            "+97333146000",
            "97335078110",
            "35078110",
            "66331055",
          ],
          invalid: [
            "12345",
            "+973350781101",
            "+97379534385",
            "+973035078110",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
            "962796477263",
            "035078110",
            "16331055",
            "hello",
            "+9733507811a",
          ],
        },
        {
          locale: "ar-EG",
          valid: [
            "+201004513789",
            "+201111453489",
            "+201221204610",
            "+201144621154",
            "+201200124304",
            "+201011201564",
            "+201124679001",
            "+201064790156",
            "+201274652177",
            "+201280134679",
            "+201090124576",
            "+201583728900",
            "201599495596",
            "201090124576",
            "01090124576",
            "01538920744",
            "1593075993",
            "1090124576",
          ],
          invalid: [
            "+221004513789",
            "+201404513789",
            "12345",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
            "962796477263",
          ],
        },
        {
          locale: "ar-JO",
          valid: [
            "0796477263",
            "0777866254",
            "0786725261",
            "+962796477263",
            "+962777866254",
            "+962786725261",
            "962796477263",
            "962777866254",
            "962786725261",
          ],
          invalid: [
            "00962786725261",
            "00962796477263",
            "12345",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
          ],
        },
        {
          locale: "ar-KW",
          valid: [
            "96550000000",
            "96560000000",
            "96590000000",
            "96541000000",
            "+96550000000",
            "+96550000220",
            "+96551111220",
            "+96541000000",
          ],
          invalid: [
            "+96570000220",
            "00962786725261",
            "00962796477263",
            "12345",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
            "+96540000000",
          ],
        },
        {
          locale: "ar-LB",
          valid: [
            "+96171234568",
            "+9613123456",
            "3456123",
            "3123456",
            "81978468",
            "77675798",
          ],
          invalid: [
            "+961712345688888",
            "00912220000",
            "7767579888",
            "+0921110000",
            "+3123456888",
            "021222200000",
            "213333444444",
            "",
            "+212234",
            "+21",
            "02122333",
          ],
        },
        {
          locale: "ar-LY",
          valid: [
            "912220000",
            "0923330000",
            "218945550000",
            "+218958880000",
            "212220000",
            "0212220000",
            "+218212220000",
          ],
          invalid: [
            "9122220000",
            "00912220000",
            "09211110000",
            "+0921110000",
            "+2180921110000",
            "021222200000",
            "213333444444",
            "",
            "+212234",
            "+21",
            "02122333",
          ],
        },
        {
          locale: "ar-MA",
          valid: [
            "0522714782",
            "0690851123",
            "0708186135",
            "+212522714782",
            "+212690851123",
            "+212708186135",
            "00212522714782",
            "00212690851123",
            "00212708186135",
          ],
          invalid: [
            "522714782",
            "690851123",
            "708186135",
            "212522714782",
            "212690851123",
            "212708186135",
            "0212522714782",
            "0212690851123",
            "0212708186135",
            "",
            "12345",
            "0922714782",
            "+212190851123",
            "00212408186135",
          ],
        },
        {
          locale: "dz-BT",
          valid: [
            "+97517374354",
            "+97517454971",
            "77324646",
            "016329712",
            "97517265559",
          ],
          invalid: ["", "9898347255", "+96326626262", "963372", "0114152198"],
        },
        {
          locale: "ar-OM",
          valid: [
            "+96891212121",
            "+96871212121",
            "0096899999999",
            "93112211",
            "99099009",
          ],
          invalid: [
            "+96890212121",
            "0096890999999",
            "0090999999",
            "+9689021212",
            "",
            "+212234",
            "+21",
            "02122333",
          ],
        },
        {
          locale: "ar-PS",
          valid: ["+970563459876", "970592334218", "0566372345", "0598273583"],
          invalid: [
            "+9759029487",
            "97059123456789",
            "598372348",
            "97058aaaafjd",
            "",
            "05609123484",
            "+97059",
            "+970",
            "97056",
          ],
        },
        {
          locale: "ar-SY",
          valid: [
            "0944549710",
            "+963944549710",
            "956654379",
            "0944549710",
            "0962655597",
          ],
          invalid: [
            "12345",
            "",
            "+9639626626262",
            "+963332210972",
            "0114152198",
          ],
        },
        {
          locale: "ar-SA",
          valid: [
            "0556578654",
            "+966556578654",
            "966556578654",
            "596578654",
            "572655597",
          ],
          invalid: [
            "12345",
            "",
            "+9665626626262",
            "+96633221097",
            "0114152198",
          ],
        },
        {
          locale: "ar-SD",
          valid: ["0128652312", "+249919425113", "249123212345", "0993212345"],
          invalid: [
            "12345",
            "",
            "+249972662622",
            "+24946266262",
            "+24933221097",
            "0614152198",
            "096554",
          ],
        },
        {
          locale: "ar-TN",
          valid: ["23456789", "+21623456789", "21623456789"],
          invalid: [
            "12345",
            "75200123",
            "+216512345678",
            "13520459",
            "85479520",
          ],
        },
        {
          locale: "bg-BG",
          valid: ["+359897123456", "+359898888888", "0897123123"],
          invalid: [
            "",
            "0898123",
            "+359212555666",
            "18001234567",
            "12125559999",
          ],
        },
        {
          locale: "bn-BD",
          valid: [
            "+8801794626846",
            "01399098893",
            "8801671163269",
            "01717112029",
            "8801898765432",
            "+8801312345678",
            "01494676946",
          ],
          invalid: [
            "",
            "0174626346",
            "017943563469",
            "18001234567",
            "0131234567",
          ],
        },
        {
          locale: "bs-BA",
          valid: [
            "060123456",
            "061123456",
            "062123456",
            "063123456",
            "0641234567",
            "065123456",
            "066123456",
            "+38760123456",
            "+38761123456",
            "+38762123456",
            "+38763123456",
            "+387641234567",
            "+38765123456",
            "+38766123456",
            "0038760123456",
            "0038761123456",
            "0038762123456",
            "0038763123456",
            "00387641234567",
            "0038765123456",
            "0038766123456",
          ],
          invalid: [
            "0601234567",
            "0611234567",
            "06212345",
            "06312345",
            "064123456",
            "0651234567",
            "06612345",
            "+3866123456",
            "+3856123456",
            "00038760123456",
            "038761123456",
          ],
        },
        {
          locale: "cs-CZ",
          valid: [
            "+420 123 456 789",
            "+420 123456789",
            "+420123456789",
            "123 456 789",
            "123456789",
          ],
          invalid: [
            "",
            "+42012345678",
            "+421 123 456 789",
            "+420 023456789",
            "+4201234567892",
          ],
        },
        {
          locale: "sk-SK",
          valid: [
            "+421 123 456 789",
            "+421 123456789",
            "+421123456789",
            "123 456 789",
            "123456789",
          ],
          invalid: [
            "",
            "+42112345678",
            "+422 123 456 789",
            "+421 023456789",
            "+4211234567892",
          ],
        },
        {
          locale: "de-DE",
          valid: [
            "+4915123456789",
            "015123456789",
            "015123456789",
            "015623456789",
            "015623456789",
            "01601234567",
            "016012345678",
            "01621234567",
            "01631234567",
            "01701234567",
            "017612345678",
          ],
          invalid: [
            "+4930405044550",
            "34412345678",
            "14412345678",
            "16212345678",
            "1761234567",
            "16412345678",
            "17012345678",
            "+4912345678910",
            "+49015123456789",
            "015345678910",
            "015412345678",
          ],
        },
        {
          locale: "de-AT",
          valid: [
            "+436761234567",
            "06761234567",
            "00436123456789",
            "+436123456789",
            "01999",
            "+4372876",
            "06434908989562345",
          ],
          invalid: ["167612345678", "1234", "064349089895623459"],
        },
        {
          locale: "hu-HU",
          valid: ["06301234567", "+36201234567", "06701234567"],
          invalid: ["1234", "06211234567", "+3620123456"],
        },
        {
          locale: "mz-MZ",
          valid: [
            "+258849229754",
            "258849229754",
            "849229754",
            "829229754",
            "839229754",
            "869229754",
            "859229754",
            "869229754",
            "879229754",
            "+258829229754",
            "+258839229754",
            "+258869229754",
            "+258859229754",
            "+258869229754",
            "+258879229754",
            "258829229754",
            "258839229754",
            "258869229754",
            "258859229754",
            "258869229754",
            "258879229754",
          ],
          invalid: [
            "+248849229754",
            "158849229754",
            "249229754",
            "819229754",
            "899229754",
            "889229754",
            "89229754",
            "8619229754",
            "87922975411",
            "257829229754",
            "+255839229754",
            "+2258869229754",
            "+1258859229754",
            "+2588692297541",
            "+2588792519754",
            "25882922975411",
          ],
        },
        {
          locale: "pt-BR",
          valid: [
            "+55 12 996551215",
            "+55 15 97661234",
            "+55 (12) 996551215",
            "+55 (15) 97661234",
            "55 (17) 96332-2155",
            "55 (17) 6332-2155",
            "55 15 976612345",
            "55 15 75661234",
            "+5512984567890",
            "+551283456789",
            "5512984567890",
            "551283456789",
            "015994569878",
            "01593456987",
            "022995678947",
            "02299567894",
            "(22)99567894",
            "(22)9956-7894",
            "(22) 99567894",
            "(22) 9956-7894",
            "(22)999567894",
            "(22)99956-7894",
            "(22) 999567894",
            "(22) 99956-7894",
            "(11) 94123-4567",
            "(11) 91431-4567",
            "+55 (11) 91431-4567",
            "+55 11 91431-4567",
            "+551191431-4567",
            "5511914314567",
            "5511912345678",
          ],
          invalid: [
            "0819876543",
            "+55 15 7566123",
            "+017 123456789",
            "5501599623874",
            "+55012962308",
            "+55 015 1234-3214",
            "+55 11 90431-4567",
            "+55 (11) 90431-4567",
            "+551190431-4567",
            "5511904314567",
            "5511902345678",
            "(11) 90431-4567",
          ],
        },
        {
          locale: "zh-CN",
          valid: [
            "13523333233",
            "13838389438",
            "14899230918",
            "14999230918",
            "15323456787",
            "15052052020",
            "16237108167",
            "008616238234822",
            "+8616238234822",
            "16565600001",
            "17269427292",
            "17469427292",
            "18199617480",
            "19151751717",
            "19651751717",
            "+8613238234822",
            "+8613487234567",
            "+8617823492338",
            "+8617823492338",
            "+8616637108167",
            "+8616637108167",
            "+8616712341234",
            "+8619912341234",
            "+8619812341234",
            "+8619712341234",
            "+8619612341234",
            "+8619512341234",
            "+8619312341234",
            "+8619212341234",
            "+8619112341234",
            "+8617269427292",
            "008618812341234",
            "008618812341234",
            "008617269427292",
            // Reserve number segments in the future.
            "92138389438",
            "+8692138389438",
            "008692138389438",
            "98199649964",
            "+8698099649964",
            "008698099649964",
          ],
          invalid: [
            "12345",
            "",
            "12038389438",
            "12838389438",
            "013838389438",
            "+86-13838389438",
            "+08613811211114",
            "+008613811211114",
            "08613811211114",
            "0086-13811211114",
            "0086-138-1121-1114",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
          ],
        },
        {
          locale: "zh-TW",
          valid: [
            "0987123456",
            "+886987123456",
            "886987123456",
            "+886-987123456",
            "886-987123456",
          ],
          invalid: ["12345", "", "Vml2YW11cyBmZXJtZtesting123", "0-987123456"],
        },
        {
          locale: "en-LS",
          valid: [
            "+26622123456",
            "+26628123456",
            "+26657123456",
            "+26658123456",
            "+26659123456",
            "+26627123456",
            "+26652123456",
          ],
          invalid: [
            "+26612345678",
            "",
            "2664512-21",
            "+2662212345678",
            "someString",
          ],
        },
        {
          locale: "en-BM",
          valid: ["+14417974653", "14413986653", "4415370973", "+14415005489"],
          invalid: [
            "85763287",
            "+14412020436",
            "+14412236546",
            "+14418245567",
            "+14416546789",
            "44087635627",
            "+4418970973",
            "",
            "+1441897465",
            "+1441897465 additional invalid string part",
          ],
        },
        {
          locale: "en-BS",
          valid: [
            "+12421231234",
            "2421231234",
            "+1-2421231234",
            "+1-242-123-1234",
            "(242)-123-1234",
            "+1 (242)-123-1234",
            "242 123-1234",
            "(242) 123 1234",
          ],
          invalid: [
            "85763287",
            "+1 242 12 12 12 12",
            "+1424123123",
            "+14418245567",
            "+14416546789",
            "not a number",
            "",
          ],
        },
        {
          locale: "en-ZA",
          valid: ["0821231234", "+27821231234", "27821231234"],
          invalid: [
            "082123",
            "08212312345",
            "21821231234",
            "+21821231234",
            "+0821231234",
          ],
        },
        {
          locale: "en-AU",
          valid: ["61404111222", "+61411222333", "0417123456"],
          invalid: [
            "082123",
            "08212312345",
            "21821231234",
            "+21821231234",
            "+0821231234",
            "04123456789",
          ],
        },
        {
          locale: "es-BO",
          valid: [
            "+59175553635",
            "+59162223685",
            "+59179783890",
            "+59160081890",
            "79783890",
            "60081890",
          ],
          invalid: [
            "082123",
            "08212312345",
            "21821231234",
            "+21821231234",
            "+59199783890",
          ],
        },
        {
          locale: "en-GG",
          valid: [
            "+441481123456",
            "+441481789123",
            "441481123456",
            "441481789123",
          ],
          invalid: ["999", "+441481123456789", "+447123456789"],
        },
        {
          locale: "en-GH",
          valid: [
            "0202345671",
            "0502345671",
            "0242345671",
            "0542345671",
            "0532345671",
            "0272345671",
            "0572345671",
            "0262345671",
            "0562345671",
            "0232345671",
            "0282345671",
            "+233202345671",
            "+233502345671",
            "+233242345671",
            "+233542345671",
            "+233532345671",
            "+233272345671",
            "+233572345671",
            "+233262345671",
            "+233562345671",
            "+233232345671",
            "+233282345671",
            "+233592349493",
            "0550298219",
          ],
          invalid: ["082123", "232345671", "0292345671", "+233292345671"],
        },
        {
          locale: "en-GY",
          valid: ["+5926121234", "06121234", "06726381", "+5926726381"],
          invalid: [
            "5926121234",
            "6121234",
            "+592 6121234",
            "05926121234",
            "+592-6121234",
          ],
        },
        {
          locale: "en-HK",
          valid: [
            "91234567",
            "9123-4567",
            "61234567",
            "51234567",
            "+85291234567",
            "+852-91234567",
            "+852-9123-4567",
            "+852 9123 4567",
            "9123 4567",
            "852-91234567",
          ],
          invalid: ["999", "+852-912345678", "123456789", "+852-1234-56789"],
        },
        {
          locale: "en-MO",
          valid: [
            "61234567",
            "+85361234567",
            "+853-61234567",
            "+853-6123-4567",
            "+853 6123 4567",
            "6123 4567",
            "853-61234567",
          ],
          invalid: [
            "999",
            "12345678",
            "612345678",
            "+853-12345678",
            "+853-22345678",
            "+853-82345678",
            "+853-612345678",
            "+853-1234-5678",
            "+853 1234 5678",
            "+853-6123-45678",
          ],
        },
        {
          locale: "en-IE",
          valid: [
            "+353871234567",
            "353831234567",
            "353851234567",
            "353861234567",
            "353871234567",
            "353881234567",
            "353891234567",
            "0871234567",
            "0851234567",
          ],
          invalid: [
            "999",
            "+353341234567",
            "+33589484858",
            "353841234567",
            "353811234567",
          ],
        },
        {
          locale: "en-JM",
          valid: ["+8761021234", "8761211234", "8763511274", "+8764511274"],
          invalid: [
            "999",
            "+876102123422",
            "+8861021234",
            "8761021212213",
            "876102123",
          ],
        },
        {
          locale: "en-KE",
          valid: [
            "+254728590432",
            "+254733875610",
            "254728590234",
            "0733346543",
            "0700459022",
            "0110934567",
            "+254110456794",
            "254198452389",
          ],
          invalid: ["999", "+25489032", "123456789", "+254800723845"],
        },
        {
          locale: "fr-CF",
          valid: [
            "+23670850000",
            "+23675038756",
            "+23677859002",
            "+23672854202",
            "+23621854052",
            "+23622854072",
            "72234650",
            "70045902",
            "77934567",
            "21456794",
            "22452389",
          ],
          invalid: [
            "+23689032",
            "123456789",
            "+236723845987",
            "022452389",
            "+236772345678",
            "+236700456794",
          ],
        },
        {
          locale: "en-KI",
          valid: ["+68673140000", "68673059999", "+68663000000", "68663019999"],
          invalid: [
            "+68653000000",
            "68664019999",
            "+68619019999",
            "686123456789",
            "+686733445",
          ],
        },
        {
          locale: "en-MT",
          valid: ["+35699000000", "+35679000000", "99000000"],
          invalid: ["356", "+35699000", "+35610000000"],
        },
        {
          locale: "en-PH",
          valid: [
            "+639275149120",
            "+639275142327",
            "+639003002023",
            "09275149116",
            "09194877624",
          ],
          invalid: [
            "12112-13-345",
            "12345678901",
            "sx23YW11cyBmZxxXJt123123",
            "010-38238383",
            "966684123123-2590",
          ],
        },
        {
          locale: "en-UG",
          valid: [
            "+256728590432",
            "+256733875610",
            "256728590234",
            "0773346543",
            "0700459022",
          ],
          invalid: [
            "999",
            "+254728590432",
            "+25489032",
            "123456789",
            "+254800723845",
          ],
        },
        {
          locale: "en-RW",
          valid: [
            "+250728590432",
            "+250733875610",
            "250738590234",
            "0753346543",
            "0780459022",
          ],
          invalid: [
            "999",
            "+254728590432",
            "+25089032",
            "123456789",
            "+250800723845",
          ],
        },
        {
          locale: "en-TZ",
          valid: [
            "+255728590432",
            "+255733875610",
            "255628590234",
            "0673346543",
            "0600459022",
          ],
          invalid: [
            "999",
            "+254728590432",
            "+25589032",
            "123456789",
            "+255800723845",
          ],
        },
        {
          locale: "en-MW",
          valid: [
            "+265994563785",
            "+265111785436",
            "+265318596857",
            "0320008744",
            "01256258",
            "0882541896",
            "+265984563214",
          ],
          invalid: [
            "58563",
            "+2658256258",
            "0896328741",
            "0708574896",
            "+26570857489635",
          ],
        },
        {
          locale: "es-PE",
          valid: [
            "+51912232764",
            "+51923464567",
            "+51968267382",
            "+51908792973",
            "974980472",
            "908792973",
            "+51974980472",
          ],
          invalid: [
            "999",
            "+51812232764",
            "+5181223276499",
            "+25589032",
            "123456789",
          ],
        },
        {
          locale: "fr-FR",
          valid: [
            "0612457898",
            "+33612457898",
            "33612457898",
            "0712457898",
            "+33712457898",
            "33712457898",
          ],
          invalid: [
            "061245789",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+34612457898",
            "+336124578980",
            "+3361245789",
          ],
        },
        {
          locale: "fr-BF",
          valid: [
            "+22661245789",
            "+22665903092",
            "+22672457898",
            "+22673572346",
            "061245789",
            "071245783",
          ],
          invalid: [
            "0612457892",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+22762457898",
            "+226724578980",
            "+22634523",
          ],
        },
        {
          locale: "fr-BJ",
          valid: [
            "+22920215789",
            "+22920293092",
            "+22921307898",
            "+22921736346",
            "+22922416346",
            "+22923836346",
          ],
          invalid: [
            "0612457892",
            "01122921737346",
            "+22762457898",
            "+226724578980",
            "+22634523",
          ],
        },
        {
          locale: "fr-CA",
          valid: ["19876543210", "8005552222", "+15673628910"],
          invalid: [
            "564785",
            "0123456789",
            "1437439210",
            "+10345672645",
            "11435213543",
          ],
        },
        {
          locale: "fr-CD",
          valid: [
            "+243818590432",
            "+243893875610",
            "243978590234",
            "0813346543",
            "0820459022",
            "+243902590221",
          ],
          invalid: [
            "243",
            "+254818590432",
            "+24389032",
            "123456789",
            "+243700723845",
          ],
        },
        {
          locale: "fr-GF",
          valid: [
            "0612457898",
            "+594612457898",
            "594612457898",
            "0712457898",
            "+594712457898",
            "594712457898",
          ],
          invalid: [
            "061245789",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+54612457898",
            "+5946124578980",
            "+59461245789",
          ],
        },
        {
          locale: "fr-GP",
          valid: [
            "0612457898",
            "+590612457898",
            "590612457898",
            "0712457898",
            "+590712457898",
            "590712457898",
          ],
          invalid: [
            "061245789",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+594612457898",
            "+5906124578980",
            "+59061245789",
          ],
        },
        {
          locale: "fr-MQ",
          valid: [
            "0612457898",
            "+596612457898",
            "596612457898",
            "0712457898",
            "+596712457898",
            "596712457898",
          ],
          invalid: [
            "061245789",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+594612457898",
            "+5966124578980",
            "+59661245789",
          ],
        },
        {
          locale: "fr-RE",
          valid: [
            "0612457898",
            "+262612457898",
            "262612457898",
            "0712457898",
            "+262712457898",
            "262712457898",
          ],
          invalid: [
            "061245789",
            "06124578980",
            "0112457898",
            "0212457898",
            "0312457898",
            "0412457898",
            "0512457898",
            "0812457898",
            "0912457898",
            "+264612457898",
            "+2626124578980",
            "+26261245789",
          ],
        },
        {
          locale: "fr-PF",
          valid: [
            "87123456",
            "88123456",
            "89123456",
            "+68987123456",
            "+68988123456",
            "+68989123456",
            "68987123456",
            "68988123456",
            "68989123456",
          ],
          invalid: [
            "7123456",
            "86123456",
            "87 12 34 56",
            "definitely not a number",
            "01+68988123456",
            "6898912345",
          ],
        },
        {
          locale: "fr-WF",
          valid: [
            "+681408500",
            "+681499387",
            "+681728590",
            "+681808542",
            "+681828540",
            "+681832014",
            "408500",
            "499387",
            "728590",
            "808542",
            "828540",
            "832014",
          ],
          invalid: [
            "+68189032",
            "123456789",
            "+681723845987",
            "022452389",
            "+681772345678",
            "+681700456794",
          ],
        },
        {
          locale: "ka-GE",
          valid: [
            "+995500011111",
            "+995515352134",
            "+995798526662",
            "798526662",
            "500011119",
            "798526662",
            "+995799766525",
          ],
          invalid: [
            "+99550001111",
            "+9957997665250",
            "+9959997665251",
            "+995780011111",
            "20000000000",
            "68129485729",
            "6589394827",
            "298RI89572",
          ],
        },
        {
          locale: "el-GR",
          valid: [
            "+306944848966",
            "306944848966",
            "06904567890",
            "6944848966",
            "6904567890",
            "6914567890",
            "6934567890",
            "6944567890",
            "6954567890",
            "6974567890",
            "6984567890",
            "6994567890",
            "6854567890",
            "6864567890",
            "6874567890",
            "6884567890",
            "6894567890",
          ],
          invalid: [
            "2102323234",
            "+302646041461",
            "120000000",
            "20000000000",
            "68129485729",
            "6589394827",
            "298RI89572",
            "6924567890",
            "6964567890",
            "6844567890",
            "690456789",
            "00690456789",
            "not a number",
          ],
        },
        {
          locale: "el-CY",
          valid: [
            "96546247",
            "96978927",
            "+35799837145",
            "+35799646792",
            "96056927",
            "99629593",
            "99849980",
            "3599701619",
            "+3599148725",
            "96537247",
            "3596676533",
            "+35795123455",
            "+35797012204",
            "35799123456",
            "+35794123456",
            "+35796123456",
          ],
          invalid: [
            "",
            "somechars",
            "9697892",
            "998499803",
            "33799837145",
            "+3799646792",
            "93056927",
          ],
        },
        {
          locale: "en-GB",
          valid: ["447789345856", "+447861235675", "07888814488"],
          invalid: [
            "67699567",
            "0773894868",
            "077389f8688",
            "+07888814488",
            "0152456999",
            "442073456754",
            "+443003434751",
            "05073456754",
            "08001123123",
            "07043425232",
            "01273884231",
            "03332654034",
          ],
        },
        {
          locale: "en-SG",
          valid: [
            "32891278",
            "87654321",
            "98765432",
            "+6587654321",
            "+6598765432",
            "+6565241234",
          ],
          invalid: [
            "332891231",
            "987654321",
            "876543219",
            "8765432",
            "9876543",
            "12345678",
            "+98765432",
            "+9876543212",
            "+15673628910",
            "19876543210",
            "8005552222",
          ],
        },
        {
          locale: "en-US",
          valid: [
            "19876543210",
            "8005552222",
            "+15673628910",
            "+1(567)3628910",
            "+1(567)362-8910",
            "+1(567) 362-8910",
            "1(567)362-8910",
            "1(567)362 8910",
            "223-456-7890",
          ],
          invalid: [
            "564785",
            "0123456789",
            "1437439210",
            "+10345672645",
            "11435213543",
            "1(067)362-8910",
            "1(167)362-8910",
            "+2(267)362-8910",
            "+3365520145",
          ],
        },
        {
          locale: "en-CA",
          valid: ["19876543210", "8005552222", "+15673628910"],
          invalid: [
            "564785",
            "0123456789",
            "1437439210",
            "+10345672645",
            "11435213543",
          ],
        },
        {
          locale: "en-ZM",
          valid: [
            "0956684590",
            "0966684590",
            "0976684590",
            "+260956684590",
            "+260966684590",
            "+260976684590",
            "260976684590",
            "+260779493521",
            "+260760010936",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
            "966684590",
            "760010936",
          ],
        },
        {
          locale: ["en-ZW"],
          valid: [
            "+263561890123",
            "+263715558041",
            "+263775551112",
            "+263775551695",
            "+263715556633",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+2631234567890",
            "+2641234567",
            "+263981234",
            "4736338855",
            "66338855",
          ],
        },
        {
          locale: ["en-NA"],
          valid: [
            "+26466189012",
            "+26461555804",
            "+26461434221",
            "+26487555169",
            "+26481555663",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+2641234567890",
            "+2641234567",
            "+2648143422",
            "+264981234",
            "4736338855",
            "66338855",
          ],
        },
        {
          locale: "ru-RU",
          valid: ["+79676338855", "79676338855", "89676338855", "9676338855"],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
          ],
        },
        {
          locale: "si-LK",
          valid: [
            "+94766661206",
            "94713114340",
            "0786642116",
            "078 7642116",
            "078-7642116",
            "0749994567",
          ],
          invalid: [
            "9912349956789",
            "12345",
            "1678123456",
            "0731234567",
            "0797878674",
          ],
        },
        {
          locale: "sr-RS",
          valid: [
            "0640133338",
            "063333133",
            "0668888878",
            "+381645678912",
            "+381611314000",
            "0655885010",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
          ],
        },
        {
          locale: "en-NZ",
          valid: ["+6427987035", "642240512347", "0293981646", "029968425"],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+642956696123566",
            "+02119620856",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
          ],
        },
        {
          locale: "en-MU",
          valid: ["+23012341234", "12341234", "012341234"],
          invalid: [
            "41234",
            "",
            "+230",
            "+2301",
            "+23012",
            "+230123",
            "+2301234",
            "+23012341",
            "+230123412",
            "+2301234123",
            "+230123412341",
            "+2301234123412",
            "+23012341234123",
          ],
        },
        {
          locale: ["nb-NO", "nn-NO"], // for multiple locales
          valid: [
            "+4796338855",
            "+4746338855",
            "4796338855",
            "4746338855",
            "46338855",
            "96338855",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+4676338855",
            "19676338855",
            "+4726338855",
            "4736338855",
            "66338855",
          ],
        },
        {
          locale: ["ne-NP"],
          valid: [
            "+9779817385479",
            "+9779717385478",
            "+9779862002615",
            "+9779853660020",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+97796123456789",
            "+9771234567",
            "+977981234",
            "4736338855",
            "66338855",
          ],
        },
        {
          locale: "vi-VN",
          valid: [
            "0336012403",
            "+84586012403",
            "84981577798",
            "0708001240",
            "84813601243",
            "0523803765",
            "0863803732",
            "0883805866",
            "0892405867",
            "+84888696413",
            "0878123456",
            "84781234567",
            "0553803765",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
            "260976684590",
            "01678912345",
            "+841698765432",
            "841626543219",
            "0533803765",
            "08712345678",
            "+0321234567",
          ],
        },
        {
          locale: "es-AR",
          valid: [
            "5491143214321",
            "+5491143214321",
            "+5492414321432",
            "5498418432143",
          ],
          invalid: [
            "1143214321",
            "91143214321",
            "+91143214321",
            "549841004321432",
            "549 11 43214321",
            "549111543214321",
            "5714003425432",
            "549114a214321",
            "54 9 11 4321-4321",
          ],
        },
        {
          locale: "es-CO",
          valid: [
            "+573003321235",
            "573003321235",
            "3003321235",
            "3213321235",
            "3103321235",
            "3243321235",
            "573011140876",
          ],
          invalid: [
            "1234",
            "+57443875615",
            "57309875615",
            "57109834567",
            "5792434567",
            "5702345689",
            "5714003425432",
            "5703013347567",
            "069834567",
            "969834567",
            "579871235",
            "574321235",
            "5784321235",
            "5784321235",
            "9821235",
            "0698345",
            "3321235",
          ],
        },
        {
          locale: "es-CL",
          valid: ["+56733875615", "56928590234", "0928590294", "0208590294"],
          invalid: [
            "1234",
            "+5633875615",
            "563875615",
            "56109834567",
            "56069834567",
          ],
        },
        {
          locale: "es-EC",
          valid: [
            "+593987654321",
            "593987654321",
            "0987654321",
            "027332615",
            "+59323456789",
          ],
          invalid: [
            "03321321",
            "+593387561",
            "59312345677",
            "02344635",
            "593123456789",
            "081234567",
            "+593912345678",
            "+593902345678",
            "+593287654321",
            "593287654321",
          ],
        },
        {
          locale: "es-CR",
          valid: [
            "+50688888888",
            "+50665408090",
            "+50640895069",
            "25789563",
            "85789563",
          ],
          invalid: [
            "+5081",
            "+5067777777",
            "+50188888888",
            "+50e987643254",
            "+506e4t4",
            "-50688888888",
            "50688888888",
            "12345678",
            "98765432",
            "01234567",
          ],
        },
        {
          locale: "es-CU",
          valid: ["+5351234567", "005353216547", "51234567", "53214567"],
          invalid: [
            "1234",
            "+5341234567",
            "0041234567",
            "41234567",
            "11234567",
            "21234567",
            "31234567",
            "60303456",
            "71234567",
            "81234567",
            "91234567",
            "+5343216547",
            "+5332165498",
            "+53121234567",
            "",
            "abc",
            "+535123457",
            "56043029304",
          ],
        },
        {
          locale: "es-DO",
          valid: [
            "+18096622563",
            "+18295614488",
            "+18495259567",
            "8492283478",
            "8092324576",
            "8292387713",
          ],
          invalid: [
            "+18091",
            "+1849777777",
            "-18296643245",
            "+18086643245",
            "+18396643245",
            "8196643245",
            "+38492283478",
            "6492283478",
            "8192283478",
          ],
        },
        {
          locale: "es-HN",
          valid: [
            "+50495551876",
            "+50488908787",
            "+50493456789",
            "+50489234567",
            "+50488987896",
            "+50497567389",
            "+50427367389",
            "+50422357389",
            "+50431257389",
            "+50430157389",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+34683456543",
            "65478932",
            "+50298787654",
            "+504989874",
          ],
        },
        {
          locale: "es-ES",
          valid: [
            "+34654789321",
            "654789321",
            "+34714789321",
            "714789321",
            "+34744789321",
            "744789321",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+3465478932",
            "65478932",
            "+346547893210",
            "6547893210",
            "+3470478932",
            "7047893210",
            "+34854789321",
            "7547893219",
          ],
        },
        {
          locale: "es-MX",
          valid: [
            "+52019654789321",
            "+52199654789321",
            "+5201965478932",
            "+5219654789321",
            "52019654789321",
            "52199654789321",
            "5201965478932",
            "5219654789321",
            "87654789321",
            "8654789321",
            "0187654789321",
            "18654789321",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+3465478932",
            "65478932",
            "+346547893210",
            "+34704789321",
            "704789321",
            "+34754789321",
          ],
        },
        {
          locale: "es-NI",
          valid: [
            "+5051234567",
            "+50512345678",
            "5051234567",
            "50512345678",
            "+50555555555",
          ],
          invalid: [
            "1234",
            "",
            "1234567",
            "12345678",
            "+12345678",
            "+505123456789",
            "+50612345678",
            "+50712345678",
            "-50512345678",
          ],
        },
        {
          locale: "es-PA",
          valid: ["+5076784565", "+5074321557", "5073331112", "+50723431212"],
          invalid: [
            "+50755555",
            "+207123456",
            "2001236542",
            "+507987643254",
            "+507jjjghtf",
          ],
        },
        {
          locale: "es-PY",
          valid: [
            "+595991372649",
            "+595992847352",
            "+595993847593",
            "+595994857473",
            "+595995348532",
            "+595996435231",
            "+595981847362",
            "+595982435452",
            "+595983948502",
            "+595984342351",
            "+595985403481",
            "+595986384012",
            "+595971435231",
            "+595972103924",
            "+595973438542",
            "+595974425864",
            "+595975425843",
            "+595976342546",
            "+595961435234",
            "+595963425043",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "65478932",
            "+59599384712",
            "+5959938471234",
            "+595547893218",
            "+591993546843",
          ],
        },
        {
          locale: "es-SV",
          valid: [
            "62136634",
            "50361366631",
            "+50361366634",
            "+50361367217",
            "+50361367460",
            "+50371367632",
            "+50371367767",
            "+50371368314",
          ],
          invalid: [
            "+5032136663",
            "21346663",
            "+50321366663",
            "12345",
            "El salvador",
            "this should fail",
            "+5032222",
            "+503 1111 1111",
            "00 +503 1234 5678",
          ],
        },
        {
          locale: "es-UY",
          valid: ["+59899123456", "099123456", "+59894654321", "091111111"],
          invalid: [
            "54321",
            "montevideo",
            "",
            "+598099123456",
            "090883338",
            "099 999 999",
          ],
        },
        {
          locale: "es-VE",
          valid: ["+582125457765", "+582125458053", "+584125458053"],
          invalid: ["+585129934395", "+58212993439", ""],
        },
        {
          locale: "et-EE",
          valid: [
            "+372 512 34 567",
            "372 512 34 567",
            "+37251234567",
            "51234567",
            "81234567",
            "+372842345678",
          ],
          invalid: [
            "12345",
            "",
            "NotANumber",
            "+333 51234567",
            "61234567",
            "+51234567",
            "+372 539 57 4",
            "+372 900 1234",
            "12345678",
          ],
        },
        {
          locale: "pl-PL",
          valid: [
            "+48512689767",
            "+48 56 376 87 47",
            "56 566 78 46",
            "657562855",
            "+48657562855",
            "+48 887472765",
            "+48 56 6572724",
            "+48 67 621 5461",
            "48 67 621 5461",
            "+48 45 621 5461",
          ],
          invalid: [
            "+48  67 621 5461",
            "+55657562855",
            "3454535",
            "teststring",
            "",
            "1800-88-8687",
            "+6019-5830837",
            "357562855",
            "+48 44 621 5461",
          ],
        },
        {
          locale: "fa-IR",
          valid: [
            "+989123456789",
            "989223456789",
            "09323456789",
            "09021456789",
            "+98-990-345-6789",
            "+98 938 345 6789",
            "0938 345 6789",
          ],
          invalid: [
            "",
            "+989623456789",
            "+981123456789",
            "01234567890",
            "09423456789",
            "09823456789",
            "9123456789",
            "091234567890",
            "0912345678",
            "+98 912 3456 6789",
            "0912 345 678",
          ],
        },
        {
          locale: "fi-FI",
          valid: [
            "+358505557171",
            "0455571",
            "0505557171",
            "358505557171",
            "04412345",
            "0457 123 45 67",
            "+358457 123 45 67",
            "+358 50 555 7171",
            "0501234",
            "+358501234",
            "050 1234",
          ],
          invalid: [
            "12345",
            "",
            "045557",
            "045555717112312332423423421",
            "Vml2YW11cyBmZXJtZtesting123",
            "010-38238383",
            "+3-585-0555-7171",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
            "044123",
            "019123456789012345678901",
          ],
        },
        {
          locale: "fj-FJ",
          valid: [
            "+6799898679",
            "6793788679",
            "+679 989 8679",
            "679 989 8679",
            "679 3456799",
            "679908 8909",
          ],
          invalid: [
            "12345",
            "",
            "04555792",
            "902w99900030900000000099",
            "8uiuiuhhyy&GUU88d",
            "010-38238383",
            "19676338855",
            "679 9 89 8679",
            "6793 45679",
          ],
        },
        {
          locale: "ms-MY",
          valid: [
            "+60128228789",
            "+60195830837",
            "+6019-5830837",
            "+6019-5830837",
            "+6010-4357675",
            "+60172012370",
            "0128737867",
            "0172012370",
            "01468987837",
            "01112347345",
            "016-2838768",
            "016 2838768",
          ],
          invalid: [
            "12345",
            "601238788657",
            "088387675",
            "16-2838768",
            "032551433",
            "6088-387888",
            "088-261987",
            "1800-88-8687",
            "088-320000",
            "+01112353576",
            "+0111419752",
          ],
        },
        {
          locale: "fr-CM",
          valid: [
            "+237677936141",
            "237623456789",
            "+237698124842",
            "237693029202",
          ],
          invalid: [
            "NotANumber",
            "+(703)-572-2920",
            "+237 623 45 67 890",
            "+2379981247429",
          ],
        },
        {
          locale: "ko-KR",
          valid: [
            "+82-010-1234-5678",
            "+82-10-1234-5678",
            "82-010-1234-5678",
            "82-10-1234-5678",
            "+82 10 1234 5678",
            "010-123-5678",
            "10-1234-5678",
            "+82 10 1234 5678",
            "011 1234 5678",
            "+820112345678",
            "01012345678",
            "+82 016 1234 5678",
            "82 19 1234 5678",
            "+82 010 12345678",
          ],
          invalid: [
            "abcdefghi",
            "+82 10 1234 567",
            "+82 10o 1234 1234",
            "+82 101 1234 5678",
            "+82 10 12 5678",
            "+011 7766 1234",
            "011_7766_1234",
            "+820 11 7766 1234",
          ],
        },
        {
          locale: "ky-KG",
          valid: [
            "+996553033300",
            "+996 222 123456",
            "+996 500 987654",
            "+996 555 111222",
            "+996 700 333444",
            "+996 770 555666",
            "+996 880 777888",
            "+996 990 999000",
            "+996 995 555666",
            "+996 996 555666",
            "+996 997 555666",
            "+996 998 555666",
          ],
          invalid: [
            "+996 201 123456",
            "+996 312 123456",
            "+996 3960 12345",
            "+996 3961 12345",
            "+996 3962 12345",
            "+996 3963 12345",
            "+996 3964 12345",
            "+996 3965 12345",
            "+996 3966 12345",
            "+996 3967 12345",
            "+996 3968 12345",
            "+996 511 123456",
            "+996 522 123456",
            "+996 561 123456",
            "+996 571 123456",
            "+996 624 123456",
            "+996 623 123456",
            "+996 622 123456",
            "+996 609 123456",
            "+996 100 12345",
            "+996 100 1234567",
            "996 100 123456",
            "0 100 123456",
            "0 100 123abc",
          ],
        },
        {
          locale: "ja-JP",
          valid: [
            "09012345678",
            "08012345678",
            "07012345678",
            "06012345678",
            "090 1234 5678",
            "+8190-1234-5678",
            "+81 (0)90-1234-5678",
            "+819012345678",
            "+81-(0)90-1234-5678",
            "+81 90 1234 5678",
          ],
          invalid: [
            "12345",
            "",
            "045555717112312332423423421",
            "Vml2YW11cyBmZXJtZtesting123",
            "+3-585-0555-7171",
            "0 1234 5689",
            "16 1234 5689",
            "03_1234_5689",
            "0312345678",
            "0721234567",
            "06 1234 5678",
            "072 123 4567",
            "0729 12 3456",
            "07296 1 2345",
            "072961 2345",
            "03-1234-5678",
            "+81312345678",
            "+816-1234-5678",
            "+81 090 1234 5678",
            "+8109012345678",
            "+81-090-1234-5678",
            "90 1234 5678",
          ],
        },
        {
          locale: "ir-IR",
          valid: [
            "09023818688",
            "09123809999",
            "+989023818688",
            "+989103923523",
          ],
          invalid: [
            "19023818688",
            "323254",
            "+903232323257",
            "++3567868",
            "0902381888832",
          ],
        },
        {
          locale: "it-IT",
          valid: [
            "370 3175423",
            "333202925",
            "+39 310 7688449",
            "+39 3339847632",
          ],
          invalid: ["011 7387545", "12345", "+45 345 6782395"],
        },
        {
          locale: "fr-BE",
          valid: [
            "0470123456",
            "+32470123456",
            "32470123456",
            "0421234567",
            "+32421234567",
            "32421234567",
          ],
          invalid: [
            "12345",
            "+3212345",
            "3212345",
            "04701234567",
            "+3204701234567",
            "3204701234567",
            "0212345678",
            "+320212345678",
            "320212345678",
            "021234567",
            "+3221234567",
            "3221234567",
          ],
        },
        {
          locale: "nl-BE",
          valid: [
            "0470123456",
            "+32470123456",
            "32470123456",
            "0421234567",
            "+32421234567",
            "32421234567",
          ],
          invalid: [
            "12345",
            "+3212345",
            "3212345",
            "04701234567",
            "+3204701234567",
            "3204701234567",
            "0212345678",
            "+320212345678",
            "320212345678",
            "021234567",
            "+3221234567",
            "3221234567",
          ],
        },
        {
          locale: "nl-NL",
          valid: [
            "0670123456",
            "0612345678",
            "31612345678",
            "31670123456",
            "+31612345678",
            "+31670123456",
            "+31(0)612345678",
            "0031612345678",
            "0031(0)612345678",
          ],
          invalid: [
            "12345",
            "+3112345",
            "3112345",
            "06701234567",
            "012345678",
            "+3104701234567",
            "3104701234567",
            "0212345678",
            "021234567",
            "+3121234567",
            "3121234567",
            "+310212345678",
            "310212345678",
          ],
        },
        {
          locale: "nl-AW",
          valid: [
            "2975612345",
            "2976412345",
            "+2975612345",
            "+2975912345",
            "+2976412345",
            "+2977312345",
            "+2977412345",
            "+2979912345",
          ],
          invalid: [
            "12345",
            "+2972345",
            "2972345",
            "06701234567",
            "012345678",
            "+2974701234567",
            "2974701234567",
            "0297345678",
            "029734567",
            "+2971234567",
            "2971234567",
            "+297212345678",
            "297212345678",
            "number",
          ],
        },
        {
          locale: "ro-MD",
          valid: [
            "+37360375781",
            "+37361945673",
            "+37362387563",
            "+37368447788",
            "+37369000101",
            "+37367568910",
            "+37376758294",
            "+37378457892",
            "+37379067436",
            "37362387563",
            "37368447788",
            "37369000101",
            "37367568910",
          ],
          invalid: [
            "",
            "+37363373381",
            "+37364310581",
            "+37365578199",
            "+37371088636",
            "Vml2YW11cyBmZXJtZtesting123",
            "123456",
            "740123456",
            "+40640123456",
            "+40210123456",
          ],
        },
        {
          locale: "ro-RO",
          valid: [
            "+40740123456",
            "+40 740123456",
            "+40740 123 456",
            "+40740.123.456",
            "+40740-123-456",
            "40740123456",
            "40 740123456",
            "40740 123 456",
            "40740.123.456",
            "40740-123-456",
            "0740123456",
            "0740/123456",
            "0740 123 456",
            "0740.123.456",
            "0740-123-456",
          ],
          invalid: [
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "123456",
            "740123456",
            "+40640123456",
            "+40210123456",
            "+0765351689",
            "+0711419752",
          ],
        },
        {
          locale: "id-ID",
          valid: [
            "0811 778 998",
            "0811 7785 9983",
            "0812 7784 9984",
            "0813 7782 9982",
            "0821 1234 1234",
            "0822 1234 1234",
            "0823 1234 1234",
            "0852 1234 6764",
            "0853 1234 6764",
            "0851 1234 6764",
            "0814 7782 9982",
            "0815 7782 9982",
            "0816 7782 9982",
            "0855 7782 9982",
            "0856 7782 9982",
            "0857 7782 9982",
            "0858 7782 9982",
            "0817 7785 9983",
            "0818 7784 9984",
            "0819 7782 9982",
            "0859 1234 1234",
            "0877 1234 1234",
            "0878 1234 1234",
            "0895 7785 9983",
            "0896 7784 9984",
            "0897 7782 9982",
            "0898 1234 1234",
            "0899 1234 1234",
            "0881 7785 9983",
            "0882 7784 9984",
            "0883 7782 9982",
            "0884 1234 1234",
            "0886 1234 1234",
            "0887 1234 1234",
            "0888 7785 9983",
            "0889 7784 9984",
            "0828 7784 9984",
            "0838 7784 9984",
            "0831 7784 9984",
            "0832 7784 9984",
            "0833 7784 9984",
            "089931236181900",
            "62811 778 998",
            "62811778998",
            "628993123618190",
            "62898 740123456",
            "62899 7401 2346",
            "+62811 778 998",
            "+62811778998",
            "+62812 9650 3508",
            "08197231819",
            "085361008008",
            "+62811787391",
          ],
          invalid: [
            "0899312361819001",
            "0217123456",
            "622178878890",
            "6221 740123456",
            "0341 8123456",
            "0778 89800910",
            "0741 123456",
            "+6221740123456",
            "+65740 123 456",
            "",
            "ASDFGJKLmZXJtZtesting123",
            "123456",
            "740123456",
            "+65640123456",
            "+64210123456",
          ],
        },
        {
          locale: "lt-LT",
          valid: ["+37051234567", "851234567"],
          invalid: [
            "+65740 123 456",
            "",
            "ASDFGJKLmZXJtZtesting123",
            "123456",
            "740123456",
            "+65640123456",
            "+64210123456",
          ],
        },
        {
          locale: "uk-UA",
          valid: [
            "+380501234567",
            "+380631234567",
            "+380661234567",
            "+380671234567",
            "+380681234567",
            "+380731234567",
            "+380751234567",
            "+380771234567",
            "+380911234567",
            "+380921234567",
            "+380931234567",
            "+380941234567",
            "+380951234567",
            "+380961234567",
            "+380971234567",
            "+380981234567",
            "+380991234567",
            "380501234567",
            "380631234567",
            "380661234567",
            "380671234567",
            "380681234567",
            "380731234567",
            "380751234567",
            "380771234567",
            "380911234567",
            "380921234567",
            "380931234567",
            "380941234567",
            "380951234567",
            "380961234567",
            "380971234567",
            "380981234567",
            "380991234567",
            "0501234567",
            "0631234567",
            "0661234567",
            "0671234567",
            "0681234567",
            "0731234567",
            "0751234567",
            "0771234567",
            "0911234567",
            "0921234567",
            "0931234567",
            "0941234567",
            "0951234567",
            "0961234567",
            "0971234567",
            "0981234567",
            "0991234567",
          ],
          invalid: [
            "+30982345679",
            "+380321234567",
            "+380441234567",
            "982345679",
            "80982345679",
            "+380 98 234 5679",
            "+380-98-234-5679",
            "+380 (98) 234-56-79",
            "",
            "ASDFGJKLmZXJtZtesting123",
            "123456",
            "740123456",
          ],
        },
        {
          locale: "uz-UZ",
          valid: [
            "+998664835244",
            "998664835244",
            "664835244",
            "+998957124555",
            "998957124555",
            "957124555",
          ],
          invalid: [
            "+998644835244",
            "998644835244",
            "644835244",
            "+99664835244",
            "ASDFGJKLmZXJtZtesting123",
            "123456789",
            "870123456",
            "",
            "+998",
            "998",
          ],
        },
        {
          locale: "da-DK",
          valid: [
            "12345678",
            "12 34 56 78",
            "45 12345678",
            "4512345678",
            "45 12 34 56 78",
            "+45 12 34 56 78",
          ],
          invalid: [
            "",
            "+45010203",
            "ASDFGJKLmZXJtZtesting123",
            "123456",
            "12 34 56",
            "123 123 12",
          ],
        },
        {
          locale: "sv-SE",
          valid: [
            "+46701234567",
            "46701234567",
            "0721234567",
            "073-1234567",
            "0761-234567",
            "079-123 45 67",
          ],
          invalid: [
            "12345",
            "+4670123456",
            "+46301234567",
            "+0731234567",
            "0731234 56",
            "+7312345678",
            "",
          ],
        },
        {
          locale: "fo-FO",
          valid: [
            "123456",
            "12 34 56",
            "298 123456",
            "298123456",
            "298 12 34 56",
            "+298 12 34 56",
          ],
          invalid: [
            "",
            "+4501020304",
            "ASDFGJKLmZXJtZtesting123",
            "12345678",
            "12 34 56 78",
          ],
        },
        {
          locale: "kl-GL",
          valid: [
            "123456",
            "12 34 56",
            "299 123456",
            "299123456",
            "299 12 34 56",
            "+299 12 34 56",
          ],
          invalid: [
            "",
            "+4501020304",
            "ASDFGJKLmZXJtZtesting123",
            "12345678",
            "12 34 56 78",
          ],
        },
        {
          locale: "kk-KZ",
          valid: ["+77254716212", "77254716212", "87254716212", "7254716212"],
          invalid: [
            "12345",
            "",
            "ASDFGJKLmZXJtZtesting123",
            "010-38238383",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
          ],
        },
        {
          locale: "be-BY",
          valid: [
            "+375241234567",
            "+375251234567",
            "+375291234567",
            "+375331234567",
            "+375441234567",
            "375331234567",
          ],
          invalid: [
            "12345",
            "",
            "ASDFGJKLmZXJtZtesting123",
            "010-38238383",
            "+9676338855",
            "19676338855",
            "6676338855",
            "+99676338855",
          ],
        },
        {
          locale: "th-TH",
          valid: ["0912345678", "+66912345678", "66912345678"],
          invalid: ["99123456789", "12345", "67812345623", "081234567891"],
        },
        {
          locale: "tk-TM",
          valid: [
            "+99312495154",
            "99312130136",
            "+99312918407",
            "99312183399",
            "812391717",
          ],
          invalid: ["12345", "+99412495154", "99412495154", "998900066506"],
        },
        {
          locale: "en-SL",
          valid: ["+23274560591", "23274560591", "074560591"],
          invalid: [
            "0745605912",
            "12345",
            "232745605917",
            "0797878674",
            "23274560591 ",
          ],
        },
        {
          locale: "en-BW",
          valid: [
            "+26772868545",
            "+26776368790",
            "+26774560512",
            "26774560591",
            "26778560512",
            "74560512",
            "76710284",
          ],
          invalid: [
            "0799375902",
            "12345",
            "+2670745605448",
            "2670745605482",
            "+26779685451",
            "+26770685451",
            "267074560",
            "2670ab5608",
            "+267074560",
            "70560512",
            "79710284",
          ],
        },
        {
          locale: "az-AZ",
          valid: [
            "+994707007070",
            "0707007070",
            "+994502111111",
            "0505436743",
            "0554328772",
            "0104328772",
            "0993301022",
            "+994776007139",
            "+994106007139",
          ],
          invalid: [
            "wrong-number",
            "",
            "994707007070",
            "++9945005050",
            "556007070",
            "1234566",
            "+994778008080a",
          ],
        },
        {
          locale: "de-LU",
          valid: ["601123456", "+352601123456"],
          invalid: [
            "NaN",
            "791234",
            "+352791234",
            "26791234",
            "+35226791234",
            "+112039812",
            "+352703123456",
            "1234",
          ],
        },
        {
          locale: "it-SM",
          valid: [
            "612345",
            "05496123456",
            "+37861234567",
            "+390549612345678",
            "+37805496123456789",
          ],
          invalid: [
            "61234567890",
            "6123",
            "1234567",
            "+49123456",
            "NotANumber",
          ],
        },
        {
          locale: "so-SO",
          valid: [
            "+252601234567",
            "+252650101010",
            "+252794567120",
            "252650647388",
            "252751234567",
            "0601234567",
            "0609876543",
          ],
          invalid: [
            "",
            "not a number",
            "+2526012345678",
            "25260123456",
            "+252705555555",
            "+0601234567",
            "06945454545",
          ],
        },
        {
          locale: "sq-AL",
          valid: [
            "0621234567",
            "0661234567",
            "0671234567",
            "0681234567",
            "0691234567",
            "+355621234567",
            "+355651234567",
            "+355661234567",
            "+355671234567",
            "+355681234567",
            "+355691234567",
          ],
          invalid: [
            "67123456",
            "06712345",
            "067123456",
            "06712345678",
            "0571234567",
            "+3556712345",
            "+35565123456",
            "+35157123456",
            "NotANumber",
          ],
        },
        {
          locale: "ca-AD",
          valid: ["+376312345", "312345"],
          invalid: ["31234", "31234567", "512345", "NotANumber"],
        },
        {
          locale: "pt-AO",
          valid: ["+244911123432", "911123432", "244911123432"],
          invalid: [
            "+2449111234321",
            "+244811123432",
            "31234",
            "31234567",
            "512345",
            "NotANumber",
          ],
        },
        {
          locale: "lv-LV",
          valid: ["+37121234567", "37121234567"],
          invalid: [
            "+37201234567",
            "+3754321",
            "3712123456",
            "+371212345678",
            "NotANumber",
          ],
        },
        {
          locale: "mg-MG",
          valid: [
            "+261204269174",
            "261204269174",
            "0204269174",
            "0209269174",
            "0374269174",
            "4269174",
          ],
          invalid: [
            "0261204269174",
            "+261 20 4 269174",
            "+261 20 4269174",
            "020 4269174",
            "204269174",
            "0404269174",
            "NotANumber",
          ],
        },
        {
          locale: "mn-MN",
          valid: [
            "+97699112222",
            "97696112222",
            "97695112222",
            "01197691112222",
            "0097688112222",
            "+97677112222",
            "+97694112222",
            "+97681112222",
          ],
          invalid: [
            "+97888112222",
            "+97977112222",
            "+97094112222",
            "+97281112222",
            "02297681112222",
          ],
        },
        {
          locale: "my-MM",
          valid: [
            "+959750202595",
            "09750202595",
            "9750202595",
            "+959260000966",
            "09256000323",
            "09276000323",
            "09426000323",
            "09456000323",
            "09761234567",
            "09791234567",
            "09961234567",
            "09771234567",
            "09660000234",
          ],
          invalid: [
            "59750202595",
            "+9597502025",
            "08943234524",
            "09950000966",
            "959240000966",
            "09246000323",
            "09466000323",
            "09951234567",
            "09801234567",
            "09650000234",
          ],
        },
        {
          locale: "en-PG",
          valid: [
            "+67570123456",
            "67570123456",
            "+67571123456",
            "+67572123456",
            "+67573123456",
            "+67574123456",
            "+67575123456",
            "+67576123456",
            "+67577123456",
            "+67578123456",
            "+67579123456",
            "+67581123456",
            "+67588123456",
          ],
          invalid: [
            "",
            "not a number",
            "12345",
            "+675123456789",
            "+67580123456",
            "+67569123456",
            "+67582123456",
            "+6757012345",
          ],
        },
        {
          locale: "en-AG",
          valid: [
            "12687151234",
            "+12687151234",
            "+12684641234",
            "12684641234",
            "+12687211234",
            "+12687302468",
            "+12687642456",
            "+12687763333",
          ],
          invalid: [
            "2687151234",
            "+12687773333",
            "+126846412333",
            "+12684641",
            "+12687123456",
            "+12687633456",
          ],
        },
        {
          locale: "en-AI",
          valid: [
            "+12642351234",
            "12642351234",
            "+12644612222",
            "+12645366326",
            "+12645376326",
            "+12647246326",
            "+12647726326",
          ],
          invalid: [
            "",
            "not a number",
            "+22642351234",
            "+12902351234",
            "+12642331234",
            "+1264235",
            "22642353456",
            "+12352643456",
          ],
        },
        {
          locale: "en-KN",
          valid: [
            "+18694699040",
            "18694699040",
            "+18697652917",
            "18697652917",
            "18694658472",
            "+18696622969",
            "+18694882224",
          ],
          invalid: [
            "",
            "+18694238545",
            "+1 8694882224",
            "8694658472",
            "+186946990",
            "+1869469904",
            "1869469904",
          ],
        },
        {
          locale: "en-PK",
          valid: [
            "+923412877421",
            "+923001234567",
            "00923001234567",
            "923001234567",
            "03001234567",
          ],
          invalid: [
            "+3001234567",
            "+933001234567",
            "+924001234567",
            "+92300123456720",
            "030012345672",
            "30012345673",
            "0030012345673",
            "3001234567",
          ],
        },
        {
          locale: ["tg-TJ"],
          valid: [
            "+992553388551",
            "+992553322551",
            "992553388551",
            "992553322551",
          ],
          invalid: [
            "12345",
            "",
            "Vml2YW11cyBmZXJtZtesting123",
            "+995563388559",
            "+9955633559",
            "19676338855",
            "+992263388505",
            "9923633885",
            "99255363885",
            "66338855",
          ],
        },
        {
          locale: "dv-MV",
          valid: [
            "+9609112345",
            "+9609958973",
            "+9607258963",
            "+9607958463",
            "9609112345",
            "9609958973",
            "9607212963",
            "9607986963",
            "9112345",
            "9958973",
            "7258963",
            "7958963",
          ],
          invalid: [
            "+96059234567",
            "+96045789",
            "7812463784",
            "NotANumber",
            "+9607112345",
            "+9609012345",
            "+609012345",
            "+96071123456",
            "3412345",
            "9603412345",
          ],
        },
        {
          locale: "ar-YE",
          valid: ["737198225", "733111355", "+967700990270"],
          invalid: [
            "+5032136663",
            "21346663",
            "+50321366663",
            "12345",
            "Yemen",
            "this should fail",
            "+5032222",
            "+503 1111 1111",
            "00 +503 1234 5678",
          ],
        },
        {
          locale: "ar-EH",
          valid: [
            "+212-5288-12312",
            "+212-5288 12312",
            "+212 5288 12312",
            "212528912312",
            "+212528912312",
            "+212528812312",
          ],
          invalid: [
            "212528812312123",
            "+212-5290-12312",
            "++212528812312",
            "12345",
            "Wester Sahara",
            "this should fail",
            "212  5288---12312",
            "+503 1111 1111",
            "00 +503 1234 5678",
          ],
        },
        {
          locale: "fa-AF",
          valid: ["0511231231", "+93511231231", "+93281234567"],
          invalid: [
            "212528812312123",
            "+212-5290-12312",
            "++212528812312",
            "12345",
            "Afghanistan",
            "this should fail",
            "212  5288---12312",
            "+503 1111 1111",
            "00 +503 1234 5678",
          ],
        },
        {
          locale: "en-SS",
          valid: [
            "+211928530422",
            "+211913384561",
            "+211972879174",
            "+211952379334",
            "0923346543",
            "0950459022",
            "0970934567",
            "211979841238",
            "211929843238",
            "211959840238",
          ],
          invalid: [
            "911",
            "+211999",
            "123456789909",
            "South Sudan",
            "21195 840 238",
            "+211981234567",
            "+211931234567",
            "+211901234567",
            "+211991234567",
          ],
        },
        {
          locale: "es-GT",
          valid: [
            "+50221234567",
            "+50277654321",
            "50226753421",
            "50272332468",
            "50278984455",
            "+50273472492",
            "71234567",
            "21132398",
          ],
          invalid: [
            "44",
            "+5022712345678",
            "1234567899",
            "502712345678",
            "This should fail",
            "5021931234567",
            "+50281234567",
          ],
        },
        {
          locale: "mk-MK",
          valid: [
            "+38923234567",
            "38931234567",
            "022123456",
            "22234567",
            "71234567",
            "31234567",
            "+38923091500",
            "80091234",
            "81123456",
            "54123456",
          ],
          invalid: [
            "38912345678",
            "+389123456789",
            "21234567",
            "123456789",
            "+3891234567",
            "700012345",
            "510123456",
            "This should fail",
            "+389123456",
            "389123456",
            "80912345",
          ],
        },
      ];

      let allValid = [];

      fixtures.forEach((fixture) => {
        // to be used later on for validating 'any' locale
        if (fixture.valid) allValid = allValid.concat(fixture.valid);

        if (Array.isArray(fixture.locale)) {
          test({
            validator: "isMobilePhone",
            valid: fixture.valid,
            invalid: fixture.invalid,
            args: [fixture.locale],
          });
        } else {
          test({
            validator: "isMobilePhone",
            valid: fixture.valid,
            invalid: fixture.invalid,
            args: [fixture.locale],
          });
        }
      });

      test({
        validator: "isMobilePhone",
        valid: allValid.slice(0, 20),
        invalid: [
          "",
          "asdf",
          "1",
          "ASDFGJKLmZXJtZtesting123",
          "Vml2YW11cyBmZXJtZtesting123",
        ],
        args: ["any"],
      });

      // strict mode
      test({
        validator: "isMobilePhone",
        valid: ["+254728530234", "+299 12 34 56", "+94766660206"],
        invalid: [
          "254728530234",
          "0728530234",
          "+728530234",
          "766667206",
          "0766670206",
        ],
        args: ["any", { strictMode: true }],
      });

      // falsey locale defaults to 'any'
      test({
        validator: "isMobilePhone",
        valid: allValid.slice(0, 20),
        invalid: [
          "",
          "asdf",
          "1",
          "ASDFGJKLmZXJtZtesting123",
          "Vml2YW11cyBmZXJtZtesting123",
        ],
        args: [],
      });
    });

    // de-CH, fr-CH, it-CH
    test({
      validator: "isMobilePhone",
      valid: [
        "+41751112233",
        "+41761112233",
        "+41771112233",
        "+41781112233",
        "+41791112233",
        "+411122112211",
      ],
      invalid: ["+41041112233"],
      args: [],
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isMobilePhone",
        args: [{ locale: ["is-NOT"] }],
        error: ["+123456789", "012345"],
      });
    });

    it("should validate currency", () => {
      // -$##,###.## (en-US, en-CA, en-AU, en-NZ, en-HK)
      test({
        validator: "isCurrency",
        valid: [
          "-$10,123.45",
          "$10,123.45",
          "$10123.45",
          "10,123.45",
          "10123.45",
          "10,123",
          "1,123,456",
          "1123456",
          "1.39",
          ".03",
          "0.10",
          "$0.10",
          "-$0.01",
          "-$.99",
          "$100,234,567.89",
          "$10,123",
          "10,123",
          "-10123",
        ],
        invalid: [
          "1.234",
          "$1.1",
          "$ 32.50",
          "500$",
          ".0001",
          "$.001",
          "$0.001",
          "12,34.56",
          "123456,123,123456",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
        ],
      });

      // -$##,###.## (en-US, en-CA, en-AU, en-NZ, en-HK)
      test({
        validator: "isCurrency",
        args: [
          {
            allow_decimal: false,
          },
        ],
        valid: [
          "-$10,123",
          "$10,123",
          "$10123",
          "10,123",
          "10123",
          "10,123",
          "1,123,456",
          "1123456",
          "1",
          "0",
          "$0",
          "-$0",
          "$100,234,567",
          "$10,123",
          "10,123",
          "-10123",
        ],
        invalid: [
          "-$10,123.45",
          "$10,123.45",
          "$10123.45",
          "10,123.45",
          "10123.45",
          "1.39",
          ".03",
          "0.10",
          "$0.10",
          "-$0.01",
          "-$.99",
          "$100,234,567.89",
          "1.234",
          "$1.1",
          "$ 32.50",
          ".0001",
          "$.001",
          "$0.001",
          "12,34.56",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
        ],
      });

      // -$##,###.## (en-US, en-CA, en-AU, en-NZ, en-HK)
      test({
        validator: "isCurrency",
        args: [
          {
            require_decimal: true,
          },
        ],
        valid: [
          "-$10,123.45",
          "$10,123.45",
          "$10123.45",
          "10,123.45",
          "10123.45",
          "10,123.00",
          "1.39",
          ".03",
          "0.10",
          "$0.10",
          "-$0.01",
          "-$.99",
          "$100,234,567.89",
        ],
        invalid: [
          "$10,123",
          "10,123",
          "-10123",
          "1,123,456",
          "1123456",
          "1.234",
          "$1.1",
          "$ 32.50",
          "500$",
          ".0001",
          "$.001",
          "$0.001",
          "12,34.56",
          "123456,123,123456",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
        ],
      });

      // -$##,###.## (en-US, en-CA, en-AU, en-NZ, en-HK)
      test({
        validator: "isCurrency",
        args: [
          {
            digits_after_decimal: [1, 3],
          },
        ],
        valid: [
          "-$10,123.4",
          "$10,123.454",
          "$10123.452",
          "10,123.453",
          "10123.450",
          "10,123",
          "1,123,456",
          "1123456",
          "1.3",
          ".030",
          "0.100",
          "$0.1",
          "-$0.0",
          "-$.9",
          "$100,234,567.893",
          "$10,123",
          "10,123.123",
          "-10123.1",
        ],
        invalid: [
          "1.23",
          "$1.13322",
          "$ 32.50",
          "500$",
          ".0001",
          "$.01",
          "$0.01",
          "12,34.56",
          "123456,123,123456",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
        ],
      });

      // -$##,###.## with $ required (en-US, en-CA, en-AU, en-NZ, en-HK)
      test({
        validator: "isCurrency",
        args: [
          {
            require_symbol: true,
          },
        ],
        valid: [
          "-$10,123.45",
          "$10,123.45",
          "$10123.45",
          "$10,123.45",
          "$10,123",
          "$1,123,456",
          "$1123456",
          "$1.39",
          "$.03",
          "$0.10",
          "$0.10",
          "-$0.01",
          "-$.99",
          "$100,234,567.89",
          "$10,123",
          "-$10123",
        ],
        invalid: [
          "1.234",
          "$1.234",
          "1.1",
          "$1.1",
          "$ 32.50",
          " 32.50",
          "500",
          "10,123,456",
          ".0001",
          "$.001",
          "$0.001",
          "1,234.56",
          "123456,123,123456",
          "$123456,123,123456",
          "123.4",
          "$123.4",
          ",123",
          "$,123",
          "$-,123",
          "$",
          ".",
          "$.",
          ",",
          "$,",
          "00",
          "$00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "$ ",
          "- $",
        ],
      });

      // \u00a5-##,###.## (zh-CN)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "\u00a5",
            negative_sign_before_digits: true,
          },
        ],
        valid: [
          "123,456.78",
          "-123,456.78",
          "\u00a56,954,231",
          "\u00a5-6,954,231",
          "\u00a510.03",
          "\u00a5-10.03",
          "10.03",
          "1.39",
          ".03",
          "0.10",
          "\u00a5-10567.01",
          "\u00a50.01",
          "\u00a51,234,567.89",
          "\u00a510,123",
          "\u00a5-10,123",
          "\u00a5-10,123.45",
          "10,123",
          "10123",
          "\u00a5-100",
        ],
        invalid: [
          "1.234",
          "\u00a51.1",
          "5,00",
          ".0001",
          "\u00a5.001",
          "\u00a50.001",
          "12,34.56",
          "123456,123,123456",
          "123 456",
          ",123",
          "\u00a5-,123",
          "",
          " ",
          "\u00a5",
          "\u00a5-",
          "\u00a5-,.",
          "-",
          "- \u00a5",
          "-\u00a5",
        ],
      });

      test({
        validator: "isCurrency",
        args: [
          {
            negative_sign_after_digits: true,
          },
        ],
        valid: [
          "$10,123.45-",
          "$10,123.45",
          "$10123.45",
          "10,123.45",
          "10123.45",
          "10,123",
          "1,123,456",
          "1123456",
          "1.39",
          ".03",
          "0.10",
          "$0.10",
          "$0.01-",
          "$.99-",
          "$100,234,567.89",
          "$10,123",
          "10,123",
          "10123-",
        ],
        invalid: [
          "-123",
          "1.234",
          "$1.1",
          "$ 32.50",
          "500$",
          ".0001",
          "$.001",
          "$0.001",
          "12,34.56",
          "123456,123,123456",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
        ],
      });

      // \u00a5##,###.## with no negatives (zh-CN)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "\u00a5",
            allow_negatives: false,
          },
        ],
        valid: [
          "123,456.78",
          "\u00a56,954,231",
          "\u00a510.03",
          "10.03",
          "1.39",
          ".03",
          "0.10",
          "\u00a50.01",
          "\u00a51,234,567.89",
          "\u00a510,123",
          "10,123",
          "10123",
          "\u00a5100",
        ],
        invalid: [
          "1.234",
          "-123,456.78",
          "\u00a5-6,954,231",
          "\u00a5-10.03",
          "\u00a5-10567.01",
          "\u00a51.1",
          "\u00a5-10,123",
          "\u00a5-10,123.45",
          "5,00",
          "\u00a5-100",
          ".0001",
          "\u00a5.001",
          "\u00a5-.001",
          "\u00a50.001",
          "12,34.56",
          "123456,123,123456",
          "123 456",
          ",123",
          "\u00a5-,123",
          "",
          " ",
          "\u00a5",
          "\u00a5-",
          "\u00a5-,.",
          "-",
          "- \u00a5",
          "-\u00a5",
        ],
      });

      // R ## ###,## and R-10 123,25 (el-ZA)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "R",
            negative_sign_before_digits: true,
            thousands_separator: " ",
            decimal_separator: ",",
            allow_negative_sign_placeholder: true,
          },
        ],
        valid: [
          "123 456,78",
          "-10 123",
          "R-10 123",
          "R 6 954 231",
          "R10,03",
          "10,03",
          "1,39",
          ",03",
          "0,10",
          "R10567,01",
          "R0,01",
          "R1 234 567,89",
          "R10 123",
          "R 10 123",
          "R 10123",
          "R-10123",
          "10 123",
          "10123",
        ],
        invalid: [
          "1,234",
          "R -10123",
          "R- 10123",
          "R,1",
          ",0001",
          "R,001",
          "R0,001",
          "12 34,56",
          "123456 123 123456",
          " 123",
          "- 123",
          "123 ",
          "",
          " ",
          "R",
          "R- .1",
          "R-",
          "-",
          "-R 10123",
          "R00",
          "R -",
          "-R",
        ],
      });

      // -\u20ac ##.###,## (it-IT)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "\u20ac",
            thousands_separator: ".",
            decimal_separator: ",",
            allow_space_after_symbol: true,
          },
        ],
        valid: [
          "123.456,78",
          "-123.456,78",
          "\u20ac6.954.231",
          "-\u20ac6.954.231",
          "\u20ac 896.954.231",
          "-\u20ac 896.954.231",
          "16.954.231",
          "-16.954.231",
          "\u20ac10,03",
          "-\u20ac10,03",
          "10,03",
          "-10,03",
          "-1,39",
          ",03",
          "0,10",
          "-\u20ac10567,01",
          "-\u20ac 10567,01",
          "\u20ac 0,01",
          "\u20ac1.234.567,89",
          "\u20ac10.123",
          "10.123",
          "-\u20ac10.123",
          "\u20ac 10.123",
          "\u20ac10.123",
          "\u20ac 10123",
          "10.123",
          "-10123",
        ],
        invalid: [
          "1,234",
          "\u20ac 1,1",
          "50#,50",
          "123,@\u20ac ",
          "\u20ac\u20ac500",
          ",0001",
          "\u20ac ,001",
          "\u20ac0,001",
          "12.34,56",
          "123456.123.123456",
          "\u20ac123\u20ac",
          "",
          " ",
          "\u20ac",
          " \u20ac",
          "\u20ac ",
          "\u20ac\u20ac",
          " 123",
          "- 123",
          ".123",
          "-\u20ac.123",
          "123 ",
          "\u20ac-",
          "- \u20ac",
          "\u20ac - ",
          "-",
          "- ",
          "-\u20ac",
        ],
      });

      // -##.###,## \u20ac (el-GR)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "\u20ac",
            thousands_separator: ".",
            symbol_after_digits: true,
            decimal_separator: ",",
            allow_space_after_digits: true,
          },
        ],
        valid: [
          "123.456,78",
          "-123.456,78",
          "6.954.231 \u20ac",
          "-6.954.231 \u20ac",
          "896.954.231",
          "-896.954.231",
          "16.954.231",
          "-16.954.231",
          "10,03\u20ac",
          "-10,03\u20ac",
          "10,03",
          "-10,03",
          "1,39",
          ",03",
          "-,03",
          "-,03 \u20ac",
          "-,03\u20ac",
          "0,10",
          "10567,01\u20ac",
          "0,01 \u20ac",
          "1.234.567,89\u20ac",
          "10.123\u20ac",
          "10.123",
          "10.123\u20ac",
          "10.123 \u20ac",
          "10123 \u20ac",
          "10.123",
          "10123",
        ],
        invalid: [
          "1,234",
          "1,1 \u20ac",
          ",0001",
          ",001 \u20ac",
          "0,001\u20ac",
          "12.34,56",
          "123456.123.123456",
          "\u20ac123\u20ac",
          "",
          " ",
          "\u20ac",
          " \u20ac",
          "\u20ac ",
          " 123",
          "- 123",
          ".123",
          "-.123\u20ac",
          "-.123 \u20ac",
          "123 ",
          "-\u20ac",
          "- \u20ac",
          "-",
          "- ",
        ],
      });

      // kr. -##.###,## (da-DK)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "kr.",
            negative_sign_before_digits: true,
            thousands_separator: ".",
            decimal_separator: ",",
            allow_space_after_symbol: true,
          },
        ],
        valid: [
          "123.456,78",
          "-10.123",
          "kr. -10.123",
          "kr.-10.123",
          "kr. 6.954.231",
          "kr.10,03",
          "kr. -10,03",
          "10,03",
          "1,39",
          ",03",
          "0,10",
          "kr. 10567,01",
          "kr. 0,01",
          "kr. 1.234.567,89",
          "kr. -1.234.567,89",
          "10.123",
          "kr. 10.123",
          "kr.10.123",
          "10123",
          "10.123",
          "kr.-10123",
        ],
        invalid: [
          "1,234",
          "kr.  -10123",
          "kr.,1",
          ",0001",
          "kr. ,001",
          "kr.0,001",
          "12.34,56",
          "123456.123.123456",
          ".123",
          "kr.-.123",
          "kr. -.123",
          "- 123",
          "123 ",
          "",
          " ",
          "kr.",
          " kr.",
          "kr. ",
          "kr.-",
          "kr. -",
          "kr. - ",
          " - ",
          "-",
          "- kr.",
          "-kr.",
        ],
      });

      // kr. ##.###,## with no negatives (da-DK)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "kr.",
            allow_negatives: false,
            negative_sign_before_digits: true,
            thousands_separator: ".",
            decimal_separator: ",",
            allow_space_after_symbol: true,
          },
        ],
        valid: [
          "123.456,78",
          "10.123",
          "kr. 10.123",
          "kr.10.123",
          "kr. 6.954.231",
          "kr.10,03",
          "kr. 10,03",
          "10,03",
          "1,39",
          ",03",
          "0,10",
          "kr. 10567,01",
          "kr. 0,01",
          "kr. 1.234.567,89",
          "kr.1.234.567,89",
          "10.123",
          "kr. 10.123",
          "kr.10.123",
          "10123",
          "10.123",
          "kr.10123",
        ],
        invalid: [
          "1,234",
          "-10.123",
          "kr. -10.123",
          "kr. -1.234.567,89",
          "kr.-10123",
          "kr.  -10123",
          "kr.-10.123",
          "kr. -10,03",
          "kr.,1",
          ",0001",
          "kr. ,001",
          "kr.0,001",
          "12.34,56",
          "123456.123.123456",
          ".123",
          "kr.-.123",
          "kr. -.123",
          "- 123",
          "123 ",
          "",
          " ",
          "kr.",
          " kr.",
          "kr. ",
          "kr.-",
          "kr. -",
          "kr. - ",
          " - ",
          "-",
          "- kr.",
          "-kr.",
        ],
      });

      // ($##,###.##) (en-US, en-HK)
      test({
        validator: "isCurrency",
        args: [
          {
            parens_for_negatives: true,
          },
        ],
        valid: [
          "1,234",
          "(1,234)",
          "($6,954,231)",
          "$10.03",
          "(10.03)",
          "($10.03)",
          "1.39",
          ".03",
          "(.03)",
          "($.03)",
          "0.10",
          "$10567.01",
          "($0.01)",
          "$1,234,567.89",
          "$10,123",
          "(10,123)",
          "10123",
        ],
        invalid: [
          "1.234",
          "($1.1)",
          "-$1.10",
          "$ 32.50",
          "500$",
          ".0001",
          "$.001",
          "($0.001)",
          "12,34.56",
          "123456,123,123456",
          "( 123)",
          ",123",
          "$-,123",
          "",
          " ",
          "  ",
          "   ",
          "$",
          "$ ",
          " $",
          " 123",
          "(123) ",
          ".",
          ",",
          "00",
          "$-",
          "$ - ",
          "$- ",
          " - ",
          "-",
          "- $",
          "-$",
          "()",
          "( )",
          "(  -)",
          "(  - )",
          "(  -  )",
          "(-)",
          "(-$)",
        ],
      });
      // $##,###.## with no negatives (en-US, en-CA, en-AU, en-HK)
      test({
        validator: "isCurrency",
        args: [{ allow_negatives: false }],
        valid: [
          "$10,123.45",
          "$10123.45",
          "10,123.45",
          "10123.45",
          "10,123",
          "1,123,456",
          "1123456",
          "1.39",
          ".03",
          "0.10",
          "$0.10",
          "$100,234,567.89",
          "$10,123",
          "10,123",
        ],
        invalid: [
          "1.234",
          "-1.234",
          "-10123",
          "-$0.01",
          "-$.99",
          "$1.1",
          "-$1.1",
          "$ 32.50",
          "500$",
          ".0001",
          "$.001",
          "$0.001",
          "12,34.56",
          "123456,123,123456",
          "-123456,123,123456",
          "123,4",
          ",123",
          "$-,123",
          "$",
          ".",
          ",",
          "00",
          "$-",
          "$-,.",
          "-",
          "-$",
          "",
          "- $",
          "-$10,123.45",
        ],
      });

      //  R$ ##,###.## (pt_BR)
      test({
        validator: "isCurrency",
        args: [
          {
            symbol: "R$",
            require_symbol: true,
            allow_space_after_symbol: true,
            symbol_after_digits: false,
            thousands_separator: ".",
            decimal_separator: ",",
          },
        ],
        valid: ["R$ 1.400,00", "R$ 400,00"],
        invalid: ["$ 1.400,00", "$R 1.400,00"],
      });
    });

    it("should validate Ethereum addresses", () => {
      test({
        validator: "isEthereumAddress",
        valid: [
          "0x0000000000000000000000000000000000000001",
          "0x683E07492fBDfDA84457C16546ac3f433BFaa128",
          "0x88dA6B6a8D3590e88E0FcadD5CEC56A7C9478319",
          "0x8a718a84ee7B1621E63E680371e0C03C417cCaF6",
          "0xFCb5AFB808b5679b4911230Aa41FfCD0cd335b42",
        ],
        invalid: [
          "0xGHIJK05pwm37asdf5555QWERZCXV2345AoEuIdHt",
          "0xFCb5AFB808b5679b4911230Aa41FfCD0cd335b422222",
          "0xFCb5AFB808b5679b4911230Aa41FfCD0cd33",
          "0b0110100001100101011011000110110001101111",
          "683E07492fBDfDA84457C16546ac3f433BFaa128",
          "1C6o5CDkLxjsVpnLSuqRs1UBFozXLEwYvU",
        ],
      });
    });

    it("should validate Bitcoin addresses", () => {
      test({
        validator: "isBtcAddress",
        valid: [
          "1MUz4VMYui5qY1mxUiG8BQ1Luv6tqkvaiL",
          "mucFNhKMYoBQYUAEsrFVscQ1YaFQPekBpg",
          "3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy",
          "2NFUBBRcTJbYc1D4HSCbJhKZp6YCV4PQFpQ",
          "bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq",
          "14qViLJfdGaP4EeHnDyJbEGQysnCpwk3gd",
          "35bSzXvRKLpHsHMrzb82f617cV4Srnt7hS",
          "17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhemt",
          "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4",
          "tb1qxhkl607frtvjsy9nlyeg03lf6fsq947pl2pe82",
          "bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg3297",
          "tb1pzpelffrdh9ptpaqnurwx30dlewqv57rcxfeetp86hsssk30p4cws38tr9y",
        ],
        invalid: [
          "3J98t1WpEZ73CNmQviecrnyiWrnqh0WNL0",
          "3J98t1WpEZ73CNmQviecrnyiWrnqh0WNLo",
          "3J98t1WpEZ73CNmQviecrnyiWrnqh0WNLI",
          "3J98t1WpEZ73CNmQviecrnyiWrnqh0WNLl",
          "4J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy",
          "0x56F0B8A998425c53c75C4A303D4eF987533c5597",
          "pp8skudq3x5hzw8ew7vzsw8tn4k8wxsqsv0lt0mf3g",
          "17VZNX1SN5NlKa8UQFxwQbFeFc3iqRYhem",
          "BC1QW508D6QEJXTDG4Y5R3ZARVAYR0C5XW7KV8F3T4",
          "bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg3291",
          "bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg329b",
          "bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg329i",
          "bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg329o",
          "BC1P5D7RJQ7G6RDK2YHZKS9SMLAQTEDR4DEKQ08GE8ZTWAC72SFR9RUSXG3297",
          "TB1PZPELFFRDH9PTPAQNURWX30DLEWQV57RCXFEETP86HSSSK30P4CWS38TR9Y",
        ],
      });
    });

    it("should validate booleans", () => {
      test({
        validator: "isBoolean",
        valid: ["true", "false", "0", "1"],
        invalid: ["1.0", "0.0", "true ", "False", "True", "yes"],
      });
    });

    it("should validate booleans with option loose set to true", () => {
      test({
        validator: "isBoolean",
        args: [{ loose: true }],
        valid: [
          "true",
          "True",
          "TRUE",
          "false",
          "False",
          "FALSE",
          "0",
          "1",
          "yes",
          "Yes",
          "YES",
          "no",
          "No",
          "NO",
        ],
        invalid: ["1.0", "0.0", "true ", " false"],
      });
    });

    it("should validate ISO 639-1 language codes", () => {
      test({
        validator: "isISO6391",
        valid: ["ay", "az", "ba", "be", "bg"],
        invalid: ["aj", "al", "pe", "pf", "abc", "123", ""],
      });
    });

    const validISO8601 = [
      "2009-12T12:34",
      "2009",
      "2009-05-19",
      "2009-05-19",
      "20090519",
      "2009123",
      "2009-05",
      "2009-123",
      "2009-222",
      "2009-001",
      "2009-W01-1",
      "2009-W51-1",
      "2009-W511",
      "2009-W33",
      "2009W511",
      "2009-05-19",
      "2009-05-19 00:00",
      "2009-05-19 14",
      "2009-05-19 14:31",
      "2009-05-19 14:39:22",
      "2009-05-19T14:39Z",
      "2009-W21-2",
      "2009-W21-2T01:22",
      "2009-139",
      "2009-05-19 14:39:22-06:00",
      "2009-05-19 14:39:22+0600",
      "2009-05-19 14:39:22-01",
      "20090621T0545Z",
      "2007-04-06T00:00",
      "2007-04-05T24:00",
      "2010-02-18T16:23:48.5",
      "2010-02-18T16:23:48,444",
      "2010-02-18T16:23:48,3-06:00",
      "2010-02-18T16:23.4",
      "2010-02-18T16:23,25",
      "2010-02-18T16:23.33+0600",
      "2010-02-18T16.23334444",
      "2010-02-18T16,2283",
      "2009-05-19 143922.500",
      "2009-05-19 1439,55",
      "2009-10-10",
      "2020-366",
      "2000-366",
    ];

    const invalidISO8601 = [
      "200905",
      "2009367",
      "2009-",
      "2007-04-05T24:50",
      "2009-000",
      "2009-M511",
      "2009M511",
      "2009-05-19T14a39r",
      "2009-05-19T14:3924",
      "2009-0519",
      "2009-05-1914:39",
      "2009-05-19 14:",
      "2009-05-19r14:39",
      "2009-05-19 14a39a22",
      "200912-01",
      "2009-05-19 14:39:22+06a00",
      "2009-05-19 146922.500",
      "2010-02-18T16.5:23.35:48",
      "2010-02-18T16:23.35:48",
      "2010-02-18T16:23.35:48.45",
      "2009-05-19 14.5.44",
      "2010-02-18T16:23.33.600",
      "2010-02-18T16,25:23:48,444",
      "2010-13-1",
      "nonsense2021-01-01T00:00:00Z",
      "2021-01-01T00:00:00Znonsense",
    ];

    it("should validate ISO 8601 dates", () => {
      // from http://www.pelagodesign.com/blog/2009/05/20/iso-8601-date-validation-that-doesnt-suck/
      test({
        validator: "isISO8601",
        valid: validISO8601,
        invalid: invalidISO8601,
      });
    });

    it("should validate ISO 8601 dates, with strict = true (regression)", () => {
      test({
        validator: "isISO8601",
        args: [{ strict: true }],
        valid: validISO8601,
        invalid: invalidISO8601,
      });
    });

    it("should validate ISO 8601 dates, with strict = true", () => {
      test({
        validator: "isISO8601",
        args: [{ strict: true }],
        valid: ["2000-02-29", "2009-123", "2009-222", "2020-366", "2400-366"],
        invalid: ["2010-02-30", "2009-02-29", "2009-366", "2019-02-31"],
      });
    });

    it("should validate ISO 8601 dates, with strictSeparator = true", () => {
      test({
        validator: "isISO8601",
        args: [{ strictSeparator: true }],
        valid: [
          "2009-12T12:34",
          "2009",
          "2009-05-19",
          "2009-05-19",
          "20090519",
          "2009123",
          "2009-05",
          "2009-123",
          "2009-222",
          "2009-001",
          "2009-W01-1",
          "2009-W51-1",
          "2009-W511",
          "2009-W33",
          "2009W511",
          "2009-05-19",
          "2009-05-19T14:39Z",
          "2009-W21-2",
          "2009-W21-2T01:22",
          "2009-139",
          "20090621T0545Z",
          "2007-04-06T00:00",
          "2007-04-05T24:00",
          "2010-02-18T16:23:48.5",
          "2010-02-18T16:23:48,444",
          "2010-02-18T16:23:48,3-06:00",
          "2010-02-18T16:23.4",
          "2010-02-18T16:23,25",
          "2010-02-18T16:23.33+0600",
          "2010-02-18T16.23334444",
          "2010-02-18T16,2283",
          "2009-10-10",
          "2020-366",
          "2000-366",
        ],
        invalid: [
          "200905",
          "2009367",
          "2009-",
          "2007-04-05T24:50",
          "2009-000",
          "2009-M511",
          "2009M511",
          "2009-05-19T14a39r",
          "2009-05-19T14:3924",
          "2009-0519",
          "2009-05-1914:39",
          "2009-05-19 14:",
          "2009-05-19r14:39",
          "2009-05-19 14a39a22",
          "200912-01",
          "2009-05-19 14:39:22+06a00",
          "2009-05-19 146922.500",
          "2010-02-18T16.5:23.35:48",
          "2010-02-18T16:23.35:48",
          "2010-02-18T16:23.35:48.45",
          "2009-05-19 14.5.44",
          "2010-02-18T16:23.33.600",
          "2010-02-18T16,25:23:48,444",
          "2010-13-1",
          "2009-05-19 00:00",
          // Previously valid cases
          "2009-05-19 14",
          "2009-05-19 14:31",
          "2009-05-19 14:39:22",
          "2009-05-19 14:39:22-06:00",
          "2009-05-19 14:39:22+0600",
          "2009-05-19 14:39:22-01",
        ],
      });
    });

    it("should validate ISO 8601 dates, with strict = true and strictSeparator = true (regression)", () => {
      test({
        validator: "isISO8601",
        args: [{ strict: true, strictSeparator: true }],
        valid: ["2000-02-29", "2009-123", "2009-222", "2020-366", "2400-366"],
        invalid: [
          "2010-02-30",
          "2009-02-29",
          "2009-366",
          "2019-02-31",
          "2009-05-19 14",
          "2009-05-19 14:31",
          "2009-05-19 14:39:22",
          "2009-05-19 14:39:22-06:00",
          "2009-05-19 14:39:22+0600",
          "2009-05-19 14:39:22-01",
        ],
      });
    });

    it("should validate ISO 15924 script codes", () => {
      test({
        validator: "isISO15924",
        valid: ["Adlm", "Bass", "Copt", "Dsrt", "Egyd", "Latn", "Zzzz"],
        invalid: ["", "arab", "zzzz", "Qaby", "Lati"],
      });
    });

    it("should validate RFC 3339 dates", () => {
      test({
        validator: "isRFC3339",
        valid: [
          "2009-05-19 14:39:22-06:00",
          "2009-05-19 14:39:22+06:00",
          "2009-05-19 14:39:22Z",
          "2009-05-19T14:39:22-06:00",
          "2009-05-19T14:39:22Z",
          "2010-02-18T16:23:48.3-06:00",
          "2010-02-18t16:23:33+06:00",
          "2010-02-18t16:23:33+06:00",
          "2010-02-18t16:12:23.23334444z",
          "2010-02-18T16:23:55.2283Z",
          "2009-05-19 14:39:22.500Z",
          "2009-05-19 14:39:55Z",
          "2009-05-31 14:39:55Z",
          "2009-05-31 14:53:60Z",
          "2010-02-18t00:23:23.33+06:00",
          "2010-02-18t00:23:32.33+00:00",
          "2010-02-18t00:23:32.33+23:00",
        ],
        invalid: [
          "2010-02-18t00:23:32.33+24:00",
          "2009-05-31 14:60:55Z",
          "2010-02-18t24:23.33+0600",
          "2009-05-00 1439,55Z",
          "2009-13-19 14:39:22-06:00",
          "2009-05-00 14:39:22+0600",
          "2009-00-1 14:39:22Z",
          "2009-05-19T14:39:22",
          "nonsense2021-01-01T00:00:00Z",
          "2021-01-01T00:00:00Znonsense",
        ],
      });
    });

    it("should validate ISO 3166-1 alpha 2 country codes", () => {
      // from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
      test({
        validator: "isISO31661Alpha2",
        valid: [
          "FR",
          "fR",
          "GB",
          "PT",
          "CM",
          "JP",
          "PM",
          "ZW",
          "MM",
          "cc",
          "GG",
        ],
        invalid: ["", "FRA", "AA", "PI", "RP", "WV", "WL", "UK", "ZZ"],
      });
    });

    it("should validate ISO 3166-1 alpha 3 country codes", () => {
      // from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-3
      test({
        validator: "isISO31661Alpha3",
        valid: ["ABW", "HND", "KHM", "RWA"],
        invalid: ["", "FR", "fR", "GB", "PT", "CM", "JP", "PM", "ZW"],
      });
    });

    it("should validate ISO 3166-1 numeric country codes", () => {
      // from https://en.wikipedia.org/wiki/ISO_3166-1_numeric
      test({
        validator: "isISO31661Numeric",
        valid: [
          "076",
          "208",
          "276",
          "348",
          "380",
          "410",
          "440",
          "528",
          "554",
          "826",
        ],
        invalid: [
          "",
          "NL",
          "NLD",
          "002",
          "197",
          "249",
          "569",
          "810",
          "900",
          "999",
        ],
      });
    });

    it("should validate ISO 4217 corrency codes", () => {
      // from https://en.wikipedia.org/wiki/ISO_4217
      test({
        validator: "isISO4217",
        valid: [
          "AED",
          "aed",
          "AUD",
          "CUP",
          "EUR",
          "GBP",
          "LYD",
          "MYR",
          "SGD",
          "SLE",
          "USD",
          "VED",
          "SLE",
        ],
        invalid: [
          "",
          "$",
          "US",
          "us",
          "AAA",
          "aaa",
          "RWA",
          "EURO",
          "euro",
          "HRK",
          "CUC",
        ],
      });
    });

    it("should validate whitelisted characters", () => {
      test({
        validator: "isWhitelisted",
        args: ["abcdefghijklmnopqrstuvwxyz-"],
        valid: ["foo", "foobar", "baz-foo"],
        invalid: ["foo bar", "fo.bar", "t\u00fcrk\u00e7e"],
      });
    });

    it("should error on non-string input", () => {
      test({
        validator: "isEmpty",
        error: [undefined, null, [], NaN],
      });
    });

    it("should validate dataURI", () => {
      /* eslint-disable max-len */
      test({
        validator: "isDataURI",
        valid: [
          "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC",
          "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAIBAMAAAA2IaO4AAAAFVBMVEXk5OTn5+ft7e319fX29vb5+fn///++GUmVAAAALUlEQVQIHWNICnYLZnALTgpmMGYIFWYIZTA2ZFAzTTFlSDFVMwVyQhmAwsYMAKDaBy0axX/iAAAAAElFTkSuQmCC",
          "data:application/media_control+xml;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAIBAMAAAA2IaO4AAAAFVBMVEXk5OTn5+ft7e319fX29vb5+fn///++GUmVAAAALUlEQVQIHWNICnYLZnALTgpmMGYIFWYIZTA2ZFAzTTFlSDFVMwVyQhmAwsYMAKDaBy0axX/iAAAAAElFTkSuQmCC",
          "   data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAIBAMAAAA2IaO4AAAAFVBMVEXk5OTn5+ft7e319fX29vb5+fn///++GUmVAAAALUlEQVQIHWNICnYLZnALTgpmMGYIFWYIZTA2ZFAzTTFlSDFVMwVyQhmAwsYMAKDaBy0axX/iAAAAAElFTkSuQmCC   ",
          "data:image/svg+xml;charset=utf-8,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22100%22%20height%3D%22100%22%3E%3Crect%20fill%3D%22%2300B1FF%22%20width%3D%22100%22%20height%3D%22100%22%2F%3E%3C%2Fsvg%3E",
          "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMDAiIGhlaWdodD0iMTAwIj48cmVjdCBmaWxsPSIjMDBCMUZGIiB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIvPjwvc3ZnPg==",
          " data:,Hello%2C%20World!",
          " data:,Hello World!",
          " data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D",
          " data:text/html,%3Ch1%3EHello%2C%20World!%3C%2Fh1%3E",
          "data:,A%20brief%20note",
          "data:text/html;charset=US-ASCII,%3Ch1%3EHello!%3C%2Fh1%3E",
          "data:application/vnd.openxmlformats-officedocument.wordprocessingml.document;base64,dGVzdC5kb2N4",
        ],
        invalid: [
          "dataxbase64",
          "data:HelloWorld",
          "data:,A%20brief%20invalid%20[note",
          "file:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D",
          "data:text/html;charset=,%3Ch1%3EHello!%3C%2Fh1%3E",
          "data:text/html;charset,%3Ch1%3EHello!%3C%2Fh1%3E",
          "data:base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC",
          "",
          "http://wikipedia.org",
          "base64",
          "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAABlBMVEUAAAD///+l2Z/dAAAAM0lEQVR4nGP4/5/h/1+G/58ZDrAz3D/McH8yw83NDDeNGe4Ug9C9zwz3gVLMDA/A6P9/AFGGFyjOXZtQAAAAAElFTkSuQmCC",
        ],
      });
      /* eslint-enable max-len */
    });

    it("should validate magnetURI", () => {
      /* eslint-disable max-len */
      test({
        validator: "isMagnetURI",
        valid: [
          "magnet:?xt.1=urn:sha1:ABCDEFGHIJKLMNOPQRSTUVWXYZ123456&xt.2=urn:sha1:ABCDEFGHIJKLMNOPQRSTUVWXYZ123456",
          "magnet:?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234&dn=helloword2000&tr=udp://helloworld:1337/announce",
          "magnet:?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234&dn=foo",
          "magnet:?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234&dn=&tr=&nonexisting=hello world",
          "magnet:?xt=urn:md5:ABCDEFGHIJKLMNOPQRSTUVWXYZ123456",
          "magnet:?xt=urn:tree:tiger:ABCDEFGHIJKLMNOPQRSTUVWXYZ123456",
          "magnet:?xt=urn:ed2k:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?tr=udp://helloworld:1337/announce&xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?xt=urn:btmh:1220caf1e1c30e81cb361b9ee167c4aa64228a7fa4fa9f6105232b28ad099f3a302e",
        ],
        invalid: [
          ":?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magneta:?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?xt=uarn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?xt=urn:btihz",
          "magnet::?xt=urn:btih:UHWY2892JNEJ2GTEYOMDNU67E8ICGICYE92JDUGH",
          "magnet:?xt:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ",
          "magnet:?xt:urn:nonexisting:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?xt.2=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234",
          "magnet:?xt=urn:ed2k:ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678901234567890123456789ABCD",
          "magnet:?xt=urn:btmh:1120caf1e1c30e81cb361b9ee167c4aa64228a7fa4fa9f6105232b28ad099f3a302e",
          "magnet:?ttxt=urn:btmh:1220caf1e1c30e81cb361b9ee167c4aa64228a7fa4fa9f6105232b28ad099f3a302e",
        ],
      });
      /* eslint-enable max-len */
    });

    it("should validate LatLong", () => {
      test({
        validator: "isLatLong",
        valid: [
          "(-17.738223, 85.605469)",
          "(-12.3456789, +12.3456789)",
          "(-60.978437, -0.175781)",
          "(77.719772, -37.529297)",
          "(7.264394, 165.058594)",
          "0.955766, -19.863281",
          "(31.269161,164.355469)",
          "+12.3456789, -12.3456789",
          "-15.379543, -137.285156",
          "(11.770570, -162.949219)",
          "-55.034319, 113.027344",
          "58.025555, 36.738281",
          "55.720923,-28.652344",
          "-90.00000,-180.00000",
          "(-71, -146)",
          "(-71.616864, -146.616864)",
          "-0.55, +0.22",
          "90, 180",
          "+90, -180",
          "-90,+180",
          "90,180",
          "0, 0",
        ],
        invalid: [
          "(020.000000, 010.000000000)",
          "89.9999999989, 360.0000000",
          "90.1000000, 180.000000",
          "+90.000000, -180.00001",
          "090.0000, 0180.0000",
          "126, -158",
          "(-126.400010, -158.400010)",
          "-95, -96",
          "-95.738043, -96.738043",
          "137, -148",
          "(-137.5942, -148.5942)",
          "(-120, -203)",
          "(-119, -196)",
          "+119.821728, -196.821728",
          "(-110, -223)",
          "-110.369532, 223.369532",
          "(-120.969949, +203.969949)",
          "-116, -126",
          "-116.894222, -126.894222",
          "-112, -160",
          "-112.96381, -160.96381",
          "-90., -180.",
          "+90.1, -180.1",
          "(-17.738223, 85.605469",
          "0.955766, -19.863281)",
          "+,-",
          "(,)",
          ",",
          " ",
        ],
      });

      test({
        validator: "isLatLong",
        args: [
          {
            checkDMS: true,
          },
        ],
        valid: [
          "40\u00b0 26\u2032 46\u2033 N, 79\u00b0 58\u2032 56\u2033 W",
          "40\u00b0 26\u2032 46\u2033 S, 79\u00b0 58\u2032 56\u2033 E",
          "90\u00b0 0\u2032 0\u2033 S, 180\u00b0 0\u2032 0\u2033 E",
          "40\u00b0 26\u2032 45.9996\u2033 N, 79\u00b0 58\u2032 55.2\u2033 E",
          "40\u00b0 26\u2032 46\u2033 n, 79\u00b0 58\u2032 56\u2033 w",
          "40\u00b026\u203246\u2033s, 79\u00b058\u203256\u2033e",
          "11\u00b0 0\u2032 0.005\u2033 S, 180\u00b0 0\u2032 0\u2033 E",
          "40\u00b026\u203245.9996\u2033N, 79\u00b058\u203255.2\u2033E",
        ],
        invalid: [
          "100\u00b0 26\u2032 46\u2033 N, 79\u00b0 70\u2032 56\u2033 W",
          "40\u00b0 89\u2032 46\u2033 S, 79\u00b0 58\u2032 100\u2033 E",
          "40\u00b0 26.445\u2032 45\u2033 N, 79\u00b0 58\u2032 55.2\u2033 E",
          "40\u00b0 46\u2033 N, 79\u00b0 58\u2032 56\u2033 W",
        ],
      });
    });

    it("should validate postal code", () => {
      const fixtures = [
        {
          locale: "AU",
          valid: ["4000", "2620", "3000", "2017", "0800"],
        },
        {
          locale: "BD",
          valid: [
            "1000",
            "1200",
            "1300",
            "1400",
            "1500",
            "2000",
            "3000",
            "4000",
            "5000",
            "6000",
            "7000",
            "8000",
            "9000",
            "9400",
            "9499",
          ],
          invalid: [
            "0999",
            "9500",
            "10000",
            "12345",
            "123",
            "123456",
            "abcd",
            "123a",
            "a123",
            "12 34",
            "12-34",
          ],
        },
        {
          locale: "BY",
          valid: ["225320", "211120", "247710", "231960"],
          invalid: ["test 225320", "211120 test", "317543", "267946"],
        },
        {
          locale: "CA",
          valid: [
            "L4T 0A5",
            "G1A-0A2",
            "A1A 1A1",
            "X0A-0H0",
            "V5K 0A1",
            "A1C 3S4",
            "A1C3S4",
            "a1c 3s4",
            "V9A 7N2",
            "B3K 5X5",
            "K8N 5W6",
            "K1A 0B1",
            "B1Z 0B9",
          ],
          invalid: [
            "        ",
            "invalid value",
            "a1a1a",
            "A1A  1A1",
            "K1A 0D1",
            "W1A 0B1",
            "Z1A 0B1",
          ],
        },
        {
          locale: "CO",
          valid: ["050034", "110221", "441029", "910001"],
          invalid: ["11001", "000000", "109999", "329999"],
        },
        {
          locale: "ES",
          valid: ["01001", "52999", "27880"],
          invalid: ["123", "1234", "53000", "052999", "0123", "abcde"],
        },
        {
          locale: "JP",
          valid: ["135-0000", "874-8577", "669-1161", "470-0156", "672-8031"],
        },
        {
          locale: "GR",
          valid: ["022 93", "29934", "90293", "299 42", "94944"],
        },
        {
          locale: "GB",
          valid: [
            "TW8 9GS",
            "BS98 1TL",
            "DE99 3GG",
            "DE55 4SW",
            "DH98 1BT",
            "DH99 1NS",
            "GIR0aa",
            "SA99",
            "W1N 4DJ",
            "AA9A 9AA",
            "AA99 9AA",
            "BS98 1TL",
            "DE993GG",
          ],
        },
        {
          locale: "FR",
          valid: ["75008", "44522", "38499", "39940", "01000"],
          invalid: ["44 522", "38 499", "96000", "98025"],
        },
        {
          locale: "ID",
          valid: ["10210", "40181", "55161", "60233"],
        },
        {
          locale: "IE",
          valid: ["A65 TF12", "A6W U9U9"],
          invalid: [
            "123",
            "75690HG",
            "AW5  TF12",
            "AW5 TF12",
            "756  90HG",
            "A65T F12",
            "O62 O1O2",
          ],
        },
        {
          locale: "IN",
          valid: ["364240", "360005"],
          invalid: [
            "123",
            "012345",
            "011111",
            "101123",
            "291123",
            "351123",
            "541123",
            "551123",
            "651123",
            "661123",
            "861123",
            "871123",
            "881123",
            "891123",
          ],
        },
        {
          locale: "IL",
          valid: [
            "10200",
            "10292",
            "10300",
            "10329",
            "3885500",
            "4290500",
            "4286000",
            "7080000",
          ],
          invalid: [
            "123",
            "012345",
            "011111",
            "101123",
            "291123",
            "351123",
            "541123",
            "551123",
            "651123",
            "661123",
            "861123",
            "871123",
            "881123",
            "891123",
          ],
        },
        {
          locale: "BG",
          valid: ["1000"],
        },
        {
          locale: "IR",
          valid: ["4351666456", "5614736867"],
          invalid: [
            "43516 6456",
            "123443516 6456",
            "891123",
            "test 4351666456",
            "4351666456 test",
            "test 4351666456 test",
          ],
        },
        {
          locale: "CZ",
          valid: ["20134", "392 90", "39919", "938 29", "39949"],
        },
        {
          locale: "NL",
          valid: ["1012 SZ", "3432FE", "1118 BH", "3950IO", "3997 GH"],
          invalid: ["1234", "0603 JV", "5194SA", "9164 SD", "1841SS"],
        },
        {
          locale: "NP",
          valid: ["10811", "32600", "56806", "977"],
          invalid: ["11977", "asds", "13 32", "-977", "97765"],
        },
        {
          locale: "PL",
          valid: [
            "47-260",
            "12-930",
            "78-399",
            "39-490",
            "38-483",
            "05-800",
            "54-060",
          ],
        },
        {
          locale: "TW",
          valid: ["360", "90312", "399", "935", "38842", "546023"],
        },
        {
          locale: "LI",
          valid: ["9485", "9497", "9491", "9489", "9496"],
        },
        {
          locale: "PT",
          valid: ["4829-489", "0294-348", "8156-392"],
        },
        {
          locale: "SE",
          valid: ["12994", "284 39", "39556", "489 39", "499 49"],
        },
        {
          locale: "AD",
          valid: [
            "AD100",
            "AD200",
            "AD300",
            "AD400",
            "AD500",
            "AD600",
            "AD700",
          ],
        },
        {
          locale: "UA",
          valid: ["65000", "65080", "01000", "51901", "51909", "49125"],
        },
        {
          locale: "BR",
          valid: [
            "39100-000",
            "22040-020",
            "39400-152",
            "39100000",
            "22040020",
            "39400152",
          ],
          invalid: [
            "79800A12",
            "13165-00",
            "38175-abc",
            "81470-2763",
            "78908",
            "13010|111",
          ],
        },
        {
          locale: "NZ",
          valid: ["7843", "3581", "0449", "0984", "4144"],
        },
        {
          locale: "PK",
          valid: ["25000", "44000", "54810", "74200"],
          invalid: ["5400", "540000", "NY540", "540CA", "540-0"],
        },
        {
          locale: "MG",
          valid: ["101", "303", "407", "512"],
        },
        {
          locale: "MT",
          valid: ["VLT2345", "VLT 2345", "ATD1234", "MSK8723"],
        },
        {
          locale: "MY",
          valid: ["56000", "12000", "79502"],
        },
        {
          locale: "PR",
          valid: ["00979", "00631", "00786", "00987"],
        },
        {
          locale: "AZ",
          valid: ["AZ0100", "AZ0121", "AZ3500"],
          invalid: ["", " AZ0100", "AZ100", "AZ34340", "EN2020", "AY3030"],
        },
        {
          locale: "DO",
          valid: ["12345"],
          invalid: ["A1234", "123", "123456"],
        },
        {
          locale: "HT",
          valid: ["HT1234"],
          invalid: ["HT123", "HT12345", "AA1234"],
        },
        {
          locale: "TH",
          valid: ["10250", "72170", "12140"],
          invalid: ["T1025", "T72170", "12140TH"],
        },
        {
          locale: "SG",
          valid: ["308215", "546080"],
        },
        {
          locale: "CN",
          valid: ["150237", "100000"],
          invalid: ["141234", "386789", "ab1234"],
        },
        {
          locale: "KR",
          valid: ["17008", "339012"],
          invalid: ["1412347", "ab1234"],
        },
        {
          locale: "LK",
          valid: ["11500", "22200", "10370", "43000"],
          invalid: ["1234", "789389", "982"],
        },
        {
          locale: "BA",
          valid: ["76300", "71000", "75412", "76100", "88202", "88313"],
          invalid: ["1234", "789389", "98212", "11000"],
        },
      ];

      let allValid = [];

      // Test fixtures
      fixtures.forEach((fixture) => {
        if (fixture.valid) allValid = allValid.concat(fixture.valid);
        test({
          validator: "isPostalCode",
          valid: fixture.valid,
          invalid: fixture.invalid,
          args: [fixture.locale],
        });
      });

      // Test generics
      test({
        validator: "isPostalCode",
        valid: [
          ...allValid,
          "1234",
          "6900",
          "1292",
          "9400",
          "27616",
          "90210",
          "10001",
          "21201",
          "33142",
          "060623",
          "123456",
          "293940",
          "002920",
        ],
        invalid: [
          "asdf",
          "1",
          "ASDFGJKLmZXJtZtesting123",
          "Vml2YW11cyBmZXJtZtesting123",
          "48380480343",
          "29923-329393-2324",
          "4294924224",
          "13",
        ],
        args: ["any"],
      });
    });

    it("should error on invalid locale", () => {
      test({
        validator: "isPostalCode",
        args: ["is-NOT"],
        error: ["293940", "1234"],
      });
    });

    it("should validate MIME types", () => {
      test({
        validator: "isMimeType",
        valid: [
          "application/json",
          "application/xhtml+xml",
          "audio/mp4",
          "image/bmp",
          "font/woff2",
          "message/http",
          "model/vnd.gtw",
          "application/media_control+xml",
          "multipart/form-data",
          "multipart/form-data; boundary=something",
          "multipart/form-data; charset=utf-8; boundary=something",
          "multipart/form-data; boundary=something; charset=utf-8",
          'multipart/form-data; boundary=something; charset="utf-8"',
          'multipart/form-data; boundary="something"; charset=utf-8',
          'multipart/form-data; boundary="something"; charset="utf-8"',
          "text/css",
          "text/plain; charset=utf8",
          'Text/HTML;Charset="utf-8"',
          "text/html;charset=UTF-8",
          "Text/html;charset=UTF-8",
          "text/html; charset=us-ascii",
          "text/html; charset=us-ascii (Plain text)",
          'text/html; charset="us-ascii"',
          "video/mp4",
        ],
        invalid: [
          "",
          " ",
          "/",
          "f/b",
          "application",
          "application\\json",
          "application/json/text",
          "application/json; charset=utf-8",
          "audio/mp4; charset=utf-8",
          "image/bmp; charset=utf-8",
          "font/woff2; charset=utf-8",
          "message/http; charset=utf-8",
          "model/vnd.gtw; charset=utf-8",
          "video/mp4; charset=utf-8",
        ],
      });
    });

    it("should validate ISO6346 shipping containerID", () => {
      test({
        validator: "isISO6346",
        valid: [
          "HLXU2008419",
          "TGHU7599330",
          "ECMU4657496",
          "MEDU6246078",
          "YMLU2809976",
          "MRKU0046221",
          "EMCU3811879",
          "OOLU8643084",
          "HJCU1922713",
          "QJRZ123456",
        ],
        invalid: [
          "OOLU1922713",
          "HJCU1922413",
          "FCUI985619",
          "ECMJ4657496",
          "TBJA7176445",
          "AFFU5962593",
        ],
      });
    });
    it("should validate ISO6346 shipping containerID", () => {
      test({
        validator: "isFreightContainerID",
        valid: [
          "HLXU2008419",
          "TGHU7599330",
          "ECMU4657496",
          "MEDU6246078",
          "YMLU2809976",
          "MRKU0046221",
          "EMCU3811879",
          "OOLU8643084",
          "HJCU1922713",
          "QJRZ123456",
        ],
        invalid: [
          "OOLU1922713",
          "HJCU1922413",
          "FCUI985619",
          "ECMJ4657496",
          "TBJA7176445",
          "AFFU5962593",
        ],
      });
    });

    it("should validate ISO6346 shipping container IDs with checksum digit 10 represented as 0", () => {
      test({
        validator: "isISO6346",
        valid: [
          "APZU3789870",
          "TEMU1002030",
          "DFSU1704420",
          "CMAU2221480",
          "SEGU5060260",
          "FCIU8939320",
          "TRHU3495670",
          "MEDU3871410",
          "CMAU2184010",
          "TCLU2265970",
        ],
        invalid: [
          "APZU3789871", // Incorrect check digit
          "TEMU1002031",
          "DFSU1704421",
          "CMAU2221481",
          "SEGU5060261",
        ],
      });
    });
    it("should validate ISO6346 shipping container IDs with checksum digit 10 represented as 0", () => {
      test({
        validator: "isFreightContainerID",
        valid: [
          "APZU3789870",
          "TEMU1002030",
          "DFSU1704420",
          "CMAU2221480",
          "SEGU5060260",
          "FCIU8939320",
          "TRHU3495670",
          "MEDU3871410",
          "CMAU2184010",
          "TCLU2265970",
        ],
        invalid: [
          "APZU3789871", // Incorrect check digit
          "TEMU1002031",
          "DFSU1704421",
          "CMAU2221481",
          "SEGU5060261",
        ],
      });
    });

    // EU-UK valid numbers sourced from https://ec.europa.eu/taxation_customs/tin/specs/FS-TIN%20Algorithms-Public.docx or constructed by @tplessas.
    it("should validate taxID", () => {
      test({
        validator: "isTaxID",
        args: ["bg-BG"],
        valid: [
          "7501010010",
          "0101010012",
          "0111010010",
          "7521010014",
          "7541010019",
        ],
        invalid: [
          "750101001",
          "75010100101",
          "75-01010/01 0",
          "7521320010",
          "7501010019",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["cs-CZ"],
        valid: [
          "530121999",
          "530121/999",
          "530121/9990",
          "5301219990",
          "1602295134",
          "5451219994",
          "0424175466",
          "0532175468",
          "7159079940",
        ],
        invalid: [
          "53-0121 999",
          "530121000",
          "960121999",
          "0124175466",
          "0472301754",
          "1975116400",
          "7159079945",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["de-AT"],
        valid: ["931736581", "93-173/6581", "93--173/6581"],
        invalid: ["999999999", "93 173 6581", "93-173/65811", "93-173/658"],
      });
      test({
        validator: "isTaxID",
        args: ["de-DE"],
        valid: [
          "26954371827",
          "86095742719",
          "65929970489",
          "79608434120",
          "659/299/7048/9",
        ],
        invalid: [
          "26954371828",
          "86095752719",
          "8609575271",
          "860957527190",
          "65299970489",
          "65999970489",
          "6592997048-9",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["dk-DK"],
        valid: [
          "010111-1113",
          "0101110117",
          "2110084008",
          "2110489008",
          "2110595002",
          "2110197007",
          "0101110117",
          "0101110230",
        ],
        invalid: [
          "010111/1113",
          "010111111",
          "01011111133",
          "2110485008",
          "2902034000",
          "0101110630",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["el-CY"],
        valid: ["00123123T", "99652156X"],
        invalid: [
          "99652156A",
          "00124123T",
          "00123123",
          "001123123T",
          "00 12-3123/T",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["el-GR"],
        valid: ["758426713", "032792320", "054100004"],
        invalid: [
          "054100005",
          "05410000",
          "0541000055",
          "05 4100005",
          "05-410/0005",
          "658426713",
          "558426713",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["en-CA"],
        valid: [
          "000000000",
          "521719666",
          "469317481",
          "120217450",
          "480534858",
          "325268597",
          "336475660",
          "744797853",
          "130692544",
          "046454286",
        ],
        invalid: [
          "        ",
          "any value",
          "012345678",
          "111111111",
          "999999999",
          "657449110",
          "74 47 978 53",
          "744 797 853",
          "744-797-853",
          "981062432",
          "267500713",
          "2675o0713",
          "70597312",
          "7058973122",
          "069437151",
          "046454281",
          "146452286",
          "30x92544",
          "30692544",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["en-GB"],
        valid: ["1234567890", "AA123456A", "AA123456 "],
        invalid: [
          "GB123456A",
          "123456789",
          "12345678901",
          "NK123456A",
          "TN123456A",
          "ZZ123456A",
          "GB123456Z",
          "DM123456A",
          "AO123456A",
          "GB-123456A",
          "GB 123456 A",
          "GB123456  ",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["en-IE"],
        valid: ["1234567T", "1234567TW", "1234577W", "1234577WW", "1234577IA"],
        invalid: ["1234567", "1234577WWW", "1234577A", "1234577JA"],
      });
      test({
        validator: "isTaxID",
        args: ["en-US"],
        valid: [
          "01-1234567",
          "01 1234567",
          "011234567",
          "10-1234567",
          "02-1234567",
          "67-1234567",
          "15-1234567",
          "31-1234567",
          "99-1234567",
        ],
        invalid: [
          "0-11234567",
          "01#1234567",
          "01  1234567",
          "01 1234 567",
          "07-1234567",
          "28-1234567",
          "96-1234567",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["es-AR"],
        valid: [
          "20271633638",
          "23274986069",
          "27333234519",
          "30678561165",
          "33693450239",
          "30534868460",
          "23111111129",
          "34557619099",
        ],
        invalid: [
          "20-27163363-8",
          "20.27163363.8",
          "33693450231",
          "69345023",
          "693450233123123",
          "3369ew50231",
          "34557619095",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["es-ES"],
        valid: [
          "00054237A",
          "54237A",
          "X1234567L",
          "Z1234567R",
          "M2812345C",
          "Y2812345B",
        ],
        invalid: ["M2812345CR", "A2812345C", "0/005 423-7A", "00054237U"],
      });
      test({
        validator: "isTaxID",
        args: ["et-EE"],
        valid: ["10001010080", "46304280206", "37102250382", "32708101201"],
        invalid: [
          "46304280205",
          "61002293333",
          "4-6304 28/0206",
          "4630428020",
          "463042802066",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["fi-FI"],
        valid: ["131052-308T", "131002+308W", "131019A3089"],
        invalid: [
          "131052308T",
          "131052-308TT",
          "131052S308T",
          "13 1052-308/T",
          "290219A1111",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["fr-BE"],
        valid: ["00012511119"],
      });
      test({
        validator: "isTaxID",
        args: ["fr-FR"],
        valid: ["30 23 217 600 053", "3023217600053"],
        invalid: [
          "30 2 3 217 600 053",
          "3 023217-600/053",
          "3023217600052",
          "3023217500053",
          "30232176000534",
          "302321760005",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["nl-BE"],
        valid: ["00012511148", "00/0125-11148", "00000011115"],
        invalid: [
          "00 01 2511148",
          "01022911148",
          "00013211148",
          "0001251114",
          "000125111480",
          "00012511149",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["fr-LU"],
        valid: ["1893120105732"],
        invalid: [
          "189312010573",
          "18931201057322",
          "1893 12-01057/32",
          "1893120105742",
          "1893120105733",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["lb-LU"],
        invalid: ["2016023005732"],
      });
      test({
        validator: "isTaxID",
        args: ["hr-HR"],
        valid: ["94577403194"],
        invalid: [
          "94 57-7403/194",
          "9457740319",
          "945774031945",
          "94577403197",
          "94587403194",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["hu-HU"],
        valid: ["8071592153"],
        invalid: [
          "80 71-592/153",
          "80715921534",
          "807159215",
          "8071592152",
          "8071582153",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["lt-LT"],
        valid: ["33309240064"],
      });
      test({
        validator: "isTaxID",
        args: ["it-IT"],
        valid: ["DMLPRY77D15H501F", "AXXFAXTTD41H501D"],
        invalid: [
          "DML PRY/77D15H501-F",
          "DMLPRY77D15H501",
          "DMLPRY77D15H501FF",
          "AAPPRY77D15H501F",
          "DMLAXA77D15H501F",
          "AXXFAX90A01Z001F",
          "DMLPRY77B29H501F",
          "AXXFAX3TD41H501E",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["lv-LV"],
        valid: ["01011012344", "32579461005", "01019902341", "325794-61005"],
        invalid: ["010110123444", "0101101234", "01001612345", "290217-22343"],
      });
      test({
        validator: "isTaxID",
        args: ["mt-MT"],
        valid: ["1234567A", "882345608", "34581M", "199Z"],
        invalid: [
          "812345608",
          "88234560",
          "8823456088",
          "11234567A",
          "12/34-567 A",
          "88 23-456/08",
          "1234560A",
          "0000000M",
          "3200100G",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["nl-NL"],
        valid: ["174559434"],
        invalid: ["17455943", "1745594344", "17 455-94/34"],
      });
      test({
        validator: "isTaxID",
        args: ["pl-PL"],
        valid: [
          "2234567895",
          "02070803628",
          "02870803622",
          "02670803626",
          "01510813623",
        ],
        invalid: [
          "020708036285",
          "223456789",
          "22 345-678/95",
          "02 070-8036/28",
          "2234567855",
          "02223013623",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["pt-BR"],
        valid: [
          "35161990910",
          "74407265027",
          "05423994000172",
          "11867044000130",
        ],
        invalid: [
          "ABCDEFGH",
          "170.691.440-72",
          "11494282142",
          "74405265037",
          "11111111111",
          "48469799384",
          "94.592.973/0001-82",
          "28592361000192",
          "11111111111111",
          "111111111111112",
          "61938188550993",
          "82168365502729",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["pt-PT"],
        valid: ["299999998", "299992020"],
        invalid: ["2999999988", "29999999", "29 999-999/8"],
      });
      test({
        validator: "isTaxID",
        args: ["ro-RO"],
        valid: [
          "8001011234563",
          "9000123456789",
          "1001011234560",
          "3001011234564",
          "5001011234568",
        ],
        invalid: [
          "5001011234569",
          "500 1011-234/568",
          "500101123456",
          "50010112345688",
          "5001011504568",
          "8000230234563",
          "6000230234563",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["sk-SK"],
        valid: [
          "530121999",
          "536221/999",
          "031121999",
          "520229999",
          "1234567890",
        ],
        invalid: [
          "53012199999",
          "990101999",
          "530121000",
          "53012199",
          "53-0121 999",
          "535229999",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["sl-SI"],
        valid: ["15012557", "15012590"],
        invalid: ["150125577", "1501255", "15 01-255/7"],
      });
      test({
        validator: "isTaxID",
        args: ["sv-SE"],
        valid: [
          "640823-3234",
          "640883-3231",
          "6408833231",
          "19640823-3233",
          "196408233233",
          "19640883-3230",
          "200228+5266",
          "20180101-5581",
        ],
        invalid: [
          "640823+3234",
          "160230-3231",
          "160260-3231",
          "160260-323",
          "160260323",
          "640823+323",
          "640823323",
          "640823+32344",
          "64082332344",
          "19640823-32333",
          "1964082332333",
        ],
      });
      test({
        validator: "isTaxID",
        args: ["uk-UA"],
        valid: ["3006321856", "3003102490", "2164212906"],
        invalid: ["2565975632", "256597563287", "\u041a\u042100123456", "2896235845"],
      });
      test({
        validator: "isTaxID",
        valid: ["01-1234567"],
      });
      test({
        validator: "isTaxID",
        args: ["is-NOT"],
        error: [
          "01-1234567",
          "01 1234567",
          "011234567",
          "0-11234567",
          "01#1234567",
          "01  1234567",
          "01 1234 567",
          "07-1234567",
          "28-1234567",
          "96-1234567",
        ],
      });
    });

    it("should validate slug", () => {
      test({
        validator: "isSlug",
        valid: [
          "foo",
          "foo-bar",
          "foo_bar",
          "foo-bar-foo",
          "foo-bar_foo",
          "foo-bar_foo*75-b4r-**_foo",
          "foo-bar_foo*75-b4r-**_foo-&&",
        ],
        invalid: [
          "not-----------slug",
          "@#_$@",
          "-not-slug",
          "not-slug-",
          "_not-slug",
          "not-slug_",
          "not slug",
        ],
      });
    });

    it("should validate strong passwords", () => {
      test({
        validator: "isStrongPassword",
        args: [
          {
            minLength: 8,
            minLowercase: 1,
            minUppercase: 1,
            minNumbers: 1,
            minSymbols: 1,
          },
        ],
        valid: [
          '%2%k{7BsL"M%Kd6e',
          "EXAMPLE of very long_password123!",
          "mxH_+2vs&54_+H3P",
          "+&DxJ=X7-4L8jRCD",
          "etV*p%Nr6w&H%FeF",
          "\u00a33.ndSau_7",
          "VaLIDWith\\Symb0l",
        ],
        invalid: [
          "",
          "password",
          "hunter2",
          "hello world",
          "passw0rd",
          "password!",
          "PASSWORD!",
        ],
      });
    });

    it("should validate date", () => {
      test({
        validator: "isDate",
        valid: [
          new Date(),
          new Date(2014, 2, 15),
          new Date("2014-03-15"),
          "2020/02/29",
          "2020-02-19",
        ],
        invalid: [
          "",
          "15072002",
          null,
          undefined,
          { year: 2002, month: 7, day: 15 },
          42,
          {
            toString() {
              return "[object Date]";
            },
          }, // faking
          "2020-02-30", // invalid date
          "2019-02-29", // non-leap year
          "2020-04-31", // invalid date
          "2020/03-15", // mixed delimiter
          "-2020-04-19",
          "-2023/05/24",
          "abc-2023/05/24",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
        ],
      });
      test({
        validator: "isDate",
        args: ["DD/MM/YYYY"], // old format for backward compatibility
        valid: ["15-07-2002", "15/07/2002"],
        invalid: [
          "15/7/2002",
          "15-7-2002",
          "15/7/02",
          "15-7-02",
          "15-07/2002",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "15/",
          "15/07",
          "15/07/",
          "15/07/2024/",
        ],
      });
      test({
        validator: "isDate",
        args: [{ format: "DD/MM/YYYY" }],
        valid: ["15-07-2002", "15/07/2002"],
        invalid: [
          "15/7/2002",
          "15-7-2002",
          "15/7/02",
          "15-7-02",
          "15-07/2002",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "15/",
          "15/07",
          "15/07/",
          "15/07/2024/",
        ],
      });
      test({
        validator: "isDate",
        args: [{ format: "DD/MM/YY" }],
        valid: ["15-07-02", "15/07/02"],
        invalid: [
          "15/7/2002",
          "15-7-2002",
          "15/07-02",
          "30/04/--",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "15/",
          "15/07",
          "15/07/",
          "15/07/2024/",
          "15/07/24/",
        ],
      });
      test({
        validator: "isDate",
        args: [{ format: "D/M/YY" }],
        valid: ["5-7-02", "5/7/02"],
        invalid: [
          "5/07/02",
          "15/7/02",
          "15-7-02",
          "5/7-02",
          "3/4/aa",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "15/",
          "15/07",
          "15/07/",
          "15/07/2024/",
          "15/07/24/",
        ],
      });
      test({
        validator: "isDate",
        args: [{ format: "DD/MM/YYYY", strictMode: true }],
        valid: ["15/07/2002"],
        invalid: [
          "15-07-2002",
          "15/7/2002",
          "15-7-2002",
          "15/7/02",
          "15-7-02",
          "15-07/2002",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "15/",
          "15/07",
          "15/07/",
          "15/07/2024/",
          "15/07/24/",
        ],
      });
      test({
        validator: "isDate",
        args: [{ strictMode: true }],
        valid: ["2020/01/15", "2014/02/15", "2014/03/15", "2020/02/29"],
        invalid: [
          "2014-02-15",
          "2020-02-29",
          "15-07/2002",
          new Date(),
          new Date(2014, 2, 15),
          new Date("2014-03-15"),
          "-2020-04-19",
          "-2023/05/24",
          "abc-2023/05/24",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
        ],
      });
      test({
        validator: "isDate",
        args: [{ delimiters: ["/", " "] }],
        valid: [
          new Date(),
          new Date(2014, 2, 15),
          new Date("2014-03-15"),
          "2020/02/29",
          "2020 02 29",
        ],
        invalid: [
          "2020-02-29",
          "",
          "15072002",
          null,
          undefined,
          { year: 2002, month: 7, day: 15 },
          42,
          {
            toString() {
              return "[object Date]";
            },
          },
          "2020/02/30",
          "2019/02/29",
          "2020/04/31",
          "2020/03-15",
          "-2020-04-19",
          "-2023/05/24",
          "abc-2023/05/24",
          "2024",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01-",
          "2024-05-01-abc",
          "2024/",
          "2024/05",
          "2024/05/",
          "2024/05/01/",
          "2024/05/01/abc",
          "2024 05 01 abc",
        ],
      });
      test({
        validator: "isDate",
        args: [{ format: "MM.DD.YYYY", delimiters: ["."], strictMode: true }],
        valid: ["01.15.2020", "02.15.2014", "03.15.2014", "02.29.2020"],
        invalid: [
          "2014-02-15",
          "2020-02-29",
          "15-07/2002",
          new Date(),
          new Date(2014, 2, 15),
          new Date("2014-03-15"),
          "29.02.2020",
          "02.29.2020.20",
          "2024-",
          "2024-05",
          "2024-05-",
          "2024-05-01",
          "-2020-04-19",
          "-2023/05/24",
          "abc-2023/05/24",
          "04.05.2024.",
          "04.05.2024.abc",
          "abc.04.05.2024",
        ],
      });
    });
    it("should validate time", () => {
      test({
        validator: "isTime",
        valid: ["00:00", "23:59", "9:00"],
        invalid: [
          "",
          null,
          undefined,
          0,
          "07:00 PM",
          "23",
          "00:60",
          "00:",
          "01:0 ",
          "001:01",
        ],
      });
      test({
        validator: "isTime",
        args: [{ hourFormat: "hour24", mode: "withSeconds" }],
        valid: ["23:59:59", "00:00:00", "9:50:01"],
        invalid: [
          "",
          null,
          undefined,
          23,
          "01:00:01 PM",
          "13:00:",
          "00",
          "26",
          "00;01",
          "0 :09",
          "59:59:59",
          "24:00:00",
          "00:59:60",
          "99:99:99",
          "009:50:01",
        ],
      });
      test({
        validator: "isTime",
        args: [{ hourFormat: "hour24", mode: "withOptionalSeconds" }],
        valid: ["23:59:59", "00:00:00", "9:50:01", "00:00", "23:59", "9:00"],
        invalid: [
          "",
          null,
          undefined,
          23,
          "01:00:01 PM",
          "13:00:",
          "00",
          "26",
          "00;01",
          "0 :09",
          "59:59:59",
          "24:00:00",
          "00:59:60",
          "99:99:99",
          "009:50:01",
        ],
      });
      test({
        validator: "isTime",
        args: [{ hourFormat: "hour12" }],
        valid: ["12:59 PM", "12:59 AM", "01:00 PM", "01:00 AM", "7:00 AM"],
        invalid: [
          "",
          null,
          undefined,
          0,
          "12:59 MM",
          "12:59 MA",
          "12:59 PA",
          "12:59 A M",
          "13:00 PM",
          "23",
          "00:60",
          "00:",
          "9:00",
          "01:0 ",
          "001:01",
          "12:59:00 PM",
          "12:59:00 A M",
          "12:59:00 ",
        ],
      });
      test({
        validator: "isTime",
        args: [{ hourFormat: "hour12", mode: "withSeconds" }],
        valid: ["12:59:59 PM", "2:34:45 AM", "7:00:00 AM"],
        invalid: [
          "",
          null,
          undefined,
          23,
          "01:00: 1 PM",
          "13:00:",
          "13:00:00 PM",
          "00",
          "26",
          "00;01",
          "0 :09",
          "59:59:59",
          "24:00:00",
          "00:59:60",
          "99:99:99",
          "9:50:01",
          "009:50:01",
        ],
      });
      test({
        validator: "isTime",
        args: [{ hourFormat: "hour12", mode: "withOptionalSeconds" }],
        valid: [
          "12:59:59 PM",
          "2:34:45 AM",
          "7:00:00 AM",
          "12:59 PM",
          "12:59 AM",
          "01:00 PM",
          "01:00 AM",
          "7:00 AM",
        ],
        invalid: [
          "",
          null,
          undefined,
          23,
          "01:00: 1 PM",
          "13:00:",
          "00",
          "26",
          "00;01",
          "0 :09",
          "59:59:59",
          "24:00:00",
          "00:59:60",
          "99:99:99",
          "9:50:01",
          "009:50:01",
        ],
      });
    });
    it("should be valid license plate", () => {
      test({
        validator: "isLicensePlate",
        args: ["es-AR"],
        valid: ["AB 123 CD", "AB123CD", "ABC 123", "ABC123"],
        invalid: [
          "",
          "notalicenseplate",
          "AB-123-CD",
          "ABC-123",
          "AABC 123",
          "AB CDE FG",
          "ABC DEF",
          "12 ABC 34",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["pt-PT"],
        valid: [
          "AA-12-34",
          "12-AA-34",
          "12-34-AA",
          "AA-12-AA",
          "AA\u00b712\u00b734",
          "12\u00b7AB\u00b734",
          "12\u00b734\u00b7AB",
          "AB\u00b734\u00b7AB",
          "AA 12 34",
          "12 AA 34",
          "12 34 AA",
          "AB 12 CD",
          "AA1234",
          "12AA34",
          "1234AA",
          "AB12CD",
        ],
        invalid: [
          "",
          "notalicenseplate",
          "AA-AA-00",
          "00-AA-AA",
          "AA-AA-AA",
          "00-00-00",
          "AA\u00b7AA\u00b700",
          "00\u00b7AA\u00b7AA",
          "AA\u00b7AA\u00b7AA",
          "00\u00b700\u00b700",
          "AA AA 00",
          "00 AA AA",
          "AA AA AA",
          "00 00 00",
          "A1-B2-C3",
          "1A-2B-3C",
          "ABC-1-EF",
          "AB-C1D-EF",
          "AB-C1-DEF",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["de-LI"],
        valid: ["FL 1", "FL 99999", "FL 1337"],
        invalid: ["", "FL 999999", "AB 12345", "FL -1"],
      });
      test({
        validator: "isLicensePlate",
        args: ["de-DE"],
        valid: [
          "M A 1",
          "M A 12",
          "M A 123",
          "M A 1234",
          "M AB 1",
          "M AB 12",
          "M AB 123",
          "M AB 1234",
          "FS A 1",
          "FS A 12",
          "FS A 123",
          "FS A 1234",
          "FS AB 1",
          "FS AB 12",
          "FS AB 123",
          "FS AB 1234",
          "FSAB1234",
          "FS-AB-1234",
          "FS AB 1234 H",
          "FS AB 1234 E",
          "FSAB1234E",
          "FS-AB-1234-E",
          "FS AB-1234-E",
          "FSAB1234 E",
          "FS AB1234E",
          "LRO AB 123",
          "LRO-AB-123-E",
          "LRO-AB-123E",
          "LRO-AB-123 E",
          "LRO-AB-123-H",
          "LRO-AB-123H",
          "LRO-AB-123 H",
        ],
        invalid: [
          "YY AB 123",
          "PAF AB 1234",
          "M ABC 123",
          "M AB 12345",
          "FS AB 1234 A",
          "LRO-AB-1234",
          "HRO ABC 123",
          "HRO ABC 1234",
          "LDK-AB-1234-E",
          "\u00d6HR FA 123D",
          "MZG-AB-123X",
          "OBG-ABD-123",
          "PAF-AB2-123",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["fi-FI"],
        valid: [
          "ABC-123",
          "ABC 123",
          "ABC123",
          "A100",
          "A 100",
          "A-100",
          "C10001",
          "C 10001",
          "C-10001",
          "123-ABC",
          "123 ABC",
          "123ABC",
          "123-A",
          "123 A",
          "123A",
          "199AA",
          "199 AA",
          "199-AA",
        ],
        invalid: [
          " ",
          "A-1",
          "A1A-100",
          "1-A-2",
          "C1234567",
          "A B C 1 2 3",
          "abc-123",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["sq-AL"],
        valid: ["AA 000 AA", "ZZ 999 ZZ"],
        invalid: ["", "AA 0 A", "AAA 00 AAA"],
      });
      test({
        validator: "isLicensePlate",
        args: ["cs-CZ"],
        valid: [
          "ALA4011",
          "4A23000",
          "DICTAT0R",
          "VETERAN",
          "AZKVIZ8",
          "2A45876",
          "DIC-TAT0R",
        ],
        invalid: [
          "",
          "invalidlicenseplate",
          "LN5758898",
          "X-|$|-X",
          "AE0F-OP4",
          "GO0MER",
          "2AAAAAAAA",
          "FS AB 1234 E",
          "GB999 9999 00",
        ],
      });

      test({
        validator: "isLicensePlate",
        args: ["pt-BR"],
        valid: [
          "ABC1234",
          "ABC 1234",
          "ABC-1234",
          "ABC1D23",
          "ABC1K23",
          "ABC1Z23",
          "ABC 1D23",
          "ABC-1D23",
        ],
        invalid: ["", "AA 0 A", "AAA 00 AAA", "ABCD123", "AB12345", "AB123DC"],
      });
      test({
        validator: "isLicensePlate",
        args: ["hu-HU"],
        valid: [
          "AAB-001",
          "AVC-987",
          "KOC-124",
          "JCM-871",
          "AWQ-777",
          "BPO-001",
          "BPI-002",
          "UCO-342",
          "UDO-385",
          "XAO-987",
          "AAI-789",
          "ABI-789",
          "ACI-789",
          "AAO-789",
          "ABO-789",
          "ACO-789",
          "YAA-123",
          "XAA-123",
          "WAA-258",
          "XZZ-784",
          "M123456",
          "CK 12-34",
          "DT 12-34",
          "CD 12-34",
          "HC 12-34",
          "HB 12-34",
          "HK 12-34",
          "MA 12-34",
          "OT 12-34",
          "RR 17-87",
          "CD 124-348",
          "C-C 2021",
          "C-X 2458",
          "X-A 7842",
          "E-72345",
          "Z-07458",
          "S ACF 83",
          "SP 04-68",
        ],
        invalid: [
          "AAA-547",
          "aab-001",
          "AAB 001",
          "AB34",
          "789-LKJ",
          "BBO-987",
          "BBI-987",
          "BWQ-777",
          "BQW-987",
          "BAI-789",
          "BBI-789",
          "BCI-789",
          "BAO-789",
          "BBO-789",
          "BCO-789",
          "ADI-789",
          "ADO-789",
          "KOC-1234",
          "M1234567",
          "W-12345",
          "S BCF 83",
          "X-D 1234",
          "C-D 1234",
          "HU 12-34",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["any"],
        valid: ["FL 1", "FS AB 123"],
        invalid: ["", "FL 999999", "FS AB 1234 A"],
      });
      test({
        validator: "isLicensePlate",
        args: ["asdfasdf"],
        error: ["FL 1", "FS AB 123", "FL 999999", "FS AB 1234 A"],
      });
      test({
        validator: "isLicensePlate",
        args: ["sv-SE"],
        valid: [
          "ABC 123",
          "ABC 12A",
          "ABC123",
          "ABC12A",
          "A WORD",
          "WORD",
          "\u00c5SNA",
          "EN VARG",
          "CERISE",
          "AA",
          "ABCDEFG",
          "\u00c5\u00c4\u00d6",
          "\u00c5\u00c4\u00d6 \u00c5\u00c4\u00d6",
        ],
        invalid: [
          "",
          "    ",
          "IQV 123",
          "IQV123",
          "ABI 12Q",
          "\u00c5\u00c4\u00d6 123",
          "\u00c5\u00c4\u00d6 12A",
          "AB1 A23",
          "AB1 12A",
          "lower",
          "abc 123",
          "abc 12A",
          "abc 12a",
          "AbC 12a",
          "WORDLONGERTHANSEVENCHARACTERS",
          "A",
          "ABC-123",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["en-IN"],
        valid: [
          "MH 04 AD 0001",
          "HR26DQ0001",
          "WB-04-ZU-2001",
          "KL 18 X 5800",
          "DL 4 CAF 4856",
          "KA-41CE-5289",
          "GJ 04-AD 5822",
        ],
        invalid: [
          "mh04ad0045",
          "invalidlicenseplate",
          "4578",
          "",
          "GJ054GH4785",
        ],
      });
      test({
        validator: "isLicensePlate",
        args: ["en-SG"],
        valid: ["SGX 1234 A", "SGX-1234-A", "SGB1234Z"],
        invalid: ["sg1234a", "invalidlicenseplate", "4578", "", "GJ054GH4785"],
      });
    });
    it("should validate VAT numbers", () => {
      test({
        validator: "isVAT",
        args: ["AT"],
        valid: ["ATU12345678", "U12345678"],
        invalid: ["AT 12345678", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["BE"],
        valid: ["BE1234567890", "1234567890"],
        invalid: ["BE 1234567890", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["BG"],
        valid: ["BG1234567890", "1234567890", "BG123456789", "123456789"],
        invalid: ["BG 1234567890", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["HR"],
        valid: ["HR12345678901", "12345678901"],
        invalid: ["HR 12345678901", "1234567890"],
      });
      test({
        validator: "isVAT",
        args: ["CY"],
        valid: ["CY123456789", "123456789"],
        invalid: ["CY 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["CZ"],
        valid: [
          "CZ1234567890",
          "CZ123456789",
          "CZ12345678",
          "1234567890",
          "123456789",
          "12345678",
        ],
        invalid: ["CZ 123456789", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["DK"],
        valid: ["DK12345678", "12345678"],
        invalid: ["DK 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["EE"],
        valid: ["EE123456789", "123456789"],
        invalid: ["EE 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["FI"],
        valid: ["FI12345678", "12345678"],
        invalid: ["FI 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["FR"],
        valid: ["FRAA123456789", "AA123456789"],
        invalid: ["FR AA123456789", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["DE"],
        valid: ["DE123456789", "123456789"],
        invalid: ["DE 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["EL"],
        valid: ["EL123456789", "123456789"],
        invalid: ["EL 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["HU"],
        valid: ["HU12345678", "12345678"],
        invalid: ["HU 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["IE"],
        valid: ["IE1234567AW", "1234567AW"],
        invalid: ["IE 1234567", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["IT"],
        valid: ["IT12345678910", "12345678910"],
        invalid: [
          "IT12345678 910",
          "IT 123456789101",
          "IT123456789101",
          "GB12345678910",
          "IT123456789",
        ],
      });
      test({
        validator: "isVAT",
        args: ["LV"],
        valid: ["LV12345678901", "12345678901"],
        invalid: ["LV 12345678901", "1234567890"],
      });
      test({
        validator: "isVAT",
        args: ["LT"],
        valid: [
          "LT123456789012",
          "123456789012",
          "LT12345678901",
          "12345678901",
          "LT1234567890",
          "1234567890",
          "LT123456789",
          "123456789",
        ],
        invalid: ["LT 123456789012", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["LU"],
        valid: ["LU12345678", "12345678"],
        invalid: ["LU 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["MT"],
        valid: ["MT12345678", "12345678"],
        invalid: ["MT 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["NL"],
        valid: ["NL123456789B10", "123456789B10"],
        invalid: [
          "NL12345678 910",
          "NL 123456789101",
          "NL123456789B1",
          "GB12345678910",
          "NL123456789",
        ],
      });
      test({
        validator: "isVAT",
        args: ["PL"],
        valid: [
          "PL1234567890",
          "1234567890",
          "PL123-456-78-90",
          "123-456-78-90",
          "PL123-45-67-890",
          "123-45-67-890",
        ],
        invalid: ["PL 1234567890", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["PT"],
        valid: ["PT123456789", "123456789"],
        invalid: ["PT 123456789", "000000001"],
      });
      test({
        validator: "isVAT",
        args: ["RO"],
        valid: ["RO1234567890", "1234567890", "RO12", "12"],
        invalid: ["RO 12", "1"],
      });
      test({
        validator: "isVAT",
        args: ["SK"],
        valid: ["SK1234567890", "1234567890"],
        invalid: ["SK 1234567890", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["SI"],
        valid: ["SI12345678", "12345678"],
        invalid: ["SI 12345678", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["ES"],
        valid: ["ESA1234567A", "A1234567A"],
        invalid: ["ES 1234567A", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["SE"],
        valid: ["SE123456789012", "123456789012"],
        invalid: ["SE 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["AL"],
        valid: ["AL123456789A", "123456789A"],
        invalid: ["AL 123456789A", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["MK"],
        valid: ["MK1234567890123", "1234567890123"],
        invalid: ["MK 1234567890123", "123456789012"],
      });
      test({
        validator: "isVAT",
        args: ["AU"],
        valid: [
          "AU53004085616",
          "53004085616",
          "AU65613309809",
          "65613309809",
          "AU34118972998",
          "34118972998",
        ],
        invalid: [
          "AU65613309808",
          "65613309808",
          "AU55613309809",
          "55613309809",
          "AU65613319809",
          "65613319809",
          "AU34117972998",
          "34117972998",
          "AU12345678901",
          "12345678901",
          "AU 12345678901",
          "1234567890",
        ],
      });
      test({
        validator: "isVAT",
        args: ["BY"],
        valid: ["\u0423\u041d\u041f 123456789", "123456789"],
        invalid: ["BY 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["CA"],
        valid: ["CA123456789", "123456789"],
        invalid: ["CA 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["IS"],
        valid: ["IS123456", "12345"],
        invalid: ["IS 12345", "1234"],
      });
      test({
        validator: "isVAT",
        args: ["IN"],
        valid: ["IN123456789012345", "123456789012345"],
        invalid: ["IN 123456789012345", "12345678901234"],
      });
      test({
        validator: "isVAT",
        args: ["ID"],
        valid: [
          "ID123456789012345",
          "123456789012345",
          "ID12.345.678.9-012.345",
          "12.345.678.9-012.345",
        ],
        invalid: ["ID 123456789012345", "12345678901234"],
      });
      test({
        validator: "isVAT",
        args: ["IL"],
        valid: ["IL123456789", "123456789"],
        invalid: ["IL 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["KZ"],
        valid: ["KZ123456789012", "123456789012"],
        invalid: ["KZ 123456789012", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["NZ"],
        valid: ["NZ123456789", "123456789"],
        invalid: ["NZ 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["NG"],
        valid: [
          "NG123456789012",
          "123456789012",
          "NG12345678-9012",
          "12345678-9012",
        ],
        invalid: ["NG 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["NO"],
        valid: ["NO123456789MVA", "123456789MVA"],
        invalid: ["NO 123456789MVA", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["PH"],
        valid: [
          "PH123456789012",
          "123456789012",
          "PH123 456 789 012",
          "123 456 789 012",
        ],
        invalid: ["PH 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["RU"],
        valid: ["RU1234567890", "1234567890", "RU123456789012", "123456789012"],
        invalid: ["RU 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["SM"],
        valid: ["SM12345", "12345"],
        invalid: ["SM 12345", "1234"],
      });
      test({
        validator: "isVAT",
        args: ["SA"],
        valid: ["SA123456789012345", "123456789012345"],
        invalid: ["SA 123456789012345", "12345678901234"],
      });
      test({
        validator: "isVAT",
        args: ["RS"],
        valid: ["RS123456789", "123456789"],
        invalid: ["RS 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["CH"],
        valid: [
          // strictly valid
          "CHE-116.281.710 MWST",
          "CHE-116.281.710 IVA",
          "CHE-116.281.710 TVA",
          // loosely valid presentation variants
          "CHE 116 281 710 IVA", // all separators are spaces
          "CHE-191.398.369MWST", // no space before suffix
          "CHE-116281710 MWST", // no number separators
          "CHE-116281710MWST", // no number separators and no space before suffix
          "CHE105854263MWST", // no separators
          "CHE-116.285.524", // no suffix (vat abbreviation)
          "CHE116281710", // no suffix and separators
          "116.281.710 TVA", // no prefix (CHE, ISO-3166-1 Alpha-3)
          "116281710MWST", // no prefix and separators
          "100.218.485", // no prefix and suffix
          "123456788", // no prefix, separators and suffix
        ],
        invalid: [
          "CH-116.281.710 MWST", // invalid prefix (should be CHE)
          "CHE-116.281 MWST", // invalid number of digits (should be 9)
          "CHE-123.456.789 MWST", // invalid last digit (should match the calculated check-number 8)
          "CHE-123.356.780 MWST", // invalid check-number (there are no swiss UIDs with the calculated check number 10)
          "CH-116.281.710 VAT", // invalid suffix (should be MWST, IVA or TVA)
          "CHE-116/281/710 IVA", // invalid number separators (should be all dots or all spaces)
        ],
      });
      test({
        validator: "isVAT",
        args: ["TR"],
        valid: ["TR1234567890", "1234567890"],
        invalid: ["TR 1234567890", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["UA"],
        valid: ["UA123456789012", "123456789012"],
        invalid: ["UA 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["GB"],
        valid: [
          "GB999 9999 00",
          "GB999 9999 96",
          "GB999999999 999",
          "GBGD000",
          "GBGD499",
          "GBHA500",
          "GBHA999",
        ],
        invalid: [
          "GB999999900",
          "GB999999996",
          "GB999 9999 97",
          "GB999999999999",
          "GB999999999 9999",
          "GB9999999999 999",
          "GBGD 000",
          "GBGD 499",
          "GBHA 500",
          "GBHA 999",
          "GBGD500",
          "GBGD999",
          "GBHA000",
          "GBHA499",
        ],
      });
      test({
        validator: "isVAT",
        args: ["UZ"],
        valid: ["UZ123456789", "123456789"],
        invalid: ["UZ 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["AR"],
        valid: ["AR12345678901", "12345678901"],
        invalid: ["AR 12345678901", "1234567890"],
      });
      test({
        validator: "isVAT",
        args: ["BO"],
        valid: ["BO1234567", "1234567"],
        invalid: ["BO 1234567", "123456"],
      });
      test({
        validator: "isVAT",
        args: ["BR"],
        valid: [
          "BR12.345.678/9012-34",
          "12.345.678/9012-34",
          "BR123.456.789-01",
          "123.456.789-01",
        ],
        invalid: ["BR 12.345.678/9012-34", "12345678901234"],
      });
      test({
        validator: "isVAT",
        args: ["CL"],
        valid: ["CL12345678-9", "12345678-9"],
        invalid: ["CL 12345678-9", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["CO"],
        valid: ["CO1234567890", "1234567890"],
        invalid: ["CO 1234567890", "123456789"],
      });
      test({
        validator: "isVAT",
        args: ["CR"],
        valid: ["CR123456789012", "123456789012", "CR123456789", "123456789"],
        invalid: ["CR 123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["EC"],
        valid: ["EC1234567890123", "1234567890123"],
        invalid: ["EC 1234567890123", "123456789012"],
      });
      test({
        validator: "isVAT",
        args: ["SV"],
        valid: ["SV1234-567890-123-1", "1234-567890-123-1"],
        invalid: ["SV 1234-567890-123-1", "1234567890123"],
      });
      test({
        validator: "isVAT",
        args: ["GT"],
        valid: ["GT1234567-8", "1234567-8"],
        invalid: ["GT 1234567-8", "1234567"],
      });
      test({
        validator: "isVAT",
        args: ["HN"],
        valid: ["HN"],
        invalid: ["HN "],
      });
      test({
        validator: "isVAT",
        args: ["MX"],
        valid: [
          "MXABCD123456EFG",
          "ABCD123456EFG",
          "MXABC123456DEF",
          "ABC123456DEF",
        ],
        invalid: ["MX ABC123456EFG", "123456"],
      });
      test({
        validator: "isVAT",
        args: ["NI"],
        valid: ["NI123-456789-0123A", "123-456789-0123A"],
        invalid: ["NI 123-456789-0123A", "1234567890123"],
      });
      test({
        validator: "isVAT",
        args: ["PA"],
        valid: ["PA"],
        invalid: ["PA "],
      });
      test({
        validator: "isVAT",
        args: ["PY"],
        valid: ["PY12345678-9", "12345678-9", "PY123456-7", "123456-7"],
        invalid: ["PY 123456-7", "123456"],
      });
      test({
        validator: "isVAT",
        args: ["PE"],
        valid: ["PE12345678901", "12345678901"],
        invalid: ["PE 12345678901", "1234567890"],
      });
      test({
        validator: "isVAT",
        args: ["DO"],
        valid: [
          "DO12345678901",
          "12345678901",
          "DO123-4567890-1",
          "123-4567890-1",
          "DO123456789",
          "123456789",
          "DO1-23-45678-9",
          "1-23-45678-9",
        ],
        invalid: ["DO 12345678901", "1234567890"],
      });
      test({
        validator: "isVAT",
        args: ["UY"],
        valid: ["UY123456789012", "123456789012"],
        invalid: ["UY 123456789012", "12345678901"],
      });
      test({
        validator: "isVAT",
        args: ["VE"],
        valid: [
          "VEJ-123456789",
          "J-123456789",
          "VEJ-12345678-9",
          "J-12345678-9",
        ],
        invalid: ["VE J-123456789", "12345678"],
      });
      test({
        validator: "isVAT",
        args: ["invalidCountryCode"],
        error: ["GB999 9999 00"],
      });
    });
    it("should validate mailto URI", () => {
      test({
        validator: "isMailtoURI",
        valid: [
          "mailto:?subject=something&cc=valid@mail.com",
          "mailto:?subject=something&cc=valid@mail.com,another@mail.com,",
          "mailto:?subject=something&bcc=valid@mail.com",
          "mailto:?subject=something&bcc=valid@mail.com,another@mail.com",
          "mailto:?bcc=valid@mail.com,another@mail.com",
          "mailto:?cc=valid@mail.com,another@mail.com",
          "mailto:?cc=valid@mail.com",
          "mailto:?bcc=valid@mail.com",
          "mailto:?subject=something&body=something else",
          "mailto:?subject=something&body=something else&cc=hello@mail.com,another@mail.com",
          "mailto:?subject=something&body=something else&bcc=hello@mail.com,another@mail.com",
          "mailto:?subject=something&body=something else&cc=something@mail.com&bcc=hello@mail.com,another@mail.com",
          "mailto:hello@mail.com",
          "mailto:info@mail.com?",
          "mailto:hey@mail.com?subject=something",
          "mailto:info@mail.com?subject=something&cc=valid@mail.com",
          "mailto:info@mail.com?subject=something&cc=valid@mail.com,another@mail.com,",
          "mailto:info@mail.com?subject=something&bcc=valid@mail.com",
          "mailto:info@mail.com?subject=something&bcc=valid@mail.com,another@mail.com",
          "mailto:info@mail.com?bcc=valid@mail.com,another@mail.com",
          "mailto:info@mail.com?cc=valid@mail.com,another@mail.com",
          "mailto:info@mail.com?cc=valid@mail.com",
          "mailto:info@mail.com?bcc=valid@mail.com&",
          "mailto:info@mail.com?subject=something&body=something else",
          "mailto:info@mail.com?subject=something&body=something else&cc=hello@mail.com,another@mail.com",
          "mailto:info@mail.com?subject=something&body=something else&bcc=hello@mail.com,another@mail.com",
          "mailto:info@mail.com?subject=something&body=something else&cc=something@mail.com&bcc=hello@mail.com,another@mail.com",
          "mailto:",
        ],
        invalid: [
          "",
          "something",
          "valid@gmail.com",
          "mailto:?subject=okay&subject=444",
          "mailto:?subject=something&wrong=888",
          "mailto:somename@\uff47\uff4d\uff41\uff49\uff4c.com",
          "mailto:hello@world.com?cc=somename@\uff47\uff4d\uff41\uff49\uff4c.com",
          "mailto:hello@world.com?bcc=somename@\uff47\uff4d\uff41\uff49\uff4c.com",
          "mailto:hello@world.com?bcc=somename@\uff47\uff4d\uff41\uff49\uff4c.com&bcc",
          "mailto:valid@gmail.com?subject=anything&body=nothing&cc=&bcc=&key=",
          "mailto:hello@world.com?cc=somename",
          "mailto:somename",
          "mailto:info@mail.com?subject=something&body=something else&cc=something@mail.com&bcc=hello@mail.com,another@mail.com&",
          "mailto:?subject=something&body=something else&cc=something@mail.com&bcc=hello@mail.com,another@mail.com&",
        ],
      });
    });
  });

  describe("isAfter", () => {
    it("should validate dates against a start date", () => {
      test({
        validator: "isAfter",
        args: [{ comparisonDate: "2011-08-03" }],
        valid: ["2011-08-04", new Date(2011, 8, 10).toString()],
        invalid: ["2010-07-02", "2011-08-03", new Date(0).toString(), "foo"],
      });

      test({
        validator: "isAfter",
        valid: ["2100-08-04", new Date(Date.now() + 86400000).toString()],
        invalid: ["2010-07-02", new Date(0).toString()],
      });

      test({
        validator: "isAfter",
        args: [{ comparisonDate: "2011-08-03" }],
        valid: ["2015-09-17"],
        invalid: ["invalid date"],
      });

      test({
        validator: "isAfter",
        args: [{ comparisonDate: "invalid date" }],
        invalid: ["invalid date", "2015-09-17"],
      });
      test({
        validator: "isAfter",
        args: [], // will fall back to the current date
        valid: ["2100-08-04", new Date(Date.now() + 86400000).toString()],
      });
      test({
        validator: "isAfter",
        args: [undefined], // will fall back to the current date
        valid: ["2100-08-04", new Date(Date.now() + 86400000).toString()],
      });
      test({
        validator: "isAfter",
        args: [{ comparisonDate: undefined }], // will fall back to the current date
        valid: ["2100-08-04", new Date(Date.now() + 86400000).toString()],
      });
    });

    describe("(legacy syntax)", () => {
      it("should validate dates against a start date", () => {
        test({
          validator: "isAfter",
          args: ["2011-08-03"],
          valid: ["2011-08-04", new Date(2011, 8, 10).toString()],
          invalid: ["2010-07-02", "2011-08-03", new Date(0).toString(), "foo"],
        });

        test({
          validator: "isAfter",
          valid: ["2100-08-04", new Date(Date.now() + 86400000).toString()],
          invalid: ["2010-07-02", new Date(0).toString()],
        });

        test({
          validator: "isAfter",
          args: ["2011-08-03"],
          valid: ["2015-09-17"],
          invalid: ["invalid date"],
        });

        test({
          validator: "isAfter",
          args: ["invalid date"],
          invalid: ["invalid date", "2015-09-17"],
        });
      });
    });
  });

  describe("isBase64", () => {
    it("should validate base64 strings with default options", () => {
      test({
        validator: "isBase64",
        valid: [
          "",
          "Zg==",
          "Zm8=",
          "Zm9v",
          "Zm9vYg==",
          "Zm9vYmE=",
          "Zm9vYmFy",
          "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZWxpdC4=",
          "Vml2YW11cyBmZXJtZW50dW0gc2VtcGVyIHBvcnRhLg==",
          "U3VzcGVuZGlzc2UgbGVjdHVzIGxlbw==",
          "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuMPNS1Ufof9EW/M98FNw" +
            "UAKrwflsqVxaxQjBQnHQmiI7Vac40t8x7pIb8gLGV6wL7sBTJiPovJ0V7y7oc0Ye" +
            "rhKh0Rm4skP2z/jHwwZICgGzBvA0rH8xlhUiTvcwDCJ0kc+fh35hNt8srZQM4619" +
            "FTgB66Xmp4EtVyhpQV+t02g6NzK72oZI0vnAvqhpkxLeLiMCyrI416wHm5Tkukhx" +
            "QmcL2a6hNOyu0ixX/x2kSFXApEnVrJ+/IxGyfyw8kf4N2IZpW5nEP847lpfj0SZZ" +
            "Fwrd1mnfnDbYohX2zRptLy2ZUn06Qo9pkG5ntvFEPo9bfZeULtjYzIl6K8gJ2uGZ" +
            "HQIDAQAB",
        ],
        invalid: [
          "12345",
          "Vml2YW11cyBmZXJtZtesting123",
          "Zg=",
          "Z===",
          "Zm=8",
          "=m9vYg==",
          "Zm9vYmFy====",
        ],
      });

      test({
        validator: "isBase64",
        args: [{ urlSafe: true }],
        valid: [
          "",
          "bGFkaWVzIGFuZCBnZW50bGVtZW4sIHdlIGFyZSBmbG9hdGluZyBpbiBzcGFjZQ",
          "1234",
          "bXVtLW5ldmVyLXByb3Vk",
          "PDw_Pz8-Pg",
          "VGhpcyBpcyBhbiBlbmNvZGVkIHN0cmluZw",
        ],
        invalid: [
          " AA",
          "\tAA",
          "\rAA",
          "\nAA",
          "This+isa/bad+base64Url==",
          "0K3RgtC+INC30LDQutC+0LTQuNGA0L7QstCw0L3QvdCw0Y8g0YHRgtGA0L7QutCw",
        ],
        error: [null, undefined, {}, [], 42],
      });

      // for (let i = 0, str = '', encoded; i < 1000; i++) {
      //   str += String.fromCharCode(Math.random() * 26 | 97); // eslint-disable-line no-bitwise
      //   encoded = Buffer.from(str).toString('base64');
      //   if (!validatorjs.isBase64(encoded)) {
      //     let msg = format('validator.isBase64() failed with "%s"', encoded);
      //     throw new Error(msg);
      //   }
      // }
    });

    it("should validate standard Base64 with padding", () => {
      test({
        validator: "isBase64",
        args: [{ urlSafe: false, padding: true }],
        valid: [
          "",
          "TWFu",
          "TWE=",
          "TQ==",
          "SGVsbG8=",
          "U29mdHdhcmU=",
          "YW55IGNhcm5hbCBwbGVhc3VyZS4=",
        ],
        invalid: [
          "TWF",
          "TWE===",
          "SGVsbG8@",
          "SGVsbG8===",
          "SGVsb G8=",
          "====",
        ],
      });
    });

    it("should validate standard Base64 without padding", () => {
      test({
        validator: "isBase64",
        args: [{ urlSafe: false, padding: false }],
        valid: [
          "",
          "TWFu",
          "TWE",
          "TQ",
          "SGVsbG8",
          "U29mdHdhcmU",
          "YW55IGNhcm5hbCBwbGVhc3VyZS4",
        ],
        invalid: [
          "TWE=",
          "TQ===",
          "SGVsbG8@",
          "SGVsbG8===",
          "SGVsb G8",
          "====",
        ],
      });
    });

    it("should validate Base64url with padding", () => {
      test({
        validator: "isBase64",
        args: [{ urlSafe: true, padding: true }],
        valid: [
          "",
          "SGVsbG8=",
          "U29mdHdhcmU=",
          "YW55IGNhcm5hbCBwbGVhc3VyZS4=",
          "SGVsbG8-",
          "SGVsbG8_",
        ],
        invalid: ["SGVsbG8===", "SGVsbG8@", "SGVsb G8=", "===="],
      });
    });

    it("should validate Base64url without padding", () => {
      test({
        validator: "isBase64",
        args: [{ urlSafe: true, padding: false }],
        valid: [
          "",
          "SGVsbG8",
          "U29mdHdhcmU",
          "YW55IGNhcm5hbCBwbGVhc3VyZS4",
          "SGVsbG8-",
          "SGVsbG8_",
        ],
        invalid: ["SGVsbG8=", "SGVsbG8===", "SGVsbG8@", "SGVsb G8", "===="],
      });
    });

    it("should handle mixed cases correctly", () => {
      test({
        validator: "isBase64",
        args: [{ urlSafe: false, padding: true }],
        valid: ["", "TWFu", "TWE=", "TQ=="],
        invalid: ["TWE", "TQ=", "TQ==="],
      });

      test({
        validator: "isBase64",
        args: [{ urlSafe: true, padding: false }],
        valid: ["", "SGVsbG8", "SGVsbG8-", "SGVsbG8_"],
        invalid: ["SGVsbG8=", "SGVsbG8@", "SGVsb G8"],
      });
    });
  });

  describe("isBefore", () => {
    describe("should validate dates a given end date", () => {
      describe("new syntax", () => {
        test({
          validator: "isBefore",
          args: [{ comparisonDate: "08/04/2011" }],
          valid: ["2010-07-02", "2010-08-04", new Date(0).toString()],
          invalid: ["08/04/2011", new Date(2011, 9, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: [{ comparisonDate: new Date(2011, 7, 4).toString() }],
          valid: ["2010-07-02", "2010-08-04", new Date(0).toString()],
          invalid: ["08/04/2011", new Date(2011, 9, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: [{ comparisonDate: "2011-08-03" }],
          valid: ["1999-12-31"],
          invalid: ["invalid date"],
        });
        test({
          validator: "isBefore",
          args: [{ comparisonDate: "invalid date" }],
          invalid: ["invalid date", "1999-12-31"],
        });
      });

      describe("legacy syntax", () => {
        test({
          validator: "isBefore",
          args: ["08/04/2011"],
          valid: ["2010-07-02", "2010-08-04", new Date(0).toString()],
          invalid: ["08/04/2011", new Date(2011, 9, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: [new Date(2011, 7, 4).toString()],
          valid: ["2010-07-02", "2010-08-04", new Date(0).toString()],
          invalid: ["08/04/2011", new Date(2011, 9, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: ["2011-08-03"],
          valid: ["1999-12-31"],
          invalid: ["invalid date"],
        });
        test({
          validator: "isBefore",
          args: ["invalid date"],
          invalid: ["invalid date", "1999-12-31"],
        });
      });
    });

    describe("should validate dates a default end date", () => {
      describe("new syntax", () => {
        test({
          validator: "isBefore",
          valid: [
            "2000-08-04",
            new Date(0).toString(),
            new Date(Date.now() - 86400000).toString(),
          ],
          invalid: ["2100-07-02", new Date(2217, 10, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: undefined, // will fall back to the current date
          valid: ["1999-06-07"],
        });
        test({
          validator: "isBefore",
          args: [], // will fall back to the current date
          valid: ["1999-06-07"],
        });
        test({
          validator: "isBefore",
          args: [undefined], // will fall back to the current date
          valid: ["1999-06-07"],
        });
        test({
          validator: "isBefore",
          args: [{ comparisonDate: undefined }], // will fall back to the current date
          valid: ["1999-06-07"],
        });
      });

      describe("legacy syntax", () => {
        test({
          validator: "isBefore",
          valid: [
            "2000-08-04",
            new Date(0).toString(),
            new Date(Date.now() - 86400000).toString(),
          ],
          invalid: ["2100-07-02", new Date(2217, 10, 10).toString()],
        });
        test({
          validator: "isBefore",
          args: undefined, // will fall back to the current date
          valid: ["1999-06-07"],
        });
        test({
          validator: "isBefore",
          args: [], // will fall back to the current date
          valid: ["1999-06-07"],
        });
        test({
          validator: "isBefore",
          args: [undefined], // will fall back to the current date
          valid: ["1999-06-07"],
        });
      });
    });
  });

  describe("isFQDN", () => {
    it("should validate domain names.", () => {
      test({
        validator: "isFQDN",
        args: [],
        valid: ["google.com"],
        invalid: ["google.l33t"],
      });
      test({
        validator: "isFQDN",
        args: [{ allow_numeric_tld: true }],
        valid: ["google.com", "google.l33t"],
        invalid: [],
      });
    });
  });

  describe("isIP", () => {
    it("should validate IP addresses", () => {
      test({
        validator: "isIP",
        valid: [
          "127.0.0.1",
          "0.0.0.0",
          "255.255.255.255",
          "1.2.3.4",
          "::1",
          "2001:db8:0000:1:1:1:1:1",
          "2001:db8:3:4::192.0.2.33",
          "2001:41d0:2:a141::1",
          "::ffff:127.0.0.1",
          "::0000",
          "0000::",
          "1::",
          "1111:1:1:1:1:1:1:1",
          "fe80::a6db:30ff:fe98:e946",
          "::",
          "::8",
          "::ffff:127.0.0.1",
          "::ffff:255.255.255.255",
          "::ffff:0:255.255.255.255",
          "::2:3:4:5:6:7:8",
          "::255.255.255.255",
          "0:0:0:0:0:ffff:127.0.0.1",
          "1:2:3:4:5:6:7::",
          "1:2:3:4:5:6::8",
          "1::7:8",
          "1:2:3:4:5::7:8",
          "1:2:3:4:5::8",
          "1::6:7:8",
          "1:2:3:4::6:7:8",
          "1:2:3:4::8",
          "1::5:6:7:8",
          "1:2:3::5:6:7:8",
          "1:2:3::8",
          "1::4:5:6:7:8",
          "1:2::4:5:6:7:8",
          "1:2::8",
          "1::3:4:5:6:7:8",
          "1::8",
          "fe80::7:8%eth0",
          "fe80::7:8%1",
          "64:ff9b::192.0.2.33",
          "0:0:0:0:0:0:10.0.0.1",
        ],
        invalid: [
          "abc",
          "256.0.0.0",
          "0.0.0.256",
          "26.0.0.256",
          "0200.200.200.200",
          "200.0200.200.200",
          "200.200.0200.200",
          "200.200.200.0200",
          "::banana",
          "banana::",
          "::1banana",
          "::1::",
          "1:",
          ":1",
          ":1:1:1::2",
          "1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1",
          "::11111",
          "11111:1:1:1:1:1:1:1",
          "2001:db8:0000:1:1:1:1::1",
          "0:0:0:0:0:0:ffff:127.0.0.1",
          "0:0:0:0:ffff:127.0.0.1",
          "BC:e4d5:c:e7b9::%40i0nccymtl9cwfKo.5vaeXLSGRMe:EDh2qs5wkhnPws5xQKqafjfAMm6wGFCJ.bVFsZfb",
          "1dC:0DF8:62D:3AC::%KTatXocjaFVioS0RTNQl4mA.V151o0RSy.JIu-D-D8.d3171ZWsSJ7PK4YjkJCRN0F",
        ],
      });

      test({
        validator: "isIP",
        args: [{ version: "invalid version" }],
        valid: [],
        invalid: ["127.0.0.1", "0.0.0.0", "255.255.255.255", "1.2.3.4"],
      });

      test({
        validator: "isIP",
        args: [{ version: null }],
        valid: ["127.0.0.1", "0.0.0.0", "255.255.255.255", "1.2.3.4"],
      });

      test({
        validator: "isIP",
        args: [{ version: undefined }],
        valid: ["127.0.0.1", "0.0.0.0", "255.255.255.255", "1.2.3.4"],
      });

      test({
        validator: "isIP",
        args: [{ version: 4 }],
        valid: [
          "127.0.0.1",
          "0.0.0.0",
          "255.255.255.255",
          "1.2.3.4",
          "255.0.0.1",
          "0.0.1.1",
        ],
        invalid: [
          "::1",
          "2001:db8:0000:1:1:1:1:1",
          "::ffff:127.0.0.1",
          "137.132.10.01",
          "0.256.0.256",
          "255.256.255.256",
        ],
      });

      test({
        validator: "isIP",
        args: [{ version: 6 }],
        valid: [
          "::1",
          "2001:db8:0000:1:1:1:1:1",
          "::ffff:127.0.0.1",
          "fe80::1234%1",
          "ff08::9abc%10",
          "ff08::9abc%interface10",
          "ff02::5678%pvc1.3",
        ],
        invalid: [
          "127.0.0.1",
          "0.0.0.0",
          "255.255.255.255",
          "1.2.3.4",
          "::ffff:287.0.0.1",
          "%",
          "fe80::1234%",
          "fe80::1234%1%3%4",
          "fe80%fe80%",
        ],
      });

      test({
        validator: "isIP",
        args: [{ version: 10 }],
        valid: [],
        invalid: [
          "127.0.0.1",
          "0.0.0.0",
          "255.255.255.255",
          "1.2.3.4",
          "::1",
          "2001:db8:0000:1:1:1:1:1",
        ],
      });
    });

    describe("legacy syntax", () => {
      it("should validate IP addresses", () => {
        test({
          validator: "isIP",
          valid: [
            "127.0.0.1",
            "0.0.0.0",
            "255.255.255.255",
            "1.2.3.4",
            "::1",
            "2001:db8:0000:1:1:1:1:1",
            "2001:db8:3:4::192.0.2.33",
            "2001:41d0:2:a141::1",
            "::ffff:127.0.0.1",
            "::0000",
            "0000::",
            "1::",
            "1111:1:1:1:1:1:1:1",
            "fe80::a6db:30ff:fe98:e946",
            "::",
            "::8",
            "::ffff:127.0.0.1",
            "::ffff:255.255.255.255",
            "::ffff:0:255.255.255.255",
            "::2:3:4:5:6:7:8",
            "::255.255.255.255",
            "0:0:0:0:0:ffff:127.0.0.1",
            "1:2:3:4:5:6:7::",
            "1:2:3:4:5:6::8",
            "1::7:8",
            "1:2:3:4:5::7:8",
            "1:2:3:4:5::8",
            "1::6:7:8",
            "1:2:3:4::6:7:8",
            "1:2:3:4::8",
            "1::5:6:7:8",
            "1:2:3::5:6:7:8",
            "1:2:3::8",
            "1::4:5:6:7:8",
            "1:2::4:5:6:7:8",
            "1:2::8",
            "1::3:4:5:6:7:8",
            "1::8",
            "fe80::7:8%eth0",
            "fe80::7:8%1",
            "64:ff9b::192.0.2.33",
            "0:0:0:0:0:0:10.0.0.1",
          ],
          invalid: [
            "abc",
            "256.0.0.0",
            "0.0.0.256",
            "26.0.0.256",
            "0200.200.200.200",
            "200.0200.200.200",
            "200.200.0200.200",
            "200.200.200.0200",
            "::banana",
            "banana::",
            "::1banana",
            "::1::",
            "1:",
            ":1",
            ":1:1:1::2",
            "1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1",
            "::11111",
            "11111:1:1:1:1:1:1:1",
            "2001:db8:0000:1:1:1:1::1",
            "0:0:0:0:0:0:ffff:127.0.0.1",
            "0:0:0:0:ffff:127.0.0.1",
          ],
        });
        test({
          validator: "isIP",
          args: [4],
          valid: [
            "127.0.0.1",
            "0.0.0.0",
            "255.255.255.255",
            "1.2.3.4",
            "255.0.0.1",
            "0.0.1.1",
          ],
          invalid: [
            "::1",
            "2001:db8:0000:1:1:1:1:1",
            "::ffff:127.0.0.1",
            "137.132.10.01",
            "0.256.0.256",
            "255.256.255.256",
          ],
        });
        test({
          validator: "isIP",
          args: [6],
          valid: [
            "::1",
            "2001:db8:0000:1:1:1:1:1",
            "::ffff:127.0.0.1",
            "fe80::1234%1",
            "ff08::9abc%10",
            "ff08::9abc%interface10",
            "ff02::5678%pvc1.3",
          ],
          invalid: [
            "127.0.0.1",
            "0.0.0.0",
            "255.255.255.255",
            "1.2.3.4",
            "::ffff:287.0.0.1",
            "%",
            "fe80::1234%",
            "fe80::1234%1%3%4",
            "fe80%fe80%",
          ],
        });
        test({
          validator: "isIP",
          args: [10],
          valid: [],
          invalid: [
            "127.0.0.1",
            "0.0.0.0",
            "255.255.255.255",
            "1.2.3.4",
            "::1",
            "2001:db8:0000:1:1:1:1:1",
          ],
        });
      });
    });
  });
  describe("isISBN", () => {
    it("should validate ISBNs", () => {
      test({
        validator: "isISBN",
        args: [{ version: 10 }],
        valid: [
          "3836221195",
          "3-8362-2119-5",
          "3 8362 2119 5",
          "1617290858",
          "1-61729-085-8",
          "1 61729 085-8",
          "0007269706",
          "0-00-726970-6",
          "0 00 726970 6",
          "3423214120",
          "3-423-21412-0",
          "3 423 21412 0",
          "340101319X",
          "3-401-01319-X",
          "3 401 01319 X",
        ],
        invalid: [
          "3423214121",
          "3-423-21412-1",
          "3 423 21412 1",
          "978-3836221191",
          "9783836221191",
          "123456789a",
          "foo",
          "",
        ],
      });
      test({
        validator: "isISBN",
        args: [{ version: 13 }],
        valid: [
          "9783836221191",
          "978-3-8362-2119-1",
          "978 3 8362 2119 1",
          "9783401013190",
          "978-3401013190",
          "978 3401013190",
          "9784873113685",
          "978-4-87311-368-5",
          "978 4 87311 368 5",
        ],
        invalid: [
          "9783836221190",
          "978-3-8362-2119-0",
          "978 3 8362 2119 0",
          "3836221195",
          "3-8362-2119-5",
          "3 8362 2119 5",
          "01234567890ab",
          "foo",
          "",
        ],
      });
      test({
        validator: "isISBN",
        valid: ["340101319X", "9784873113685"],
        invalid: ["3423214121", "9783836221190"],
      });
      test({
        validator: "isISBN",
        args: [{ version: "foo" }],
        invalid: ["340101319X", "9784873113685"],
      });
    });

    describe("(legacy syntax)", () => {
      it("should validate ISBNs", () => {
        test({
          validator: "isISBN",
          args: [10],
          valid: [
            "3836221195",
            "3-8362-2119-5",
            "3 8362 2119 5",
            "1617290858",
            "1-61729-085-8",
            "1 61729 085-8",
            "0007269706",
            "0-00-726970-6",
            "0 00 726970 6",
            "3423214120",
            "3-423-21412-0",
            "3 423 21412 0",
            "340101319X",
            "3-401-01319-X",
            "3 401 01319 X",
          ],
          invalid: [
            "3423214121",
            "3-423-21412-1",
            "3 423 21412 1",
            "978-3836221191",
            "9783836221191",
            "123456789a",
            "foo",
            "",
          ],
        });
        test({
          validator: "isISBN",
          args: [13],
          valid: [
            "9783836221191",
            "978-3-8362-2119-1",
            "978 3 8362 2119 1",
            "9783401013190",
            "978-3401013190",
            "978 3401013190",
            "9784873113685",
            "978-4-87311-368-5",
            "978 4 87311 368 5",
          ],
          invalid: [
            "9783836221190",
            "978-3-8362-2119-0",
            "978 3 8362 2119 0",
            "3836221195",
            "3-8362-2119-5",
            "3 8362 2119 5",
            "01234567890ab",
            "foo",
            "",
          ],
        });
        test({
          validator: "isISBN",
          valid: ["340101319X", "9784873113685"],
          invalid: ["3423214121", "9783836221190"],
        });
        test({
          validator: "isISBN",
          args: ["foo"],
          invalid: ["340101319X", "9784873113685"],
        });
      });
    });
  });

  return assertionCount;
}

})();

ValidatorJSBenchmark = __webpack_exports__;
/******/ })()
;
//# sourceMappingURL=bundle.es6.js.map