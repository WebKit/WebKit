function shouldBe(op, actual, expected) {
    if (actual !== expected) {
        throw new Error(`Bad value for ${op}: ${actual} (expected ${expected})`);
    }
}

const N = 100000;

let count = 0;
class A {
    valueOf() { count++; }
}
const a = new A();

function lt() { a < a }
function lte() { a <= a }
function gt() { a > a }
function gte() { a >= a }
function eq() { a == a }
function eqq() { a === a }

count = 0;
for (let i = 0; i != N; i++)
    lt();
shouldBe("lt", count, 2*N);

count = 0;
for (let i = 0; i != N; i++)
    lte();
shouldBe("lte", count, 2*N);

count = 0;
for (let i = 0; i != N; i++)
    gt();
shouldBe("gt", count, 2*N);

count = 0;
for (let i = 0; i != N; i++)
    gte();
shouldBe("gte", count, 2*N);

// valueOf() should not be called for == or ===

count = 0;
for (let i = 0; i != N; i++)
    eq();
shouldBe("eq", count, 0);

count = 0;
for (let i = 0; i != N; i++)
    eqq();
shouldBe("eqq", count, 0);
