var niters = 100000;

// proxy -> target -> x
function cacheOnTarget() {
    var target = $vm.createGlobalObject();
    target.x = 42;
    var proxy = $vm.createGlobalProxy(target);

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
    var target = $vm.createGlobalObject(proto);
    var proxy = $vm.createGlobalProxy(target);

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
    var target = $vm.createGlobalObject();
    target.x = 42;
    var protoProxy = $vm.createGlobalProxy(target);
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
    var target2 = $vm.createGlobalObject();
    target2.x = 42;
    var protoProxy = $vm.createGlobalProxy(target2);
    var target1 = $vm.createGlobalObject(protoProxy);
    var proxy = $vm.createGlobalProxy(target1);

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
