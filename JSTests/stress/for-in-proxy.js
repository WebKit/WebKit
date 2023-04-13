function foo(o_) {
    var o = o_;
    var result = 0;
    for (var s in o) {
        result += o[s];
    }
    return result;
}

noInline(foo);

var global = $vm.createGlobalObject();
global.a = 1;
global.b = 2;
global.c = 3;
global.d = 4;

for (var i = 0; i < 10000; ++i) {
    var result = foo($vm.createGlobalProxy(global));
    if (result != 1 + 2 + 3 + 4)
        throw "Error: bad result: " + result;
}
