(function() {
    var target = {};
    for (var i = 0; i < 200; i++)
        target["k" + i] = i;

    var proxy = new Proxy(target, {});
    var x = 0;

    for (var j = 0; j < 1200; j++) {
        for (var k in proxy) {
            if (k === "k100")
                x++;
        }
    }

    if (x !== 1200)
        throw new Error("Bad assertion!");
})();
