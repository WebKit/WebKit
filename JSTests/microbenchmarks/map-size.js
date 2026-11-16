function mapSize(m) {
    return m.size;
}
noInline(mapSize);

var m = new Map();
for (var i = 0; i < 100; ++i)
    m.set(i, i);

var sum = 0;
for (var i = 0; i < 1e6; ++i)
    sum += mapSize(m);
