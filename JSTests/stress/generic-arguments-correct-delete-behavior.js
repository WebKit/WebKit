function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function makeTest(shouldCaptureArgument, deleteTwice, zeroAsString) {
    return eval(`
        function foo(x) {
            ${shouldCaptureArgument ? `function bar() { return x; }` : ""}

            assert(x === null);

            let prop = ${zeroAsString ? "'0'" : "0"};
            Object.defineProperty(arguments, "0", {enumerable: false, value:45});
            assert(arguments[prop] === 45);
            assert(x === 45);

            let result = delete arguments[prop];
            assert(result);
            ${deleteTwice ? `assert(delete arguments[prop]);` : ""};

            assert(arguments[prop] === undefined); // don't crash here.
            assert(!(prop in arguments));

            arguments[prop] = 50;

            assert(arguments[prop] === 50);
            assert(x === 45);
        }; foo;
    `);
}

let functions = [];
functions.push(makeTest(false, false, true));
functions.push(makeTest(false, false, false));
functions.push(makeTest(false, true, false));
functions.push(makeTest(false, true, true));
functions.push(makeTest(true, false, true));
functions.push(makeTest(true, false, false));
functions.push(makeTest(true, true, false));
functions.push(makeTest(true, true, true));

for (let f of functions) {
    noInline(f);
    for (let i = 0; i < 1000; ++i) 
        f(null);
}
