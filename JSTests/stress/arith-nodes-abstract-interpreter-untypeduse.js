// This test that the DFG Abstract interpreter properly handles UntypedUse for various ArithXXX and CompareXXX nodes.
// This test should run without errors or crashing.

let errors = 0;
const smallValue = 2.3023e-320;

function test(functionName, testFunc) {
    try{
        var ary_1 = [1.1, 2.2, 3.3];
        var ary_2 = ary_1;
        var f64_1 = new Float64Array(0x10);

        for (var i = 0; i < 200000; i++)
            testFunc(ary_1, f64_1, '1');

        testFunc(ary_2, f64_1, { toString:()=>{ ary_2[0] = {}; return "3"}});
        if (ary_2[2] != smallValue)
            throw functionName + " returned the wrong value";
    } catch(e) {
        errors++;
        print("Exception testing " + functionName + ": " + e);
    };
};

for (let i = 0; i < 8; i++) {
    test("Warmup", function(a, b, c){
        a[0] = 1.1;
        a[1] = 2.2;
        var tmp = Math.abs(c);
        b[0] = a[0];
        a[2] = smallValue;
    });
}

test("Unary -", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = -c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("Unary +", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = +c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("+", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = a[1] + c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("-", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = a[1] - c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("*", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = a[1] * c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("/", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = a[1] / c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("%", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = a[1] % c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("**", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c ** a[1];
    b[0] = a[0];
    a[2] = smallValue;
});

test("prefix ++", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = ++c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("prefix --", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = --c;
    b[0] = a[0];
    a[2] = smallValue;
});

test("==", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c == 7;
    b[0] = a[0];
    a[2] = smallValue;
});

test("<", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c < 42;
    b[0] = a[0];
    a[2] = smallValue;
});

test("<=", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c <= 7;
    b[0] = a[0];
    a[2] = smallValue;
});

test(">", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c > 3;
    b[0] = a[0];
    a[2] = smallValue;
});

test(">=", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = c >= 12;
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.abs", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.abs(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.acos", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.acos(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.acosh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.acosh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.asin", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.asin(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.asinh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.asinh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.atan", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.atan(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.atan2", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.atan2(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.atanh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.atanh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.ceil", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.ceil(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.cos", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.cos(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.cosh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.cosh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.exp", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.exp(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.expm1", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.expm1(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.floor", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.floor(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.fround", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.fround(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.log", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.log(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.log1p", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.log1p(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.log10", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.log10(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.log2", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.log2(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.round", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.round(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.sin", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.sin(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.sign", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.sign(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.sinh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.sinh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.sqrt", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.sqrt(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.tan", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.tan(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.tanh", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.tanh(c);
    b[0] = a[0];
    a[2] = smallValue;
});

test("Math.trunc", function(a, b, c){
    a[0] = 1.1;
    a[1] = 2.2;
    var tmp = Math.trunc(c);
    b[0] = a[0];
    a[2] = smallValue;
});


if (errors)
    throw "Failed " + errors + " tests."
