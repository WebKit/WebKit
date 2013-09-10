// This tests that integer modulo is appropriately optimized

function myMod(a, b) {
    return a % b;
}

function myModByPos2(a) {
    return a % 2;
}

function myModByPos5(a) {
    return a % 5;
}

function myModByPos8(a) {
    return a % 8;
}

function myModByNeg1(a) {
    return a % -1;
}

function myModByNeg4(a) {
    return a % -4;
}

function myModByNeg81(a) {
    return a % -81;
}

var t = 10;
var v = 2;
var w = 4;
var x = 65535;
var y = -131071;
var z = 3;

var result = 0;

// Use a loop to ensure we cover all three tiers of optimization.
for (var i = 0; i < 2000; ++i) {
    result += myMod(x, t);
    result += myMod(y, t);
    result += myMod(x, z);
    result += myMod(y, z);
    result += myModByPos2(x);
    result += myModByPos2(y);
    result += myModByPos5(x);
    result += myModByPos5(y);
    result += myModByPos8(x);
    result += myModByPos8(y);
    result += myModByNeg1(x);
    result += myModByNeg1(y);
    result += myModByNeg4(x);
    result += myModByNeg4(y);
    result += myModByNeg81(x);
    result += myModByNeg81(y);

    if (i > 100) {
        v = x;
        w = y;
    }

    result += myMod(v, t);
    result += myMod(w, t);
    result += myModByPos2(v);
    result += myModByPos2(w);
    result += myModByPos5(v);
    result += myModByPos5(w);
    result += myModByPos8(v);
    result += myModByPos8(w);
    result += myModByNeg1(v);
    result += myModByNeg1(w);
    result += myModByNeg4(v);
    result += myModByNeg4(w);
    result += myModByNeg81(v);
    result += myModByNeg81(w);
}

if (result != -14970) {
    throw "Bad result: " + result;
}
