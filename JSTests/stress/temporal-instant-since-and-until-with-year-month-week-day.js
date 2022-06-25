//@ requireOptions("--useTemporal=1")
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

let earlier = new Temporal.Instant(1_000_000_000_000_000_000n);
let later = new Temporal.Instant(1_000_090_061_987_654_321n);
let units = [ "year", "month", "week", "day", ];
for (let smallestUnit of units) {
    shouldThrow(() => {
        later.since(earlier, { smallestUnit });
    }, `RangeError: smallestUnit is a disallowed unit`);
}
for (let largestUnit of units) {
    shouldThrow(() => {
        later.since(earlier, { largestUnit });
    }, `RangeError: largestUnit is a disallowed unit`);
}
for (let smallestUnit of units) {
    shouldThrow(() => {
        earlier.until(later, { smallestUnit });
    }, `RangeError: smallestUnit is a disallowed unit`);
}
for (let largestUnit of units) {
    shouldThrow(() => {
        earlier.until(later, { largestUnit });
    }, `RangeError: largestUnit is a disallowed unit`);
}
