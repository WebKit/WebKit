description(
"Tests what happens when you do a negative out-of-bounds access on a string while the prototype has a negative indexed property."
);

function foo(s) {
    return s[-1];
}

noInline(foo);
silentTestPass = true;

String.prototype[-1] = "hello";

for (var i = 0; i < 2; i = dfgIncrement({f:foo, i:i + 1, n:1, compiles:2}))
    shouldBe("foo(\"hello\")", "\"hello\"");

