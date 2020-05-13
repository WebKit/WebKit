function g() {
}
noInline(g);

function foo() {
    g.apply(undefined, arguments);
    this; // Check(OtherUse:@this). This should not escape arguments.
}
noInline(foo);

for (let i = 0; i < 100000; ++i)
    foo.call(undefined);
