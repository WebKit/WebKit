function foo(x) {
    return Math.sin(x);
}

noInline(foo);

var j = 0;
for (var i = 0; i < 100000; ++i)
    j = foo(i);

if (0.860248280789742 != j){
    throw "Error"
}
