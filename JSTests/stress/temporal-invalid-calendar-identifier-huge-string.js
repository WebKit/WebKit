//@ skip if $memoryLimited
//@ requireOptions("--useTemporal=1")

function shouldThrowRangeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error(`expected RangeError but got ${error}`);
    if (error.message.length > 1000)
        throw new Error(`error message is too long: ${error.message.length}`);
}

// Error messages quoting an invalid calendar identifier must not exceed String::MaxLength.
const maxLength = 2 ** 31 - 1;

let calendar;
try {
    calendar = 'A'.repeat(maxLength - 1);
} catch {
    // Not enough memory to allocate the string.
}

if (calendar) {
    shouldThrowRangeError(() => Temporal.PlainMonthDay.from({ calendar, monthCode: "M01", day: 1 }));
    shouldThrowRangeError(() => Temporal.PlainDate.from({ calendar, year: 2020, month: 1, day: 1 }));
    shouldThrowRangeError(() => new Temporal.PlainDate(2020, 1, 1).withCalendar(calendar));
    calendar = undefined;
}

// Calendar annotation parsed out of an ISO string.
let annotation;
try {
    const prefix = "2020-01-01[u-ca=";
    const suffix = "]";
    const chunkCount = Math.floor((maxLength - prefix.length - suffix.length - 3) / 4);
    annotation = prefix + "abc" + "-abc".repeat(chunkCount) + suffix;
} catch {
    // Not enough memory to allocate the string.
}

if (annotation) {
    shouldThrowRangeError(() => Temporal.PlainDate.from(annotation));
    shouldThrowRangeError(() => Temporal.PlainDateTime.from(annotation));
    shouldThrowRangeError(() => Temporal.PlainMonthDay.from(annotation));
    shouldThrowRangeError(() => Temporal.PlainYearMonth.from(annotation));
}
