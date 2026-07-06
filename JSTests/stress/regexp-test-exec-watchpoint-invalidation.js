function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function target(re, s) {
    return re.test(s);
}
noInline(target);

const re = /foo/;
for (let i = 0; i < testLoopCount; i++)
    shouldBe(target(re, "foo"), true);

let called = 0;
RegExp.prototype.exec = function(s) {
    called++;
    return null;
};

shouldBe(target(re, "foo"), false);
shouldBe(called, 1);
