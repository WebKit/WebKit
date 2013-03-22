description(
"Checks that the DFG CFA does the right things if it proves that a put_by_id is a simple replace when storing to a specialized function property."
);

function foo(o, v) {
    o.f = v;
}

// Warm up foo's put_by_id to make it look polymorphic.
for (var i = 0; i < 100; ++i)
    foo(i % 2 ? {a: 1} : {b: 2});

function bar(f) {
    foo(this, f);
    return this.f();
}

function baz() {
    debug("baz!");
    return "baz";
}

for (var i = 0; i < 100; ++i)
    shouldBe("bar.call({f:baz}, baz)", "\"baz\"");

function fuzz() {
    debug("fuzz!");
    return "fuzz";
}

shouldBe("bar.call({f:baz}, fuzz)", "\"fuzz\"");

