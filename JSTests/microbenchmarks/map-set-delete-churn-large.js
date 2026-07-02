var m = new Map();
for (var i = 0; i < 8192; ++i)
    m.set("k" + i, i);

var acc = 0;
for (var i = 0; i < 100000; ++i) {
    m.set("t", i);
    m.delete("t");
    acc += m.size;
}
if (acc !== 819200000)
    throw new Error("bad result: " + acc);
