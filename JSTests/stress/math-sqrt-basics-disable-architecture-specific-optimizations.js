//@ run("no-architecture-specific-optimizations", "--useArchitectureSpecificOptimizations=false", *NO_CJIT_OPTIONS)
//@ run("no-architecture-specific-optimizations-ftl", "--useArchitectureSpecificOptimizations=false", *FTL_OPTIONS)

// Basic cases of Math.sqrt().
function sqrtOnInteger(radicand) {
    return Math.sqrt(radicand);
}
noInline(sqrtOnInteger);

function testSquareRoot16() {
    for (var i = 0; i < 10000; ++i) {
        var result = sqrtOnInteger(16);
        if (result !== 4)
            throw "sqrtOnInteger(16) = " + result + ", expected 4";
    }
}
testSquareRoot16();

var sqrtOnIntegerNegativeNumber = sqrtOnDouble(-4);
if (!isNaN(sqrtOnIntegerNegativeNumber))
    throw "sqrtOnDouble(-4) = " + sqrtOnIntegerNegativeNumber + ", expected NaN";

var sqrtOnIntegerFallback = sqrtOnInteger(Math.PI);
if (sqrtOnIntegerFallback != 1.7724538509055159)
    throw "sqrtOnInteger(Math.PI) = " + sqrtOnIntegerFallback + ", expected 1.7724538509055159";


function sqrtOnDouble(radicand) {
    return Math.sqrt(radicand);
}
noInline(sqrtOnDouble);

function testSquareRootDouble() {
    for (var i = 0; i < 10000; ++i) {
        var result = sqrtOnInteger(Math.LN2);
        if (result !== 0.8325546111576977)
            throw "sqrtOnInteger(Math.LN2) = " + result + ", expected 0.8325546111576977";
    }
}
testSquareRootDouble();

var sqrtOnDoubleNegativeNumber = sqrtOnDouble(-Math.PI);
if (!isNaN(sqrtOnDoubleNegativeNumber))
    throw "sqrtOnDouble(-Math.PI) = " + sqrtOnDoubleNegativeNumber + ", expected NaN";

var sqrtOnDoubleFallback = sqrtOnDouble(4);
if (sqrtOnDoubleFallback !== 2)
    throw "sqrtOnDouble(4) = " + sqrtOnDoubleFallback + ", expected 2";