function opt(a1, a2, a3, a4, a5, a6, a7) {
    let ws = new WeakSet();
    let key = {};
    ws.add(key);
    ws.delete(key);
    var array = new Array(3);
    array[0] = a6;
    array[1] = key;
    if (a4)
        return array;
    let generator = (async function*() {
        let { getMonth: a8 = a4 } = a1;
        yield array;
        yield await generator;
    })();
    generator.next();
    generator.return(ws).then(function() {
        (function() {
            class C extends NotDefined { }
        })();
    });
    drainMicrotasks();
}

for (let i = 0; i < testLoopCount * 20; i++) {
    try {
        opt("a💩b", 4.725232529096e-312, 2.2, Math.E, "aaaaaaaaa", 0x80000000, JSON.stringify);
    } catch (e) { }
}

// The crashing compile is the OSR entry into the loop above, and it only reproduces with a call after the loop.
print("done");
