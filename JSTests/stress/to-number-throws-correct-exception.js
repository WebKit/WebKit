function test(op) {
    let test = `
        function runTest(iters) {
            let shouldThrow = false;
            let a = {
                valueOf() { 
                    if (shouldThrow)
                        throw "a";
                    return 0;
                }
            };
            let {proxy: b, revoke} = Proxy.revocable({}, {
                get: function(target, prop) {
                    if (prop === "valueOf") {
                        if (shouldThrow)
                            throw new Error("Should not be here!");
                        return function() {
                            return 0;
                        }
                    }
                }
            });
            function f(a, b) {
                return a ${op} b;
            }
            noInline(f);
            for (let i = 0; i < iters; i++) {
                f(a, b);
            }

            shouldThrow = true;
            let validException = false;
            let exception = null;
            revoke();
            try {
                f(a, b);
            } catch(e) {
                validException = e === "a";
                exception = e;
            }
            if (!validException)
                throw new Error("Bad operation: " + exception.toString() + " with iters: " + iters);
        }
        runTest(2);
        runTest(10);
        runTest(50);
        runTest(1000);
        runTest(10000);
    `;
    eval(test);
}
let ops = [
    "+"
    , "-"
    , "*"
    , "**"
    , "/"
    , "%"
    , "&"
    , "|"
    , "^"
    , ">>"
    , ">>>"
    , "<<"
];
for (let op of ops)
    test(op);

function test2(op) {
    function runTest(iters) {
        let test = `
            let shouldThrow = false;
            let a = {
                valueOf() { 
                    if (shouldThrow)
                        throw "a";
                    return 0;
                }
            };
            let {proxy: b, revoke} = Proxy.revocable({}, {
                get: function(target, prop) {
                    if (prop === "valueOf") {
                        if (shouldThrow)
                            throw new Error("Should not be here!");
                        return function() {
                            return 0;
                        }
                    }
                }
            });
            function f(a, b) {
                return a ${op} b;
            }
            noInline(f);
            for (let i = 0; i < ${iters}; i++) {
                f(a, b);
            }

            shouldThrow = true;
            let validException = false;
            let exception = null;
            revoke();
            try {
                f(a, b);
            } catch(e) {
                validException = e === "a";
                exception = e;
            }
            if (!validException)
                throw new Error("Bad operation: " + exception.toString() + " with iters: " + ${iters});
        `;
        eval(Math.random() + ";" + test);
    }
    runTest(2);
    runTest(10);
    runTest(50);
    runTest(1000);
    runTest(10000);
}
for (let op of ops)
    test2(op);
