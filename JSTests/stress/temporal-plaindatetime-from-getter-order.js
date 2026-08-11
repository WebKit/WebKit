//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type, message) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
    if (message !== undefined && err.message !== message)
        throw new Error(`Expected message "${message}" but got "${err.message}"`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function trackedBag(reads, props) {
    let obj = {};
    for (const key of Object.keys(props)) {
        Object.defineProperty(obj, key, {
            get() { reads.push(key); return props[key]; },
            enumerable: true,
        });
    }
    return obj;
}

// Missing `day` (alphabetically first): every other defined property must still be read.
{
    let reads = [];
    shouldThrow(() => {
        Temporal.PlainDateTime.from(trackedBag(reads, {
            hour: 5, month: 3, monthCode: undefined, nanosecond: 1, second: 2, year: 2020,
        }));
    }, TypeError, "day property must be present");
    shouldBe(JSON.stringify(reads), JSON.stringify(["hour", "month", "monthCode", "nanosecond", "second", "year"]));
}

// Missing both `month` and `monthCode`.
{
    let reads = [];
    shouldThrow(() => {
        Temporal.PlainDateTime.from(trackedBag(reads, {
            day: 5, hour: 1, nanosecond: 1, second: 1, year: 2020,
        }));
    }, TypeError, "month or monthCode property must be present");
    shouldBe(JSON.stringify(reads), JSON.stringify(["day", "hour", "nanosecond", "second", "year"]));
}

// Missing `year` (alphabetically last): everything before it must still be read.
{
    let reads = [];
    shouldThrow(() => {
        Temporal.PlainDateTime.from(trackedBag(reads, {
            day: 5, hour: 1, month: 3, nanosecond: 1, second: 1,
        }));
    }, TypeError, "year property must be present");
    shouldBe(JSON.stringify(reads), JSON.stringify(["day", "hour", "month", "nanosecond", "second"]));
}

// Sanity: a fully-populated bag still reads every property, in alphabetical order, and succeeds.
{
    let reads = [];
    let pdt = Temporal.PlainDateTime.from(trackedBag(reads, {
        day: 15, hour: 1, microsecond: 0, millisecond: 0, minute: 0, month: 6,
        monthCode: undefined, nanosecond: 0, second: 0, year: 2020,
    }));
    shouldBe(JSON.stringify(reads), JSON.stringify([
        "day", "hour", "microsecond", "millisecond", "minute", "month", "monthCode", "nanosecond", "second", "year",
    ]));
    shouldBe(pdt.toString(), "2020-06-15T01:00:00");
}
