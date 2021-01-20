//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    var x = 42;
    var y = 43;
    return function(z) {
        var result = x + y;
        x += z;
        y += z;
        return result;
    }
}

var f = foo();
noInline(f);

for (var i = 0; i < 10000000; ++i)
    f(1);
