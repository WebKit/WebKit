function sumValues(set) {
    var sum = 0;
    set.forEach(function (value) {
        sum += value;
    });
    return sum;
}
noInline(sumValues);

var set = new Set;
for (var i = 0; i < 1000; ++i)
    set.add(i);

for (var i = 0; i < 1e4; ++i)
    sumValues(set);
