const window = this;

(function() {
    var obj;

    for (var i = 0; i < 1e6; i++)
        obj = new window.Object();

    if (obj.constructor !== Object)
        throw new Error("Bad assert!");
})();
