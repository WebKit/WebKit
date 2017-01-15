//@ skip if $memoryLimited
(function() {
    
    function bench(name, f, arg) {
        var start = new Date;
        var result = f(arg);
        var end = new Date;
        const verbose = false;
        if (verbose)
            print(name + " " + result + " " + (end-start) + "ms");
    }

    var denseSet = new Set;
    var excludeSet = [123, 1230, 12300, 123000, 234, 2340, 23400];
    for (var idx = 0; idx < 5e6; ++idx) {
        if (excludeSet.includes(idx))
            continue;
        denseSet.add(idx);
    }

    bench("Dense Set Property Existence", function(s) {
        var count = 0;
        for (var i = 0; i < 5e6; ++i)
            if (s.has(i))
                count++
        return count;
    }, denseSet);

})();
