description(
"Tests that DFG respects neutered typed arrays."
);

var array = new Int8Array(100);
array[0] = 42;
shouldBe("array.length", "100");
shouldBe("array[0]", "42");

function foo(array) { return array.length; }
function bar(array) { return array[0]; }

noInline(foo);
noInline(bar);

while (!dfgCompiled({f:[foo, bar]})) {
    foo(array);
    bar(array);
}

shouldBe("foo(array)", "100");
shouldBe("bar(array)", "42");

window.postMessage(array, "*", [array.buffer]);

shouldBe("array.length", "0");
shouldThrow("array[0]");

shouldBe("foo(array)", "0");
shouldThrow("bar(array)");
