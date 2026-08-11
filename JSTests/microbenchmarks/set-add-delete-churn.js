var s = new Set();
for (var i = 0; i < 32; ++i)
    s.add("k" + i);

var acc = 0;
for (var i = 0; i < 1000000; ++i) {
    s.add("t");
    s.delete("t");
    acc += s.size;
}
if (acc !== 32000000)
    throw new Error("bad result: " + acc);
