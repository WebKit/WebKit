//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// Exercises the debug-printing helpers range(id).toString() and
// relsAt(id).toString(). When writing a new test, call
//   print(iro.relsAt("y").toString());
// to see, at a probe's IR position, its recorded facts and the range of every
// probe those facts mention — the quickest way to find out what IRO proved
// before deciding what to assert. Here two probes are related (x > y + 1); the
// rendering at "y" should report the cross-probe fact and both probes' ranges.

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

// Fact order within a probe is not stable, so check for substrings rather than
// an exact rendering.
function expectContains(text, needle, what) {
    if (!text.includes(needle))
        throw new Error(what + ": expected output to contain " + JSON.stringify(needle)
            + ", got:\n" + text);
}

const rels = iro.relsAt("y").toString();
expectContains(rels, 'Relations at "y"', "relsAt header");
expectContains(rels, "ranges:", "relsAt ranges section");
expectContains(rels, '"y":', "relsAt y range line");
expectContains(rels, '"x":', "relsAt x range line");      // x shown because a fact mentions it
expectContains(rels, "facts:", "relsAt facts section");
expectContains(rels, '"y"<"x"-1', "relsAt cross-probe fact");

const r = iro.range("y").toString();
expectContains(r, "range ", "range header");
expectContains(r, '("y")', "range label");
