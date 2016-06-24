var sum = 0;
var array = [1, 2, 3];
for (var i = 0; i < 1e5; ++i) {
    array.forEach(function (value) {
        sum += value;
    });
}
