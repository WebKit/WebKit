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

const df = new Intl.DurationFormat();

{
    const invalidValues = [
        2**32,
        2**32 + 1,
        Number.MAX_SAFE_INTEGER,
        Number.MAX_VALUE,
    ];
    for (const v of invalidValues) {
        shouldThrow(() => { df.format({ years: v }); }, RangeError);
        shouldThrow(() => { df.format({ months: v }); }, RangeError);
        shouldThrow(() => { df.format({ weeks: v }); }, RangeError);
    }
}

{
    shouldThrow(() => { df.format({ days: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 86400) }); }, RangeError)
    shouldThrow(() => { df.format({ hours: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 3600) }) }, RangeError);
    shouldThrow(() => { df.format({ minutes: Math.ceil((Number.MAX_SAFE_INTEGER + 1) / 60) }) }, RangeError);
    shouldThrow(() => { df.format({ seconds: Number.MAX_SAFE_INTEGER + 1 }) }, RangeError);
    shouldThrow(() => { df.format({ milliseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e3 }) }, RangeError);
    shouldThrow(() => { df.format({ milliseconds: 9007199254740992_000 }) }, RangeError);
    shouldThrow(() => { df.format({ microseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e6 }) }, RangeError);
    shouldThrow(() => { df.format({ microseconds: 9007199254740992_000_000 }) }, RangeError);
    shouldThrow(() => { df.format({ nanoseconds: (Number.MAX_SAFE_INTEGER + 1) * 1e9 }) }, RangeError);
    shouldThrow(() => { df.format({ nanoseconds: 9007199254740992_000_000_000 }) }, RangeError);
}
