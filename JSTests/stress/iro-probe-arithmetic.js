//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// IRO forms offset relationships (e.g. `x > y + 1`) on the int32 values it
// tracks. This checks one is observable through probes, at either operand's
// anchor, in the weaker and orientation-flipped forms it implies.
//
// Note: probe the operands, not `probe(y) + 1` — an add fed by a probe result
// is a ValueAdd, which IRO does not track.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(x, y) {
    if (x > y + 1) {
        const px = $vm.probe("x", x);
        const py = $vm.probe("y", y);
        return px - py;
    }
    return 0;
}
noInline(fn);

for (let i = 0; i < testLoopCount; i++) fn(100, 1);
const iro = makeIROHelper(fn);

// The exact recorded relation, from either operand's anchor.
iro.assertRel({ at: "x", lhs: "x", rel: ">", rhs: "y", offset: 1 });
iro.assertRel({ at: "y", lhs: "x", rel: ">", rhs: "y", offset: 1 });

// Weaker / equivalent / flipped forms it implies.
iro.assertRel({ at: "y", lhs: "x", rel: ">", rhs: "y" });             // x > y
iro.assertRel({ at: "y", lhs: "x", rel: ">=", rhs: "y", offset: 2 }); // x >= y + 2  ⇔  x > y + 1
iro.assertRel({ at: "y", lhs: "y", rel: "<", rhs: "x", offset: -1 }); // y < x - 1
