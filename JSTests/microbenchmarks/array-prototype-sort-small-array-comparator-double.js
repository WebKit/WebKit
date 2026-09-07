var comparator = (a, b) => a - b;
var source = [0.5, 1.5, 2.5, 3.5, 7.5, 6.5, 5.5, 4.5];

for (var i = 0; i < 1e6; ++i) {
    Array.from(source).sort(comparator);
}
