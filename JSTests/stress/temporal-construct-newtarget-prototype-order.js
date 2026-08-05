//@ requireOptions("--useTemporal=1")

function shouldThrowRangeErrorNotPrototypeRead(name, C, args) {
    const newTarget = new Proxy(function () { }, {
        get(target, key, receiver) {
            if (key === "prototype")
                throw new EvalError("read newTarget.prototype before validating arguments");
            return Reflect.get(target, key, receiver);
        },
    });

    let error = null;
    try {
        Reflect.construct(C, args, newTarget);
    } catch (e) {
        error = e;
    }

    if (!error)
        throw new Error(`${name}: expected a RangeError, got no exception`);
    if (error instanceof EvalError)
        throw new Error(`${name}: ${error.message}`);
    if (!(error instanceof RangeError))
        throw new Error(`${name}: expected a RangeError, got ${error}`);
}

// Each case picks arguments that fail the AO's own validity check — the one that lives inside
// CreateTemporalX rather than in the constructor body — so the ordering is what is under test.
shouldThrowRangeErrorNotPrototypeRead("Temporal.Duration", Temporal.Duration, [1, -1]); // IsValidDuration: mixed signs
shouldThrowRangeErrorNotPrototypeRead("Temporal.Duration", Temporal.Duration, [Infinity]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.Instant", Temporal.Instant, [10n ** 30n]); // IsValidEpochNanoseconds
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainDate", Temporal.PlainDate, [2020, 13, 1]); // IsValidISODate
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainDate", Temporal.PlainDate, [300000, 1, 1]); // ISODateWithinLimits
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainDateTime", Temporal.PlainDateTime, [2020, 13, 1]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainDateTime", Temporal.PlainDateTime, [300000, 1, 1]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainTime", Temporal.PlainTime, [25]); // IsValidTime
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainYearMonth", Temporal.PlainYearMonth, [2020, 13]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainYearMonth", Temporal.PlainYearMonth, [300000, 1]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.PlainMonthDay", Temporal.PlainMonthDay, [13, 1]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.ZonedDateTime", Temporal.ZonedDateTime, [10n ** 30n, "UTC"]);

// The tz/calendar checks in the ZonedDateTime constructor body precede Step 11 too.
shouldThrowRangeErrorNotPrototypeRead("Temporal.ZonedDateTime", Temporal.ZonedDateTime, [0n, "Nope/Nope"]);
shouldThrowRangeErrorNotPrototypeRead("Temporal.ZonedDateTime", Temporal.ZonedDateTime, [0n, "UTC", "nope"]);

// Subclassing must still work: with valid arguments the derived prototype is honoured.
class MyDuration extends Temporal.Duration { }
const derived = new MyDuration(1, 2);
if (!(derived instanceof MyDuration) || !(derived instanceof Temporal.Duration))
    throw new Error("subclass prototype chain broken");
if (derived.years !== 1 || derived.months !== 2)
    throw new Error("subclass slots not initialized");

class MyPlainDate extends Temporal.PlainDate { }
const derivedDate = new MyPlainDate(2020, 1, 1);
if (!(derivedDate instanceof MyPlainDate) || derivedDate.toString() !== "2020-01-01")
    throw new Error("PlainDate subclass broken");

// Reflect.construct with a distinct newTarget uses that newTarget's prototype.
const alt = { prototype: { tag: "alt" } };
const viaReflect = Reflect.construct(Temporal.Duration, [3], Object.assign(function () { }, alt));
if (Object.getPrototypeOf(viaReflect) !== alt.prototype)
    throw new Error("Reflect.construct did not use newTarget.prototype");
// `years` is an accessor on Temporal.Duration.prototype, which is deliberately NOT in this
// object's prototype chain, so read the slot through the getter rather than as a property.
const yearsGetter = Object.getOwnPropertyDescriptor(Temporal.Duration.prototype, "years").get;
if (yearsGetter.call(viaReflect) !== 3)
    throw new Error("Reflect.construct did not initialize slots");
