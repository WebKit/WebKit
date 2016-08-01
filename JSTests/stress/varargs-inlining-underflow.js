function baz() {
}

function bar() {
    baz.apply(this, arguments);
}

for (var i = 0; i < 1000; ++i)
    bar(1, 2, 3, 4, 5, 6, 7);

function foo() {
    bar();
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo();
