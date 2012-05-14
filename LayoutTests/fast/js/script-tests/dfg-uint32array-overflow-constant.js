description(
"Tests that storing a value that is outside of the int32 range into a Uint32Array results in correct wrap-around."
);

function foo(a) {
    a[0] = 0x8005465c;
}

var array = new Uint32Array(1);

for (var i = 0; i < 200; ++i) {
    foo(array);
    shouldBe("array[0]", "0x8005465c");
}

