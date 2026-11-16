description(
"Tests what happens in the Baseline JIT if we load from a Float32 array that contains a weirdly formatted NaN."
);

var buffer = new ArrayBuffer(4);

var int32View = new Int32Array(buffer);
var floatView = new Float32Array(buffer);

int32View[0] = -4587521;

debug(int32View[0]);

for (var i = 0; i < 100; ++i) {
    debug(floatView[0]);
    shouldBe("\"\" + floatView[0]", "\"NaN\"");
}

debug("I didn't crash, which is a good thing.");
