(function() {
    var array = new Array(300);
    for (var i = 0; i < 300; i++)
        array[i] = i % 10;

    var compacted;
    for (var j = 0; j < 30000; j++)
        compacted = array.filter(Boolean);

    if (compacted.length !== 270)
        throw new Error("Bad assert!");
})();
