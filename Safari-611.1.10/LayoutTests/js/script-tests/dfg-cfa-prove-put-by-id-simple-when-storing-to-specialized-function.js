description(
"Checks that the DFG CFA does the right things if it proves that a put_by_id is a simple replace when storing to a specialized function property."
);

silentTestPass = true;

function foo(o, v) {
    o.func = v;
}

// Warm up foo's put_by_id to make it look polymorphic.
for (var i = 0; i < 100; ++i)
    foo(i % 2 ? {a: 1} : {b: 2});

function bar(f) {
    foo(this, f);
    return this.func();
}

function baz() {
    return "baz";
}

noInline(bar);
noInline(baz);

while (!dfgCompiled({f:bar}))
    shouldBe("bar.call({func:baz}, baz)", "\"baz\"");

function fuzz() {
    return "fuzz";
}

noInline(fuzz);

shouldBe("bar.call({func:baz}, fuzz)", "\"fuzz\"");

