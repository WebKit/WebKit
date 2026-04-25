//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py

var o = $vm.createCustomTestGetterSetter();
function test(o) {
    var k = 0;
    o.customValue2 = "bar";
    for (var i = 0; i < 30000; i++)
        o.customValue2 = "foo";

    return o.customValue2;
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

