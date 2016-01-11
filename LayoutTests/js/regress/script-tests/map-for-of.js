var globalSum = 0;
(function () {
    function createMap(count) {
        var map = new Map;
        for (var i = 0; i < count; i++) {
            map.set(i, i);
        }
        return map;
    }

    var COUNT = 2000;
    var map = createMap(COUNT);
    var sum = 0;

    for (var i = 0; i < 1e2; ++i) {
        sum = 0;
        for (var item of map) {
            sum += item[0];
        }
        globalSum = sum;
    }
    return sum;
}());
