//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py

var o = $vm.createCustomTestGetterSetter();
o.customValue2 = "foo";

function test(o) {
    var result = null;
    for (var i = 0; i < 30000; i++)
        result = o.customValue2;

    return result;
}

for (var k = 0; k < 9; k++) {
    var newResult = test(o)
    if (newResult != "foo")
        throw "Failed at " + k + " with " +newResult;
    result = newResult; 
    o = {__proto__ : o }
}

