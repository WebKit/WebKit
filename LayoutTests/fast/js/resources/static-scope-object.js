description('This test ensures that the correct "this" object is used when calling named function expressions or exceptions.');

this.toString = function() { return "the global object" };
var globalObject = this;

function namedFunctionExpression() {
    return function f(i) { if (i > 0) return this; return f(1); }(0);
}

shouldBe("namedFunctionExpression()", 'globalObject');

function throwingFunctionAsException() {
    try {
        throw function(){ return this; }
    } catch(e) {
        return e();
    }
}

shouldBe("throwingFunctionAsException()", 'globalObject');

successfullyParsed = true;
