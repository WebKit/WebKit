//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// RegExp.input is a handy getter

var o = RegExp;
o.input = "foo";

function test(o) {
    var result = null;
    for (var i = 0; i < 30000; i++)
        result = o.input;

    return result;
}

for (var k = 0; k < 9; k++) {
    var newResult = test(o)
    if (newResult != "foo")
        throw "Failed at " + k + " with " +newResult;
    result = newResult; 
    o = {__proto__ : o }
}

