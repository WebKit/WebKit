// RegExp.input is a handy setter

var o = RegExp;
function test(o) {
    var k = 0;
    o.input = "bar";
    for (var i = 0; i < 30000; i++)
        o.input = "foo";

    return o.input;
}

var result = test(o);

for (var k = 0; k < 9; k++) {
    var start = new Date;
    var newResult = test(o)
    var end = new Date;
    if (newResult != result)
        throw "Failed at " + k + "with " +newResult + " vs. " + result
    result = newResult; 
    o = {__proto__ : o }
}

