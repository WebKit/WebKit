//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}
function shouldThrow(ctor, fn) {
    try { fn(); } catch (e) {
        if (!(e instanceof ctor))
            throw new Error(`expected ${ctor.name}, got ${e}`);
        return;
    }
    throw new Error(`expected ${ctor.name} but no exception thrown`);
}

const notStrings = [5, 0, true, false, null, 12n, Symbol('M03')];

// from() and with() across every type that reads a monthCode field.
for (const monthCode of notStrings) {
    shouldThrow(TypeError, () => Temporal.PlainDate.from({ year: 2024, month: 3, day: 5, monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainYearMonth.from({ year: 2024, month: 3, monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ month: 3, day: 5, monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainDateTime.from({ year: 2024, month: 3, day: 5, monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainDate.from('2024-03-05').with({ monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainDateTime.from('2024-03-05T00:00').with({ monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainYearMonth.from('2024-03').with({ monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from('03-05').with({ monthCode }));
    shouldThrow(TypeError, () => Temporal.PlainDate.from('2024-03-05[u-ca=hebrew]').with({ monthCode, day: 3 }));
}

// A malformed String is still a RangeError, and a well-formed one still works.
{
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ year: 2024, day: 5, monthCode: 'bogus' }));
    shouldThrow(RangeError, () => Temporal.PlainDateTime.from('2024-03-05T00:00').with({ monthCode: 'bogus' }));
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ year: 2024, month: 3, day: 5, monthCode: 'M05' }));
    shouldBe(Temporal.PlainDate.from({ year: 2024, month: 3, day: 5, monthCode: 'M03' }).toString(), '2024-03-05');
    shouldBe(Temporal.PlainDateTime.from('2024-03-05T00:00').with({ monthCode: 'M05' }).toString(), '2024-05-05T00:00:00');
}

// ToPrimitive is applied, so an object that stringifies to a valid monthCode is accepted and one
// that stringifies to garbage is a RangeError rather than a TypeError.
{
    shouldBe(Temporal.PlainDate.from({ year: 2024, day: 5, monthCode: { toString: () => 'M07' } }).toString(), '2024-07-05');
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ year: 2024, day: 5, monthCode: { toString: () => 'nope' } }));

    let calls = 0;
    const counted = { toString() { calls++; return 'M07'; } };
    shouldBe(Temporal.PlainDate.from({ year: 2024, day: 5, monthCode: counted }).toString(), '2024-07-05');
    shouldBe(calls, 1);
}

// Grammar is checked as monthCode is read, before year's type is validated; suitability for the
// calendar is checked after. Both orderings are already pinned by test262 and must not regress.
{
    shouldThrow(RangeError, () => Temporal.PlainDateTime.from({ day: 1, monthCode: 'L99M', year: Symbol() }));
    shouldThrow(TypeError, () => Temporal.PlainDateTime.from({ day: 1, monthCode: 'M99L', year: Symbol() }));
}
