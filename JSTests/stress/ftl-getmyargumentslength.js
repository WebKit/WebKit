function foo(){
    return arguments.length;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var r = foo(11, 12, 13, 18, 19, 20);
    if (r != 6) throw "Error: "+r;
}

