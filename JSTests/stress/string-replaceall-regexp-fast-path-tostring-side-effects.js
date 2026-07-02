function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

// RegExp.prototype.compile does not fire any watchpoint. Dropping the "g" flag
// during ToString(this) must result in a single replacement, matching the
// generic @@replace which rereads the flags after ToString.
{
    let re = /,/g;
    let obj = {
        toString() {
            re.compile(",");
            return "a,b,c";
        }
    };
    let result = String.prototype.replaceAll.call(obj, re, "X");
    shouldBe(result, "aXb,c");
}

// Overriding RegExp.prototype.exec fires the primordial RegExp watchpoint, so
// this must stay the last case in this file.
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
    let result = String.prototype.replaceAll.call(obj, re, "X");
    shouldBe(execCalled, true);
    shouldBe(result, "a,b,c");
}
