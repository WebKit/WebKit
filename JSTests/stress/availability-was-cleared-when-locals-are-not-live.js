//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=false")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

var a;

function foo(x, y, z) {
    baz(a);
    0 + (x ? a : [] + 0);
    return y;
}

function bar() {
    return foo.apply(null, arguments);
}

function baz(p) {
    if (p) {
        return bar(1, 1, 0);
    }
}

baz(1);

for (let i = 0; i < 1; i++) {
    foo(1);
}

for (let i = 0; i < 10000; i++) {
    baz();
}

let hello = baz(1);
shouldBe(hello, 1);
