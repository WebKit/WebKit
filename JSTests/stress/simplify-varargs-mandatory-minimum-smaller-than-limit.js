function foo() { return 42; }

function bar() { return foo.apply(this, arguments); }

function baz() { return bar(1, 2, 3); }

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz();
    if (result != 42)
        throw "Error: bad result: " + result;
}


