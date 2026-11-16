(function() {
    var target = {};
    for (var i = 0; i < 10; i++)
        target["k" + i] = i;
    var trapResult = Reflect.ownKeys(target);
    var proxy = new Proxy(target, {
        ownKeys: function() { return trapResult; },
    });

    var j = 0, lengthSum = 0;
    for (; j < 50_000; ++j)
        lengthSum += Reflect.ownKeys(proxy).length;

    if (lengthSum !== trapResult.length * j)
        throw "Bad assertion!";
})();
