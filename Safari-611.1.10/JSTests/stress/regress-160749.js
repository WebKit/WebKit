// Regression test for 160749.  This test should not exit with an error or crash.
// Check that the Baseline JIT GetByValWithCacheId and PutByValWithCahcedId stubs properly handle exceptions.

function testCachedGetByVal()
{
    o = { };
    o['a'] = 42;

    let result = 0;
    let loopCount = 100000;
    let interationToChange = 90000;
    let expectedResult = 42 * interationToChange;
    let exceptions = 0;
    let expectedExceptions = loopCount - interationToChange;

    for (let i = 0; i < loopCount; i++) {
        if (i == interationToChange) {
            Object.defineProperty(o, "a", {
                enumerable: true,
                get: function() { throw "error"; return 100; }
            });
        }

        for (let v in o) {
            try {
                result += o[v.toString()];
            } catch(e) {
                if (e == "error")
                    exceptions++;
                else
                    throw "Got wrong exception \"" + e + "\"";
            }
        }
    }

    if (result != expectedResult)
        throw "Expected a result of " + expectedResult + ", but got " + result;
    if (exceptions != expectedExceptions)
        throw "1 Expected " + expectedExceptions + " exceptions, but got " + exceptions;
}

noDFG(testCachedGetByVal);

function testCachedPutByVal()
{
    o = { };
    o['a'] = 0;

    let result = 0;
    let loopCount = 100000;
    let iterationToChange = 90000;
    let exceptions = 0;
    let expectedExceptions = loopCount - iterationToChange;

    for (let i = 0; i < loopCount; i++) {
        if (i == iterationToChange) {
            result = o.a;
            Object.defineProperty(o, "_a", {
                enumerable: false,
                value: -1
            });
            Object.defineProperty(o, "a", {
                enumerable: true,
                set: function(v) { throw "error"; o._a = v; }
            });
        }

        for (let v in o) {
            try {
                o[v.toString()] = i + 1;
            } catch(e) {
                if (e == "error")
                    exceptions++;
                else
                    throw "Got wrong exception \"" + e + "\"";
            }
        }
    }

    if (result != iterationToChange)
        throw "Expected a result of " + result + ", but got " + o.a;
    if (o._a != -1)
        throw "Expected o._b to -1, but it is " + o._a;
    if (exceptions != expectedExceptions)
        throw "Expected " + expectedExceptions + " exceptions, but got " + exceptions;
}

noDFG(testCachedPutByVal);

testCachedGetByVal();
testCachedPutByVal();
