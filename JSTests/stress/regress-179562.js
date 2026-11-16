//@ runFTLNoCJIT("--maxPerThreadStackUsage=400000") if $jitTests

function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            return f();
        }
    }
    return t()
}

function foo(a, b) {
  return [{
    name: b + "" + a
  }];
}

var exception;
try {
    __v_25012 = [].concat(
        foo(1, []),
        runNearStackLimit(() => {
            return foo("bla", Symbol.search);
        })
    );
} catch (e) {
    exception = e;
}

if (exception != "TypeError: Cannot convert a symbol to a string")
    throw "FAILED";
