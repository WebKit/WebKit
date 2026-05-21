//@ requireOptions("--useTemporal=1")

// Regression test: ZonedDateTime.round() sub-day must round wall-clock time, not
// epoch-offset-since-midnight. These diverge when rounding UP crosses a fall-back
// transition: the rounded local time (02:00) is past the fold and unambiguously EST,
// but epoch-space arithmetic lands on 01:00 EST (wrong instant).

function assert(cond, msg) {
    if (!cond) throw new Error("FAIL: " + msg);
}

// America/New_York fall-back 2024-11-03: 02:00 AM EDT → 01:00 AM EST (UTC-4 → UTC-5)

// Round UP: 01:31 AM EDT → 02:00 AM (past fold) → unambiguously 02:00 AM EST
const zdt = Temporal.ZonedDateTime.from("2024-11-03T01:31:00-04:00[America/New_York]");
const rounded = zdt.round({ smallestUnit: "hour", roundingMode: "halfExpand" });
assert(rounded.hour === 2,    "round up: hour should be 2, got " + rounded.hour);
assert(rounded.offset === "-05:00", "round up: offset should be -05:00, got " + rounded.offset);
assert(rounded.toString() === "2024-11-03T02:00:00-05:00[America/New_York]",
    "round up: " + rounded.toString());

// Round DOWN: 01:29 AM EDT → 01:00 AM EDT (still in fold, same offset preserved)
const zdt2 = Temporal.ZonedDateTime.from("2024-11-03T01:29:00-04:00[America/New_York]");
const rounded2 = zdt2.round({ smallestUnit: "hour", roundingMode: "halfExpand" });
assert(rounded2.hour === 1,    "round down: hour should be 1, got " + rounded2.hour);
assert(rounded2.offset === "-04:00", "round down: offset should be -04:00, got " + rounded2.offset);

// Spring-forward still works: 01:45 AM PST rounds to 03:00 AM PDT
const zdt3 = Temporal.ZonedDateTime.from("2024-03-10T01:45:00-08:00[America/Los_Angeles]");
const rounded3 = zdt3.round({ smallestUnit: "hour", roundingMode: "halfExpand" });
assert(rounded3.hour === 3,    "spring-forward: hour should be 3, got " + rounded3.hour);
assert(rounded3.offset === "-07:00", "spring-forward: offset should be -07:00, got " + rounded3.offset);

// Round within fold (no crossing): 01:15 AM EDT → 01:00 AM EDT (prefer same offset)
const zdt4 = Temporal.ZonedDateTime.from("2024-11-03T01:15:00-04:00[America/New_York]");
const rounded4 = zdt4.round({ smallestUnit: "hour", roundingMode: "halfExpand" });
assert(rounded4.hour === 1,    "within fold: hour should be 1, got " + rounded4.hour);
assert(rounded4.offset === "-04:00", "within fold: offset preserved -04:00, got " + rounded4.offset);
