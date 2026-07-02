x = undefined;
load("./resources/literal-parser-test-case.js", "caller relative");
if (x !== undefined)
    throw new Error("Bad result");
