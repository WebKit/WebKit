function foo(a){
    return arguments[1];
}
noInline(foo);

var r = 0;
for (var i = 0; i < 100000; ++i) {
    var r = foo(52, 42);
}

if (r != 42) throw "Error: "+r;

