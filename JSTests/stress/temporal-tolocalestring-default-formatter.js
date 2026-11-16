//@ requireOptions("--useTemporal=1")
// Temporal.*.prototype.toLocaleString with undefined locales/options uses the
// per-globalObject default IntlDateTimeFormat (shared with Date.prototype.toLocale*String).
// Verify that the cached formatter path produces results identical to the
// fresh-formatter path and stays consistent across repeated and interleaved calls.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg ?? "shouldBe"}: expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType, msg) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`${msg ?? "shouldThrow"}: expected ${errorType.name} but got ${error}`);
}

const plainDate = new Temporal.PlainDate(2024, 1, 15);
const plainTime = new Temporal.PlainTime(10, 30, 45);
const plainDateTime = new Temporal.PlainDateTime(2024, 1, 15, 10, 30, 45);
const plainYearMonth = new Temporal.PlainYearMonth(2024, 1);
const plainMonthDay = new Temporal.PlainMonthDay(1, 15);
const instant = new Temporal.Instant(1705314645000000000n); // 2024-01-15T10:30:45Z

// PlainDate, PlainTime, PlainDateTime, and Instant format successfully with the
// default formatter regardless of the locale's default calendar.
const formattable = [plainDate, plainTime, plainDateTime, instant];

// 1. Repeated calls on the cached default formatter must produce identical, non-empty results.
for (const object of formattable) {
    const tag = object[Symbol.toStringTag];
    const first = object.toLocaleString();
    shouldBe(typeof first, "string", `${tag} returns a string`);
    shouldBe(first.length > 0, true, `${tag} returns a non-empty string`);
    for (let i = 0; i < 100; ++i)
        shouldBe(object.toLocaleString(), first, `${tag} repeated call ${i}`);
}

// 2. The cached formatter must produce the same result as a freshly created formatter.
// Passing an empty options object forces the fresh-formatter path with identical effective options.
for (const object of formattable) {
    const tag = object[Symbol.toStringTag];
    shouldBe(object.toLocaleString(), object.toLocaleString(undefined, {}), `${tag} cached vs fresh`);
}

// 3. PlainYearMonth/PlainMonthDay throw RangeError when the formatter's calendar (locale default)
// does not match the object's calendar (iso8601). The cached path must throw the same way,
// consistently, on every call.
for (const object of [plainYearMonth, plainMonthDay]) {
    const tag = object[Symbol.toStringTag];
    if (new Intl.DateTimeFormat().resolvedOptions().calendar === "iso8601")
        continue;
    for (let i = 0; i < 10; ++i) {
        shouldThrow(() => object.toLocaleString(), RangeError, `${tag} cached call ${i}`);
        shouldThrow(() => object.toLocaleString(undefined, {}), RangeError, `${tag} fresh call ${i}`);
    }
    // With a matching calendar, formatting succeeds via the fresh-formatter path.
    const formatted = object.toLocaleString(undefined, { calendar: "iso8601" });
    shouldBe(typeof formatted, "string", `${tag} with iso8601 calendar`);
    shouldBe(formatted.length > 0, true, `${tag} with iso8601 calendar non-empty`);
}

// 4. Interleaving with Date.prototype.toLocale*String (which shares the same per-globalObject
// default formatters) must not change any result.
const date = new Date(2024, 0, 15, 10, 30, 45);
const dateExpected = [date.toLocaleString(), date.toLocaleDateString(), date.toLocaleTimeString()];
const temporalExpected = formattable.map((object) => object.toLocaleString());
for (let i = 0; i < 100; ++i) {
    shouldBe(date.toLocaleString(), dateExpected[0], `Date#toLocaleString interleave ${i}`);
    shouldBe(date.toLocaleDateString(), dateExpected[1], `Date#toLocaleDateString interleave ${i}`);
    shouldBe(date.toLocaleTimeString(), dateExpected[2], `Date#toLocaleTimeString interleave ${i}`);
    formattable.forEach((object, index) => {
        shouldBe(object.toLocaleString(), temporalExpected[index], `${object[Symbol.toStringTag]} interleave ${i}`);
    });
}

// 5. Explicit locales/options still take the fresh-formatter path and respect the arguments.
// Compare against Intl.DateTimeFormat directly to stay independent of ICU/CLDR output differences.
function shouldMatchDateTimeFormat(object, locale, options) {
    const tag = object[Symbol.toStringTag];
    shouldBe(object.toLocaleString(locale, options), new Intl.DateTimeFormat(locale, options).format(object), `${tag} toLocaleString vs Intl.DateTimeFormat`);
}
shouldMatchDateTimeFormat(plainDate, "en-US", { calendar: "iso8601" });
shouldMatchDateTimeFormat(plainDate, "en-US", { month: "long", day: "numeric", year: "numeric", calendar: "iso8601" });
shouldMatchDateTimeFormat(plainTime, "en-US", undefined);
shouldMatchDateTimeFormat(plainDateTime, "en-US", { calendar: "iso8601" });
shouldMatchDateTimeFormat(plainYearMonth, "en-US", { calendar: "iso8601" });
shouldMatchDateTimeFormat(plainMonthDay, "en-US", { calendar: "iso8601" });
shouldMatchDateTimeFormat(instant, "en-US", { timeZone: "UTC", calendar: "iso8601" });
