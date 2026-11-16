//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(fn, type) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
}

// Bug: Multiple issues in ecmaReferenceYear and calendarDateFromFields for non-ISO calendars:
// 1. Non-lunisolar calendars (gregory, buddhist, japanese, roc, coptic, ethiopic, etc.)
//    accepted leap month codes (M01L etc.) instead of throwing RangeError.
// 2. ecmaReferenceYear tables had errors:
//    - Chinese M01L/M12L: wrong year (1898) → should be UseRegularIfConstrain
//    - Chinese month 11 bigDay: wrong condition order (day>26 checked before bigDay)
//    - Ethiopic: used Coptic AM years instead of Ethiopic years (+276 offset)
//    - Hebrew: only M05L (Adar I) valid; others should throw MonthNotInCalendar
//    - Islamic-civil/tbla: used UmmAlQura table instead of Tabular table
// 3. calendarDateFromFields Reject validation: compared UCAL_MONTH+1 and IS_LEAP_MONTH
//    instead of using computeMonthCode for lunisolar calendars (Hebrew M05L gave wrong slot).

// ── Non-lunisolar calendars reject ALL leap month codes ──────────────────
for (const cal of ["gregory", "buddhist", "japanese", "roc", "coptic", "ethiopic",
                   "ethioaa", "persian", "indian"]) {
    shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:cal}), RangeError);
    shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M06L", day:1, calendar:cal}), RangeError);
}

// ── Chinese/Dangi: M01L and M12L use constrain fallback ──────────────────
// M01L constrain → use M01 reference year. M01L reject → throw.
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"chinese"}).toString(), "1972-02-15[u-ca=chinese]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01L", day:30, calendar:"chinese"}).toString(), "1970-03-07[u-ca=chinese]");
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"chinese"}, {overflow:"reject"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M12L", day:1, calendar:"chinese"}).toString(), "1972-01-16[u-ca=chinese]");
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M12L", day:1, calendar:"chinese"}, {overflow:"reject"}), RangeError);

// Valid Chinese leap months work in both modes
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M04L", day:1, calendar:"chinese"}).toString(), "1963-05-23[u-ca=chinese]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M04L", day:1, calendar:"chinese"}, {overflow:"reject"}).toString(), "1963-05-23[u-ca=chinese]");

// ── Chinese month 11 bigDay: year 1969, not 1971 ──────────────────────────
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M11", day:30, calendar:"chinese"}).toString(), "1970-01-07[u-ca=chinese]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M11", day:27, calendar:"chinese"}).toString(), "1972-01-13[u-ca=chinese]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M11", day:1, calendar:"chinese"}).toString(), "1972-12-06[u-ca=chinese]");

// ── Ethiopic: uses year offset +276 from Coptic ───────────────────────────
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"ethiopic"}).toString(), "1972-09-11[u-ca=ethiopic]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"coptic"}).toString(), "1972-09-11[u-ca=coptic]");

// ── Hebrew: M05L (Adar I) only valid leap month ────────────────────────────
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M05L", day:1, calendar:"hebrew"}).toString(), "1970-02-07[u-ca=hebrew]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M05L", day:1, calendar:"hebrew"}, {overflow:"reject"}).toString(), "1970-02-07[u-ca=hebrew]");
// Hebrew M01L, M02L, etc. don't exist in Hebrew calendar
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"hebrew"}), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M07L", day:1, calendar:"hebrew"}), RangeError);

// ── Islamic-civil/tbla: use tabular table, not UmmAlQura ─────────────────
// Tabular: month 2 bigDay (day=30) uses year 1392; UmmAlQura uses 1390
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M02", day:30, calendar:"islamic-civil"}).toString(), "1972-04-14[u-ca=islamic-civil]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M02", day:30, calendar:"islamic-tbla"}).toString(), "1972-04-13[u-ca=islamic-tbla]");
// UmmAlQura M02 day=30 → year 1390
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M02", day:29, calendar:"islamic-civil"}).toString(), "1972-04-14[u-ca=islamic-civil]");

// ── Dangi (Chinese alias): same lunisolar behaviour as Chinese ─────────────
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"dangi"}).toString(), "1972-02-15[u-ca=dangi]");
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"dangi"}, {overflow:"reject"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M05L", day:1, calendar:"dangi"}).toString(), "1971-06-23[u-ca=dangi]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"dangi"}).toString(), "1972-02-15[u-ca=dangi]");

// ── Persian (non-lunisolar): reject all leap month codes ──────────────────
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"persian"}), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M07L", day:1, calendar:"persian"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"persian"}).toString(), "1972-03-21[u-ca=persian]");
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M07", day:30, calendar:"persian"}).toString(), "1972-10-22[u-ca=persian]");

// ── Islamic-umalqura (non-lunisolar for month codes): reject leap month codes ─
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"islamic-umalqura"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"islamic-umalqura"}).toString(), "1972-02-16[u-ca=islamic-umalqura]");

// ── roc, japanese (non-lunisolar): reject leap month codes ───────────────
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"roc"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"roc"}).toString(), "1972-01-01[u-ca=roc]");
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"japanese"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"japanese"}).toString(), "1972-01-01[u-ca=japanese]");

// ── Indian (non-lunisolar): reject leap month codes ───────────────────────
shouldThrow(() => Temporal.PlainMonthDay.from({monthCode:"M01L", day:1, calendar:"indian"}), RangeError);
shouldBe(Temporal.PlainMonthDay.from({monthCode:"M01", day:1, calendar:"indian"}).toString(), "1972-03-21[u-ca=indian]");