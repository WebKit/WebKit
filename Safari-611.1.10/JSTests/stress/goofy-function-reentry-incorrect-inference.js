function foo(a) {
    var x;
    if (a)
        x = a;
    return [function() {
        return x;
    }, function(a) {
        x = a;
    }];
}

var array = foo(false);
noInline(array[0]);
noInline(array[1]);
array[1](42);
for (var i = 0; i < 10000; ++i) {
    var result = array[0]();
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

array[1](43);
var result = array[0]();
if (result != 43)
    throw "Error: bad result at end: " + result;
