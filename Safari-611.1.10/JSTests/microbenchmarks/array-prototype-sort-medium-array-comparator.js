var comparator = (a, b) => a - b;

for (var i = 0; i < 1e5; ++i) {
    [0,1,2,3,4,5,11,10,9,8,7,6,12,13,14,15,16,17,23,22,21,20,19,18].sort(comparator);
}
