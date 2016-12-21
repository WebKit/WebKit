// This file tests the global property on the global object.

let g = new Function("return this")();

if (g !== global)
    throw new Error("global returned the wrong value");

for (name in g) {
    if (name === "global")
        throw new Error("global should not be enumerable")
}


global = 5;
if (global !== 5)
    throw new Error("global should be assignable");

delete global;
if ("global" in g)
    throw new Error("global should be configurable");
