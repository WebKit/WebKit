//@ requireOptions("--useTemporal=1")

function shouldThrow(op, errorConstructor) {
    try {
        op();
    } catch (e) {
        if (!(e instanceof errorConstructor)) {
            throw new Error(`threw ${e}, but should have thrown ${errorConstructor.name}`);
        }
        return;
    }
    throw new Error(`Expected to throw ${errorConstructor.name}, but no exception thrown`);
}

{
    const invalidValues = [
        2**32,
        2**32 + 1,
        Number.MAX_SAFE_INTEGER,
        Number.MAX_VALUE,
    ];
    for (const v of invalidValues) {
        shouldThrow(() => { Temporal.Duration.from({ years: v }); }, RangeError);
        shouldThrow(() => { Temporal.Duration.from({ months: v }); }, RangeError);
        shouldThrow(() => { Temporal.Duration.from({ weeks: v }); }, RangeError);
    }
}

{
    shouldThrow(() => { Temporal.Duration.from({ days: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 86400) }); }, RangeError)
    shouldThrow(() => { Temporal.Duration.from({ hours: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 3600) }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ minutes: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 60) }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ seconds: Number.MAX_SAFE_INTEGER + 1 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ milliseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e3 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ milliseconds: 9007199254740992_000 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ microseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e6 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ microseconds: 9007199254740992_000_000 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ nanoseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e9 }) }, RangeError);
    shouldThrow(() => { Temporal.Duration.from({ nanoseconds: 9007199254740992_000_000_000 }) }, RangeError);
}
