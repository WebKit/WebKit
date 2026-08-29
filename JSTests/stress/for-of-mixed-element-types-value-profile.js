//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")

// The baseline JIT's fast-array op_iterator_next path must profile the iterated element into the
// getValue checkpoint's value profile. When it wrote to the computeNext slot instead, the DFG kept
// predicting the loop variable from whatever the LLInt had sampled, so it re-speculated the same
// wrong type on every recompilation until the reoptimization retry counter ran out.

function events(i)
{
    // Only the baseline JIT ever sees the number: by iteration 20000 this function is long past the
    // LLInt.
    return i < 20000 ? ["a", "bb", "ccc"] : ["a", "bb", 0];
}
noInline(events);

function walk(i)
{
    let count = 0;
    for (let event of events(i)) {
        if (typeof event === "number")
            count += event;
        else
            count += event.length;
    }
    return count;
}
noInline(walk);

let total = 0;
for (let i = 0; i < 300000; ++i)
    total += walk(i);

if (total !== 960000)
    throw new Error(`bad result: ${total}`);

const compiles = numberOfDFGCompiles(walk);
if (compiles > 4)
    throw new Error(`walk was DFG-compiled ${compiles} times; the loop variable's value profile is not being updated`);
