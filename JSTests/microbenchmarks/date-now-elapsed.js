var sum = 0;
var start = Date.now();
for (var i = 0; i < 1e6; ++i) {
    var diff = Date.now() - start;
    if (diff >= 0 && diff < 1e9)
        sum += diff;
}
