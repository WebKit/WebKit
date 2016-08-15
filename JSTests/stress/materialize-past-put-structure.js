function foo(p) {
    var o = {f:42};
    if (p)
        return {f:42, g:o};
}

noInline(foo);

var array = new Array(1000);
for (var i = 0; i < 4000000; ++i) {
    var o = foo(true);
    array[i % array.length] = o;
}

