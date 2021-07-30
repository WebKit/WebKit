//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py

o = $vm.createCustomTestGetterSetter();
j = 0;
l = 2;
z = 0;
function test(o, z) {
    var k = arguments[(((j << 1 | l) >> 1) ^ 1) & (z *= 1)];
    k.customValue2 = 0;
    for (var i = 0; i < 25000; i++) {
        k.customValue2 = "foo";
    }

    return k.customValue2;
}
var result = test({__proto__: {bar:"wibble", customValue2:"foo"}});
var result = test({customValue2:"foo"});
var result = test(o)
for (var k = 0; k < 6; k++) {
    var start = new Date;
    var newResult = test(o)
    var end = new Date;
    if (newResult != result)
        throw "Failed at " + k + "with " + newResult + " vs. " + result
    result = newResult;
    o = {__proto__ : o }
}
