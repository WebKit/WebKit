function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

// Overriding RegExp.prototype.exec fires the primordial RegExp watchpoint, so
// the recheck after ToString(this) must reject the fast path from then on.
{
    let execCalled = false;
    let re = /,/g;
    let obj = {
        toString() {
            RegExp.prototype.exec = function() {
                execCalled = true;
                return null;
            };
            return "a,b,c";
        }
    };
    let result = String.prototype.replace.call(obj, re, "X");
    shouldBe(execCalled, true);
    shouldBe(result, "a,b,c");
}

// After the watchpoint has fired, the generic @@replace path must also honor
// the overridden exec.
{
    let execCalled = false;
    RegExp.prototype.exec = function() {
        execCalled = true;
        return null;
    };
    let result = "a,b,c".replace(/,/, "X");
    shouldBe(execCalled, true);
    shouldBe(result, "a,b,c");
}
