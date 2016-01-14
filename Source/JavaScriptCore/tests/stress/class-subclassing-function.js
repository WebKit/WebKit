F = class extends Function { }

function test(i) {

    let f = new F("x", "return x + " + i + ";");
    let C = new F("x", "this.x = x; this.i = " + i);

    if (!(f instanceof Function && f instanceof F))
        throw "bad chain";

    if (f(1) !== i+1)
        throw "function was not called correctly";

    let o = new C("hello");
    if (o.x !== "hello" || o.i !== i)
        throw "function as constructor was not correct";

    let g = new F("x", "y", "return this.foo + x + y");
    if (g.call({foo:1}, 1, 1) !== 3)
        throw "function was not .callable";

    let g2 = g.bind({foo:1}, 1);
    if (g2 instanceof F)
        throw "the binding of a subclass should not inherit from the original function";

    if (g2(1) !== 3)
        throw "binding didn't work";

}
noInline(test);

for (i = 0; i < 10000; i++)
    test(i);
