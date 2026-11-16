function test(str, n) {
    var sum = 0;
    for (var i = 0; i < n; ++i) {
        var idx = (i * 0x9E3779B9 >>> 0) % str.length;
        var c = str.at(idx);
        sum += c.charCodeAt(0);
    }
    return sum;
}
noInline(test);

var str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

var result = test(str, 5000000);
if (result !== 433632023)
    throw "Error: bad result: " + result;
