//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// Covers the relation-matching logic in iro-test-helpers (canonicalRelation /
// impliedBy): the <= and != query forms, and the weaker relations an == or an
// inequality fact implies. This exercises the helper, not new IRO behavior, so
// the function just needs a fact of each kind in scope at one point:
//   if (x)         -> p != 0                          (NotEqual)
//   if (a > b + 1) -> a > b + 1  and  b < a - 1       (GreaterThan / LessThan)
//   every probe    -> self == self                    (Equal)

load("./resources/iro-test-helpers.js", "caller relative");

function fn(x, a, b) {
    if (x) {
        if (a > b + 1) {
            const p = $vm.probe("p", x);
            const pa = $vm.probe("a", a);
            const pb = $vm.probe("b", b);
            return p + pa - pb;
        }
    }
    return 0;
}
noInline(fn);

for (let i = 0; i < testLoopCount; i++) fn((i & 7) + 1, 100, 1);
const iro = makeIROHelper(fn);

function expectThrow(thunk, what) {
    try { thunk(); } catch { return; }
    throw new Error(what + ": expected assertRel to throw, but it did not");
}

// != query implied by a > fact and a < fact.
iro.assertRel({ at: "a", lhs: "a", rel: "!=", rhs: "b" });   // a > b+1  => a != b
iro.assertRel({ at: "b", lhs: "b", rel: "!=", rhs: "a" });   // b < a-1  => b != a

// != fact matched by a != query (lhs omitted defaults to `at`).
iro.assertRel({ at: "p", rel: "!=", rhs: { const: 0 } });    // p != 0

// <= / >= queries canonicalized to < / > and matched against the offset facts.
iro.assertRel({ at: "b", lhs: "b", rel: "<=", rhs: "a", offset: -2 }); // b <= a-2 <=> b < a-1
iro.assertRel({ at: "a", lhs: "a", rel: ">=", rhs: "b", offset: 2 });  // a >= b+2 <=> a > b+1

// An == fact implies the trivially-true weaker forms.
iro.assertRel({ at: "a", lhs: "a", rel: ">", rhs: "a", offset: -1 });  // a > a-1
iro.assertRel({ at: "a", lhs: "a", rel: "<", rhs: "a", offset: 1 });   // a < a+1
iro.assertRel({ at: "a", lhs: "a", rel: "!=", rhs: "a", offset: 5 });  // a != a+5

// An unsupported relation is rejected.
expectThrow(() => iro.assertRel({ at: "a", lhs: "a", rel: "<>", rhs: "b" }), "unsupported relation");

// A relation IRO did not record is not invented (a > b+1 does not imply a < b).
expectThrow(() => iro.assertRel({ at: "a", lhs: "a", rel: "<", rhs: "b" }), "a < b not implied");
