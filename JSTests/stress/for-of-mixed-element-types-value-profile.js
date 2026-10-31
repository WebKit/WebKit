//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")

// Once a number shows up among strings, recompiling walk must not re-speculate that the loop variable is a string.

function events(i)
{
    // By iteration 20000, walk has long since tiered up past the LLInt.
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
