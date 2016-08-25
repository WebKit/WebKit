// Test tryGetById's value profiling feedback without going too polymorphic.

var it = 1e5;

const check = (got, expect) => { if (got != expect) throw "Error: bad result got " + got + " expected " + expect; };

const bench = f => {
    // Re-create the builtin each time, so each benchmark gets its own value prediction.
    const fooPlusBar = createBuiltin(`(function(o) { return @tryGetById(o, "foo") + @tryGetById(o, "bar"); })`);
    noInline(fooPlusBar);
    f(fooPlusBar);
}
noInline(bench);

// Non bool int32.
o = { foo: 42, bar: 1337 };
bench(builtin => { var res = 0; for (var i = 0; i < it; ++i) res += builtin(o);  check(res, (o.foo + o.bar) * it); });

// Non int double.
p = { foo: Math.PI, bar: Math.E };
bench(builtin => { var res = 0.; for (var i = 0; i < it; ++i) res += builtin(p); check(Math.round(res), Math.round((p.foo + p.bar) * it)); });

// String ident.
s = { foo: "", bar: "⌽" };
bench(builtin => { var res = ""; for (var i = 0; i < it; ++i) res += builtin(s); check(res.length, (s.foo.length + s.bar.length) * it); });

// Again: non bool int32.
bench(builtin => { var res = 0; for (var i = 0; i < it; ++i) res += builtin(o); check(res, (o.foo + o.bar) * it); });
