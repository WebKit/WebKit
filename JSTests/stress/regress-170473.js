var heap = new SharedArrayBuffer(4096);
var Uint8ArrayView = new Uint8Array(heap);

function test(k)  {
    var d = new Float32Array();
    var c = d | 0;
    var b = 1 % c;
    var a = b | 0;
    Uint8ArrayView[a] = 0;
}
noInline(test);

for (var k = 0; k < 200; ++k)
    test(k);
