function foo(e) {
    if (e) {
        arguments[0]--;
        return arguments.callee.apply(this, arguments);
    }
}
noInline(foo);

for (var i = 0; i < 10000; i++)
    foo(1);

