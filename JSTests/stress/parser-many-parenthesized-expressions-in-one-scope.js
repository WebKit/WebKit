//@ skip if $memoryLimited
//@ slow!
//@ runDefault

// Used-variable sets internally kept by the parser should not be accumulated indefinitely
// when repeatedly parsing parenthesized expressions within the same scope.
const depth = 100;
const statement = "(".repeat(depth) + "0" + ")".repeat(depth) + ";";
const result = eval(statement.repeat(24_000_000 / depth));
if (result !== 0)
    throw new Error("Bad result: " + result);
