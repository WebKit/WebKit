// Test tryGetById's value profiling feedback after it's too polymorphic.

var createBuiltin = $vm.createBuiltin;

var it = 1e5;

const check = (got, expect) => { if (got != expect) throw "Error: bad result got " + got + " expected " + expect; };

fooPlusBar = createBuiltin(`(function (o) { return @tryGetById(o, "foo") + @tryGetById(o, "bar"); })`);
noInline(fooPlusBar);

const bench = f => { f(); }
noInline(bench);

// Non bool int32.
o = { foo: 42, bar: 1337 };
bench(() => { var res = 0; for (var i = 0; i < it; ++i) res += fooPlusBar(o);  check(res, (o.foo + o.bar) * it); });

// Non int double.
p = { foo: Math.PI, bar: Math.E };
bench(() => { var res = 0.; for (var i = 0; i < it; ++i) res += fooPlusBar(p); check(Math.round(res), Math.round((p.foo + p.bar) * it)); });

// String ident.
// This gets too polymorphic for the engine's taste.
s = { foo: "", bar: "⌽" };
bench(() => { var res = ""; for (var i = 0; i < it; ++i) res += fooPlusBar(s); check(res.length, (s.foo.length + s.bar.length) * it); });

// Again: non bool int32.
bench(() => { var res = 0; for (var i = 0; i < it; ++i) res += fooPlusBar(o); check(res, (o.foo + o.bar) * it); });
