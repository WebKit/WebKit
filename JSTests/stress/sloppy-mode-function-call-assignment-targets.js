function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof SyntaxError))
        throw new Error('Expected SyntaxError!');
}

function shouldThrowReferenceError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof ReferenceError))
        throw new Error('Expected ReferenceError!');
}

let fCallCount = 0;
let fValueOfCallCount = 0;
let gCallCount = 0;

function f() {
    fCallCount++;
    return {
        valueOf() { fValueOfCallCount++; return 1; }
    };
}

function g() {
    gCallCount++;
    return 2;
}

shouldThrowReferenceError(() => { f() = g(); });

shouldThrowReferenceError(() => { f() += g(); });
shouldThrowReferenceError(() => { f() -= g(); });
shouldThrowReferenceError(() => { f() *= g(); });
shouldThrowReferenceError(() => { f() **= g(); });
shouldThrowReferenceError(() => { f() &= g(); });
shouldThrowReferenceError(() => { f() |= g(); });
shouldThrowReferenceError(() => { f() ^= g(); });
shouldThrowReferenceError(() => { f() <<= g(); });
shouldThrowReferenceError(() => { f() >>= g(); });
shouldThrowReferenceError(() => { f() >>>= g(); });

shouldThrowSyntaxError('f() &&= g()');
shouldThrowSyntaxError('f() ||= g()');
shouldThrowSyntaxError('f() ??= g()');

shouldThrowReferenceError(() => { ++f(); });
shouldThrowReferenceError(() => { f()++; });
shouldThrowReferenceError(() => { --f(); });
shouldThrowReferenceError(() => { f()--; });

shouldThrowReferenceError(() => { for (f() in [1]) {} });
shouldThrowReferenceError(() => { for (f() of [1]) {} });

const o = { f };
shouldThrowReferenceError(() => { o.f() = g(); });
shouldThrowReferenceError(() => { o['f']() = g(); });
const ff = () => f;
shouldThrowReferenceError(() => { ff()() = g(); });

shouldBe(fCallCount, 20);
shouldBe(fValueOfCallCount, 0);
shouldBe(gCallCount, 0);
