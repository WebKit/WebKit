//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// Exercises assertNoRel and the ANY wildcard. assertNoRel proves IRO did NOT
// relate two probes; ANY matches "anything" — as an assertRel operand it means
// "related to some value in this direction", and as assertNoRel's `with` it
// means "not related to anything" (its trivial self == self echo aside).
//
// x and y are related by `x > y + 1`; z is compared to nothing, so it carries
// only its identity self-fact.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(x, y, z) {
    if (x > y + 1) {
        const px = $vm.probe("x", x);
        const py = $vm.probe("y", y);
        const pz = $vm.probe("z", z);
        return px - py + pz;
    }
    return 0;
}
noInline(fn);

for (let i = 0; i < testLoopCount; i++) fn(100, 1, 42);
const iro = makeIROHelper(fn);

// ANY as an assertRel operand: x is greater than *something*.
iro.assertRel({ at: "x", lhs: "x", rel: ">", rhs: iro.ANY });
// Omitting rhs defaults it to ANY, so this is the same assertion.
iro.assertRel({ at: "x", lhs: "x", rel: ">" });

// assertNoRel with a specific probe: x and z were never related.
iro.assertNoRel({ at: "x", with: "z" });

// assertNoRel with ANY: z has no relations at all beyond its identity fact.
iro.assertNoRel({ at: "z", with: iro.ANY });
