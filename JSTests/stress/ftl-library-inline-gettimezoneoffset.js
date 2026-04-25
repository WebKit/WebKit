function foo(x, d){
    return x + d.getTimezoneOffset();
}

noInline(foo);

var d = new Date();
var expected = foo(0, d);
var count = 1000000;
var result = 0;
for (var i = 0 ; i < count; i++){
    result += foo(0, d);
}

if (result != count * expected)
    throw "Error: bad result: " + result;
