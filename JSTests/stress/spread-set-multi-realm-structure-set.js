// A polymorphic [...s] whose proven structure set holds the primordial Set structure of three
// different realms.
//
// Restricting the fold to a single realm would be sound but would give this up. Taking each realm
// from Structure::realm() and arming every one of them keeps it foldable, so this is the
// non-regression guard for that: it must stay correct, and it must keep exercising the multi-realm
// path rather than quietly falling back to the slow path.
//
// Unlike the other cross-realm spread tests, this one is not a bug reproduction - it fails only if
// the fix over-restricts - so it is not gated on $secAccel3Removed. With the fix removed the fold
// falls back to the slow path and the assertions below still have to hold.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

const realmB = createGlobalObject();
const realmC = createGlobalObject();

const SetA = Set;
const SetB = realmB.Set;
const SetC = realmC.Set;

// Allocating the Set in the compiled function is what gives the Spread operand a proven, finite
// structure set; merging three arms makes that set span three realms. An argument would leave the
// structure Top and only exercise the single-realm runtime-check path.
function spreadIt(which) {
    let s;
    if (which === 0) {
        s = new SetA();
        s.add(1);
        s.add(2);
        s.add(3);
    } else if (which === 1) {
        s = new SetB();
        s.add(1);
        s.add(2);
        s.add(3);
        s.add(4);
    } else {
        s = new SetC();
        s.add(1);
        s.add(2);
    }
    return [...s].length;
}
noInline(spreadIt);

const expected = [3, 4, 2];

for (let i = 0; i < testLoopCount; ++i)
    spreadIt(i % 3);

for (let j = 0; j < 3; ++j)
    shouldBe(spreadIt(j), expected[j]);

// Invalidating one realm's protocol must be observed through that realm and only that realm.
realmC.Set.prototype[Symbol.iterator] = function* () {
    yield 1;
};

shouldBe(spreadIt(0), 3);
shouldBe(spreadIt(1), 4);
shouldBe(spreadIt(2), 1);
