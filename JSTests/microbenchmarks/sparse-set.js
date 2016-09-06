(function() {
    
    function bench(name, f, arg) {
        var start = new Date;
        var result = f(arg);
        var end = new Date;
        const verbose = false;
        if (verbose)
            print(name + " " + result + " " + (end-start) + "ms");
    }


    var sparseSet = new Set;
    for (var x of [123, 1230, 12300, 123000, 234, 2340, 23400]) {
        sparseSet.add(x);
    }

    bench("Sparse Set Property Existence", function(s) {
        var count = 0;
        for (var i = 0; i < 5e6; ++i)
            if (s.has(i))
                count++
        return count;
    }, sparseSet);
})();
