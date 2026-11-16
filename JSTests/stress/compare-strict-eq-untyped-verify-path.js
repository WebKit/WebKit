//@ if isFTLEnabled then runFTLNoCJIT else skip end
//@ requireOptions("--useDollarVM=1")

let ftlTrue = $vm.ftlTrue;

function eq(a, b) {
    return a === b;
}
noInline(eq);

var mix = [1.5, null, {}, "x", true, 0, Symbol(), 1n, undefined, -0.0, NaN];
for (var i = 0; i < testLoopCount; ++i)
    eq(mix[i % mix.length], mix[(i + 3) % mix.length]);

var values = [
    0, 1, -1, 2, 6, 7, 10, 42, 2147483647, -2147483648, 0x7FFFFFFF, -0x80000000,
    0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, 5.0,
    5e-324,
    -5e-324,
    2.2250738585072014e-308,
    1.7976931348623157e308,
    Infinity, -Infinity, NaN, 0/0, Infinity - Infinity,
    2147483647.0, -2147483648.0,
    2147483648.0, -2147483649.0,
    4294967295.0, 4294967296.0,
    9007199254740992.0,
    null, undefined, true, false,
    {}, {}, [], [], function(){}, function(){},
    "", "a", "a" + "", "hello",
    Symbol(), Symbol(), Symbol.iterator, Symbol.for("x"), Symbol.for("x"),
    0n, 0n, 1n, -1n, 12345678901234567890n, BigInt("12345678901234567890"),
    new Proxy({}, {}),
    Object(1), Object("x"), Object(true),
    /regex/, new Date(0),
];

values.push("runtime-" + Math.random().toString(36).slice(2));
values.push("runtime-" + Math.random().toString(36).slice(2));

var dyn = "same-content-" + Date.now();
values.push(dyn);
values.push(dyn.split("").join(""));

var N = values.length;

var ref = new Array(N * N);
for (var a = 0; a < N; ++a)
    for (var b = 0; b < N; ++b)
        ref[a * N + b] = (values[a] === values[b]);

var errors = [];
var didFTLCompile = false;

function runMatrix() {
    didFTLCompile = ftlTrue();
    for (var a = 0; a < N; ++a) {
        for (var b = 0; b < N; ++b) {
            var got = eq(values[a], values[b]);
            if (got !== ref[a * N + b]) {
                errors.push("[" + a + "," + b + "] eq(" + typeof values[a] + ":" +
                    String(values[a]) + ", " + typeof values[b] + ":" + String(values[b]) +
                    ") = " + got + ", expected " + ref[a * N + b]);
            }
        }
    }
}
noInline(runMatrix);

for (var iter = 0; iter < testLoopCount; ++iter) {
    runMatrix();
    if (errors.length)
        break;
}

if (!didFTLCompile)
    throw new Error("runMatrix never reached FTL");

if (errors.length)
    throw new Error("FAILURES (" + errors.length + " total, first 20):\n" + errors.slice(0, 20).join("\n"));

function stressCheck(a, b, expected, label) {
    for (var i = 0; i < testLoopCount; ++i) {
        if (eq(a, b) !== expected)
            throw new Error("stressCheck " + label + " failed at i=" + i);
    }
}

stressCheck(0.0, null, false, "double-zero-vs-null");
stressCheck(null, 0.0, false, "null-vs-double-zero");
stressCheck(0.0, 0.0, true, "double-zero-both");
stressCheck(0.0, 0, true, "double-zero-vs-int-zero");

stressCheck(2147483647, 2147483647, true, "int32-max-bits-equal");
stressCheck(-2147483648, -2147483648, true, "int32-min-bits-equal");
stressCheck(2147483647, -2147483648, false, "int32-max-vs-min");

var o = {};
stressCheck(o, null, false, "cell-or-null");
stressCheck(o, undefined, false, "cell-or-undefined");
stressCheck(o, true, false, "cell-or-true");
stressCheck(o, false, false, "cell-or-false");
stressCheck(o, 0, false, "cell-or-int");

var o2 = {};
stressCheck(o, o2, false, "cell-or-cell-diff");
stressCheck(o, o, true, "cell-self-bits-equal");

var s1 = ("content-equal-test" + Math.random()).slice(0, 18);
var s2 = s1.split("").join("");
stressCheck(s1, s2, s1 === s2, "string-content-compare");

stressCheck(99999999999999999999n, 99999999999999999999n, true, "bigint-content-equal");
stressCheck(99999999999999999999n, 99999999999999999998n, false, "bigint-content-diff");

stressCheck(NaN, NaN, false, "nan-self");
stressCheck(NaN, 0.0, false, "nan-vs-zero");
stressCheck(NaN, null, false, "nan-vs-null");

stressCheck(-0.0, 0.0, true, "neg-zero-pos-zero");
stressCheck(-0.0, 0, true, "neg-zero-int-zero");
