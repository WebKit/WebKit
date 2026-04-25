description(
"Tests that byteOffset does the right thing for neutered views."
);

function foo(o) {
    return o.byteOffset;
}

var array = new Int8Array(new ArrayBuffer(100));

noInline(foo);
while (!dfgCompiled({f:foo}))
    foo(array);

window.postMessage(array, "*", [array.buffer]);

shouldBe("foo(array)", "0");

