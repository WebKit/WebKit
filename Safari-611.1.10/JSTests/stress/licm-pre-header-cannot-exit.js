//@ runFTLNoCJIT("--createPreHeaders=false")

function foo(object, predicate) {
    var result = 0;
    var i = 0;
    if ($vm.dfgTrue())
        predicate = 42;
    while (predicate >= 42) {
        result += object.array[i++];
        if (i >= object.array.length)
            break;
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo({array: [1, 2, 3]}, {valueOf: function() { return 42; }});
