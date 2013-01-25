description(
"Tests that modulo of various values do the right thing."
);


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

// Use a loop to ensure we cover all three tiers of optimization.
for (var i = 0; i < 200; ++i) {
    shouldBe("myMod(x, t)", "5");
    shouldBe("myMod(y, t)", "-1");
    shouldBe("myMod(x, z)", "0");
    shouldBe("myMod(y, z)", "-1");
    shouldBe("myModByPos2(x)", "1");
    shouldBe("myModByPos2(y)", "-1");
    shouldBe("myModByPos5(x)", "0");
    shouldBe("myModByPos5(y)", "-1");
    shouldBe("myModByPos8(x)", "7");
    shouldBe("myModByPos8(y)", "-7");
    shouldBe("myModByNeg1(x)", "0");
    shouldBe("myModByNeg1(y)", "-0");
    shouldBe("myModByNeg4(x)", "3");
    shouldBe("myModByNeg4(y)", "-3");
    shouldBe("myModByNeg81(x)", "6");
    shouldBe("myModByNeg81(y)", "-13");

    if (i > 100) {
        v = x;
        w = y;
    }

    shouldBe("myMod(v, t)", i > 100 ? "5" : "2");
    shouldBe("myMod(w, t)", i > 100 ? "-1" : "4");
    shouldBe("myModByPos2(v)", i > 100 ? "1" : "0");
    shouldBe("myModByPos2(w)", i > 100 ? "-1" : "0");
    shouldBe("myModByPos5(v)", i > 100 ? "0" : "2");
    shouldBe("myModByPos5(w)", i > 100 ? "-1" : "4");
    shouldBe("myModByPos8(v)", i > 100 ? "7" : "2");
    shouldBe("myModByPos8(w)", i > 100 ? "-7" : "4");
    shouldBe("myModByNeg1(v)", i > 100 ? "0" : "0");
    shouldBe("myModByNeg1(w)", i > 100 ? "-0" : "0");
    shouldBe("myModByNeg4(v)", i > 100 ? "3" : "2");
    shouldBe("myModByNeg4(w)", i > 100 ? "-3" : "0");
    shouldBe("myModByNeg81(v)", i > 100 ? "6" : "2");
    shouldBe("myModByNeg81(w)", i > 100 ? "-13" : "4");
}

