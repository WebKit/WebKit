function foo() {
    var o = {f:42};
    var result = 0;
    OSRExit();
    for (var i = 0; i < 10000; ++i) {
        if (!$vm.dfgTrue())
            result += o.f;
    }
    return result;
}

for (var i = 0; i < 1000; ++i) {
    foo();
    fullGC();
}

