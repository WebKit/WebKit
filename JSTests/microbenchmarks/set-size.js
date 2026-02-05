function setSize(s) {
    return s.size;
}
noInline(setSize);

var s = new Set();
for (var i = 0; i < 100; ++i)
    s.add(i);

var sum = 0;
for (var i = 0; i < 1e6; ++i)
    sum += setSize(s);
