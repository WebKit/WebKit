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

const date = Temporal.PlainDate.from('2025-07-31');

// A month/monthCode conflict must read overflow first.
{
    const log = [];
    shouldThrow(RangeError, () => date.with({ month: 3, monthCode: 'M04' }, { get overflow() { log.push('overflow'); return 'constrain'; } }));
    shouldBe(log.join(','), 'overflow');

    // So a throwing overflow getter wins over the conflict.
    shouldThrow(EvalError, () => date.with({ month: 3, monthCode: 'M04' }, { get overflow() { throw new EvalError('x'); } }));
    // And an invalid overflow value is reported before the conflict.
    shouldThrow(RangeError, () => date.with({ month: 3, monthCode: 'M04' }, { overflow: 'bogus' }));
}

// An empty bag must throw before overflow is read at all.
{
    const log = [];
    shouldThrow(TypeError, () => date.with({}, { get overflow() { log.push('overflow'); return 'constrain'; } }));
    shouldBe(log.join(','), '');

    // So the TypeError wins over a throwing overflow getter.
    shouldThrow(TypeError, () => date.with({}, { get overflow() { throw new EvalError('x'); } }));
}

// A consistent month/monthCode pair still resolves, and overflow is read exactly once.
{
    let calls = 0;
    shouldBe(date.with({ month: 4, monthCode: 'M04' }, { get overflow() { calls++; return 'constrain'; } }).toString(), '2025-04-30');
    shouldBe(calls, 1);

    shouldBe(date.with({ monthCode: 'M04' }).toString(), '2025-04-30');
    shouldBe(date.with({ month: 4 }).toString(), '2025-04-30');
    shouldThrow(RangeError, () => date.with({ month: 4 }, { overflow: 'reject' }));
    shouldBe(date.with({ day: 15 }).toString(), '2025-07-15');
}

// The non-ISO branch already read options first and must stay that way.
{
    const gregory = Temporal.PlainDate.from('2025-07-31[u-ca=gregory]');
    const log = [];
    shouldThrow(RangeError, () => gregory.with({ month: 3, monthCode: 'M04' }, { get overflow() { log.push('overflow'); return 'constrain'; } }));
    shouldBe(log.join(','), 'overflow');
    shouldThrow(TypeError, () => gregory.with({}));
}
