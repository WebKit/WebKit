const length = 100;

var target = new Array(100);
for (var i = 0; i < length; i++)
    target[i] = {index: i};

(function() {
    var el;

    for (var j = 0; j < 1e6; j++) {
        for (var i = 0; i < length / 2; i++)
            el = target[i];
    }

    if (el.index + 1 !== length / 2)
        throw new Error(`Bad assert!`);
})();
