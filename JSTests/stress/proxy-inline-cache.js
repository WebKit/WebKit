var niters = 100000;

// proxy -> target -> x
function cacheOnTarget() {
    var target = {x:42};
    var proxy = createProxy(target);

    var getX = function(o) { return o.x; };
    noInline(getX);

    var sum = 0;
    for (var i = 0; i < niters; ++i)
        sum += getX(proxy);

    if (sum != 42 * niters)
        throw new Error("Incorrect result");
};

// proxy -> target -> proto -> x
function cacheOnPrototypeOfTarget() {
    var proto = {x:42};
    var target = Object.create(proto);
    var proxy = createProxy(target);

    var getX = function(o) { return o.x; };
    noInline(getX);

    var sum = 0;
    for (var i = 0; i < niters; ++i)
        sum += getX(proxy);

    if (sum != 42 * niters)
        throw new Error("Incorrect result");
};

// base -> proto (proxy) -> target -> x
function dontCacheOnProxyInPrototypeChain() {
    var target = {x:42};
    var protoProxy = createProxy(target);
    var base = Object.create(protoProxy);

    var getX = function(o) { return o.x; };
    noInline(getX);

    var sum = 0;
    for (var i = 0; i < niters; ++i)
        sum += getX(base);

    if (sum != 42 * niters)
        throw new Error("Incorrect result");
};

// proxy -> target 1 -> proto (proxy) -> target 2 -> x
function dontCacheOnTargetOfProxyInPrototypeChainOfTarget() {
    var target2 = {x:42};
    var protoProxy = createProxy(target2);
    var target1 = Object.create(protoProxy);
    var proxy = createProxy(target1);

    var getX = function(o) { return o.x; };
    noInline(getX);

    var sum = 0;
    for (var i = 0; i < niters; ++i)
        sum += getX(proxy);

    if (sum != 42 * niters)
        throw new Error("Incorrect result");
};

cacheOnTarget();
cacheOnPrototypeOfTarget();
dontCacheOnProxyInPrototypeChain();
dontCacheOnTargetOfProxyInPrototypeChainOfTarget();
