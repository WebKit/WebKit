(function() {
    var trapResult = [];
    var proxy = new Proxy({}, {
        ownKeys: function() { return trapResult; },
    });

    var j = 0, lengthSum = 0;
    for (; j < 200_000; ++j)
        lengthSum += Reflect.ownKeys(proxy).length;

    if (lengthSum !== trapResult.length * j)
        throw "Bad assertion!";
})();
