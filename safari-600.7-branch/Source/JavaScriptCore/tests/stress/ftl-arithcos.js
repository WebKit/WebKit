function foo(x) {
    return Math.cos(x);
}

noInline(foo);

var j = 0;
for (var i = 0; i < 100000; ++i)
    j = foo(i);

if (-0.5098753724179009 != j){
    throw "Error"
}
