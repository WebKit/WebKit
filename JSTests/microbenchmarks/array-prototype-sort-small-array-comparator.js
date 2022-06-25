var comparator = (a, b) => a - b;

for (var i = 0; i < 1e5; ++i) {
    [0,1,2,3,7,6,5,4].sort(comparator);
}
