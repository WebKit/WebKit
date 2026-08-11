//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}

const earlier = new Temporal.PlainTime(8, 22, 36, 123, 456, 789);
const later = new Temporal.PlainTime(12, 39, 40, 987, 654, 289);

// Each row: [roundingMode, smallestUnit, expectedHours(later.since(earlier)), expectedHours(earlier.since(later))]
// Values verified against the test262 corpus and against V8 (which delegates to temporal_rs).
// Hours are chosen because the +4.28h / -4.28h difference produces distinct integer hours
// under every asymmetric mode.
const cases = [
    ["ceil", "hour", 5, -4],
    ["floor", "hour", 4, -5],
    ["expand", "hour", 5, -5],
    ["trunc", "hour", 4, -4],
    ["halfCeil", "hour", 4, -4],
    ["halfFloor", "hour", 4, -4],
    ["halfExpand", "hour", 4, -4],
    ["halfTrunc", "hour", 4, -4],
    ["halfEven", "hour", 4, -4],
];

for (const [roundingMode, smallestUnit, posHours, negHours] of cases) {
    const pos = later.since(earlier, { roundingMode, smallestUnit });
    const neg = earlier.since(later, { roundingMode, smallestUnit });
    shouldBe(pos.hours, posHours, `later.since(earlier) ${roundingMode}/${smallestUnit} hours`);
    shouldBe(neg.hours, negHours, `earlier.since(later) ${roundingMode}/${smallestUnit} hours`);
    // The sign must match the wall-clock direction.
    if (posHours !== 0) shouldBe(pos.sign, 1, `later.since(earlier) ${roundingMode} positive sign`);
    if (negHours !== 0) shouldBe(neg.sign, -1, `earlier.since(later) ${roundingMode} negative sign`);
}

// Sanity check: until() and since() differ only in sign for symmetric modes.
for (const mode of ["halfExpand", "halfEven", "trunc", "expand"]) {
    const u = later.until(earlier, { roundingMode: mode, smallestUnit: "hour" });
    const s = later.since(earlier, { roundingMode: mode, smallestUnit: "hour" });
    shouldBe(u.hours, -s.hours, `until/since hours symmetry under ${mode}`);
}
