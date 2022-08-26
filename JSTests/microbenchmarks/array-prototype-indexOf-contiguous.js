const length = 100;

var target = new Array(100);
for (var i = 0; i < length; i++)
    target[i] = {index: i};

(function() {
    var {indexOf} = Array.prototype;
    var searchElement = target[50];

    for (var j = 0; j < 1e6; j++) {
        if (indexOf.call(target, searchElement) !== 50)
            throw new Error(`Bad assert: ${j}!`);
    }
})();
