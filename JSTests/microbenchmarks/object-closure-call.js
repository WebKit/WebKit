//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function makeObjectClosure(value) {
    function foo() {
        return this.x;
    }
    return {foo:foo, x:value};
}

var result = 0;
for (var i = 0; i < 10000; ++i) {
    var o = makeObjectClosure(i);
    for (var j = 0; j < 100; ++j)
        result += o.foo();
}

if (result != 4999500000)
    throw "Error: bad result: " + result;
