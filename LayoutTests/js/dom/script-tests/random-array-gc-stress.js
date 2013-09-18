description(
'Tests to randomly modify an object graph of arrays to make sure the GC handles them properly now that it allocates both the JSArray and the ArrayStorage.  To pass we need to not crash.'
);

var global = [];

var getRandomIndex = function(length) {
    return Math.floor(Math.random() * length);
};

var test = function() {
    var numActions = 4;
    var iters = 1000000;
    var percent = 0;

    for (var i = 0; i < iters; i++) {
        var r = Math.floor(Math.random() * numActions);
        var actionNum = r % numActions;
        if (actionNum === 0 || actionNum === 3) {
            global.push([]);
        } else if (actionNum === 1) {
            global.pop(getRandomIndex(global.length));
        } else if (actionNum === 2) {
            if (global.length > 1) {
            var receivingIndex = getRandomIndex(global.length);
            var sendingIndex = getRandomIndex(global.length);
            while (receivingIndex === sendingIndex)
                sendingIndex = getRandomIndex(global.length);
            global[receivingIndex].push(global[sendingIndex]);
            } else
                continue;
        } 

        if (Math.floor((i / iters) * 100) !== percent) {
            percent = Math.floor((i / iters) * 100);
        }
    }
};

var runs = 0; 
while (runs < 10) {
    test();
    runs += 1;
    global = [];
    gc();
}
