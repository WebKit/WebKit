function foo() {
    var a = new Array(1000);
    for (var i = 0; i < 1000; ++i) {
        if (i % 7 === 0)
            continue;
        a[i] = i;
    }

    var niters = 10000;
    var remove = true;
    var lastRemovedItem = null;
    var lastRemovedIndex = null;
    for (var i = 0; i < niters; ++i) {
        if (remove) {
            lastRemovedIndex = Math.floor(Math.random() * a.length);
            lastRemovedItem = a[lastRemovedIndex];
            a.splice(lastRemovedIndex, 1);
        } else {
            a.splice(lastRemovedIndex, 0, lastRemovedItem);
        }
        remove = !remove;
    }
    if (a.length !== 1000)
        throw new Error("Incorrect length");
};
foo();
