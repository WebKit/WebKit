const length = 100;

var target = new Array(length);
for (var i = 0; i < length; i++)
    target[i] = {index: i};

(function() {
    var {slice} = Array.prototype;
    var lastSlice;

    for (var j = 0; j < 1e6; j++)
        lastSlice = slice.call(target);

    if (lastSlice.pop().index !== 99)
        throw new Error(`Bad assert!`);
})();
