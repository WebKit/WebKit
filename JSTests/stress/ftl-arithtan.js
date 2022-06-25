function foo(x) {
    return Math.tan(x);
}

noInline(foo);

var expected = foo(100000 - 1);
var j = 0;
for (var i = 0; i < 100000; ++i)
    j = foo(i);

if (expected != j){
    throw `Error: ${j}`;
}
