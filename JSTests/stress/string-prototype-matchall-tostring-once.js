// Regression test: String.prototype.matchAll must call ToString(this) at most once,
// even when the user's toString invalidates the matchAll fast-path watchpoint mid-call,
// and must pass the original (un-ToString'd) thisValue to a user-defined @@matchAll.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// Fast-path entry, watchpoint invalidates inside toString → fall back to primordial @@matchAll
// with the already-converted string. ToString(this) must run exactly once.
{
    let toStringCount = 0;
    const receiver = {
        toString() {
            toStringCount++;
            // No-op write that still invalidates regExpPrimordialPropertiesWatchpointSet.
            RegExp.prototype.exec = RegExp.prototype.exec;
            return "aabbaa";
        },
    };

    const results = [...String.prototype.matchAll.call(receiver, /a+/g)];
    shouldBe(toStringCount, 1);
    shouldBe(results.length, 2);
    shouldBe(results[0][0], "aa");
    shouldBe(results[1][0], "aa");
}

// Spec compliance: when GetMethod(regexp, @@matchAll) returns a non-undefined matcher,
// String.prototype.matchAll passes the original thisValue (not a coerced string) to it.
{
    RegExp.prototype[Symbol.matchAll] = function(o) {
        return { receivedThis: o };
    };

    let toStringCount = 0;
    const receiver = {
        toString() { toStringCount++; return "should-not-be-called"; },
    };

    const result = String.prototype.matchAll.call(receiver, /x/g);
    shouldBe(toStringCount, 0);
    shouldBe(result.receivedThis, receiver);

    delete RegExp.prototype[Symbol.matchAll];
}
