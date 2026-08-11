var m = new Map();
for (var i = 0; i < 32; ++i)
    m.set("k" + i, i);

var acc = 0;
for (var i = 0; i < 1000000; ++i) {
    m.set("t", i);
    m.delete("t");
    acc += m.size;
}
if (acc !== 32000000)
    throw new Error("bad result: " + acc);
