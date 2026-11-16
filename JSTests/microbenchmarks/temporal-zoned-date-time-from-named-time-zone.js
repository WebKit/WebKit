//@ requireOptions("--useTemporal=1")

// Perf/regression benchmark for getNamedTimeZoneEpochNanoseconds
// (TimeZoneICUBridge.cpp), the local-wall-clock -> epoch resolver for named IANA
// zones that was rewritten to use ucal_getTimeZoneOffsetFromLocal.
//
// The base ZonedDateTimes are built once, outside the timed loop, so the loop
// body is dominated by the inverse resolution rather than by property-bag
// parsing and allocation. ZonedDateTime.with re-resolves the modified local
// wall-clock time to an epoch (interpretISODateTimeOffset ->
// getPossibleEpochNanosecondsFor -> getNamedTimeZoneEpochNanoseconds) on every
// call. A named zone forces the ICU path; UTC/offset zones bypass it via the
// isUTCOffset() fast path. The `minute` is varied per iteration so every call is
// a distinct local time and no input-keyed result caching can turn this into a
// no-op guard.
const timeZone = "America/Vancouver";
const cases = [
    { year: 2024, month: 7,  day: 15, hour: 12, minute: 30, timeZone }, // normal (DST)
    { year: 2024, month: 1,  day: 15, hour: 12, minute: 30, timeZone }, // normal (standard)
    { year: 2024, month: 11, day: 3,  hour: 1,  minute: 30, timeZone }, // fall-back fold
    { year: 2024, month: 3,  day: 10, hour: 3,  minute: 30, timeZone }, // day of spring-forward
];
const zdts = cases.map(c => Temporal.ZonedDateTime.from(c, { disambiguation: "compatible" }));

let acc = 0n;
for (var i = 0; i < 1e4; ++i) {
    for (var j = 0; j < zdts.length; ++j) {
        const zdt = zdts[j].with({ minute: i % 60 }); // re-resolves local->epoch each call
        acc += zdt.epochNanoseconds;                  // read result so it can't be DCE'd
    }
}

if (acc === 0n)
    throw "Bad result: " + acc;
