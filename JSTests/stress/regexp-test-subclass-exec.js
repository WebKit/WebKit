function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

class OverriddenExec extends RegExp {
    exec(s) {
        execCalls++;
        return null;
    }
}
let execCalls = 0;

function target(re, s) {
    return re.test(s);
}
noInline(target);

const re = /foo/;
for (let i = 0; i < testLoopCount; i++)
    shouldBe(target(re, "foo"), true);

const subclassed = new OverriddenExec("foo");
shouldBe(target(subclassed, "foo"), false);
shouldBe(execCalls, 1);

class PlainSubclass extends RegExp { }
shouldBe(target(new PlainSubclass("foo"), "foo"), true);
