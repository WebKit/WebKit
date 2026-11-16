function sumEntries(map) {
    var sum = 0;
    map.forEach(function (value, key) {
        sum += value + key;
    });
    return sum;
}
noInline(sumEntries);

var map = new Map;
for (var i = 0; i < 1000; ++i)
    map.set(i, i * 2);

for (var i = 0; i < 1e4; ++i)
    sumEntries(map);
