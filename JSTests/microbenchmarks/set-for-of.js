var globalSum = 0;
(function () {
    function createSet(count) {
        var set = new Set;
        for (var i = 0; i < count; i++) {
            set.add(i);
        }
        return set;
    }

    var COUNT = 2000;
    var set = createSet(COUNT);
    var sum = 0;

    for (var i = 0; i < 1e2; ++i) {
        sum = 0;
        for (var item of set) {
            sum += item;
        }
        globalSum = sum;
    }
    return sum;
}());
