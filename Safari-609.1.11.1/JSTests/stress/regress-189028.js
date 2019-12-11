function assert(x, y) {
    if (x != y) {
        $vm.print("actual: ", x);
        $vm.print("expected: ", y);
        throw "FAILED\n" + new Error().stack;
    }
}

(function() {
    let arr = [1.1, 2.2];
    let arr2 = [1.1, 2.2];

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "CopyOnWriteArrayWithDouble");

    let o = $vm.createGlobalObject();

    $vm.haveABadTime(o);

    let proto = new o.Object();
    assert($vm.isHavingABadTime(o), true);
    assert($vm.isHavingABadTime(proto), true);

    arr2.__proto__ = proto;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "ArrayWithSlowPutArrayStorage");
})();

gc();

(function() {
    let arr = [1.1, 2.2];
    let arr2 = [1.1, 2.2];

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "CopyOnWriteArrayWithDouble");

    let o = $vm.createGlobalObject();

    let proto = new o.Object();
    assert($vm.isHavingABadTime(o), false);
    assert($vm.isHavingABadTime(proto), false);

    arr2.__proto__ = proto;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "ArrayWithDouble");

    $vm.haveABadTime(o);

    assert($vm.isHavingABadTime(o), true);
    assert($vm.isHavingABadTime(proto), true);

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "ArrayWithSlowPutArrayStorage");
})();

gc();

(function() {
    let arr = [1.1, 2.2];
    let arr2 = {};

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArray");

    let o = $vm.createGlobalObject();

    $vm.haveABadTime(o);

    let proto = new o.Object();
    assert($vm.isHavingABadTime(o), true);
    assert($vm.isHavingABadTime(proto), true);

    arr2.__proto__ = proto;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArray");

    arr2[0] = 1.1;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArrayWithSlowPutArrayStorage");
})();

gc();

(function() {
    let arr = [1.1, 2.2];
    let arr2 = {};

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArray");

    let o = $vm.createGlobalObject();
    let proto = new o.Object();

    assert($vm.isHavingABadTime(o), false);
    assert($vm.isHavingABadTime(proto), false);

    arr2.__proto__ = proto;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArray");

    arr2[0] = 1.1;

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArrayWithDouble");

    $vm.haveABadTime(o);

    assert($vm.isHavingABadTime(o), true);
    assert($vm.isHavingABadTime(proto), true);

    assert($vm.isHavingABadTime(arr), false);
    assert($vm.indexingMode(arr), "CopyOnWriteArrayWithDouble");
    assert($vm.isHavingABadTime(arr2), false);
    assert($vm.indexingMode(arr2), "NonArrayWithSlowPutArrayStorage");
})();

gc();

(function() {
    let g0 = $vm.createGlobalObject();
    let o0 = new g0.Object(); 
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(o0), false);

    let g1 = $vm.createGlobalObject();
    let o1 = new g1.Object();
    assert($vm.isHavingABadTime(g1), false);
    assert($vm.isHavingABadTime(o1), false);

    let g2 = $vm.createGlobalObject();
    assert($vm.isHavingABadTime(g2), false);

    $vm.haveABadTime(g1);
    assert($vm.isHavingABadTime(g1), true);

    o1.__proto__ = null;
    g2.Array.prototype.__proto__ = o1;
    o0.__proto__ = o1;

    assert($vm.indexingMode(o0), "NonArray");
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(g2), true);
})();

gc();

(function() {
    let g0 = $vm.createGlobalObject();
    let o0 = new g0.Object(); 
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(o0), false);

    let g1 = $vm.createGlobalObject();
    let o1 = new g1.Object();
    assert($vm.isHavingABadTime(g1), false);
    assert($vm.isHavingABadTime(o1), false);

    let g2 = $vm.createGlobalObject();
    assert($vm.isHavingABadTime(g2), false);

    o1.__proto__ = null;
    g2.Array.prototype.__proto__ = o1;
    o0.__proto__ = o1;
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(g1), false);
    assert($vm.isHavingABadTime(g2), false);

    $vm.haveABadTime(g1);

    assert($vm.indexingMode(o0), "NonArray");
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(g1), true);
    assert($vm.isHavingABadTime(g2), true);
})();

gc();

(function() {
    let g0 = $vm.createGlobalObject();
    let o0 = new g0.Object(); 
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(o0), false);

    let g1 = $vm.createGlobalObject();
    let o1 = new g1.Object();
    assert($vm.isHavingABadTime(g1), false);
    assert($vm.isHavingABadTime(o1), false);

    let g2 = $vm.createGlobalObject();
    let o2 = new g2.Object();
    assert($vm.isHavingABadTime(g2), false);
    assert($vm.isHavingABadTime(o2), false);

    let g3 = $vm.createGlobalObject();
    assert($vm.isHavingABadTime(g3), false);

    o1.__proto__ = null;
    g2.Array.prototype.__proto__ = o1;
    o2.__proto__ = o1;
    g3.Array.prototype.__proto__ = o2;
    o0.__proto__ = o1;
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(g1), false);
    assert($vm.isHavingABadTime(g2), false);
    assert($vm.isHavingABadTime(g3), false);

    $vm.haveABadTime(g1);

    assert($vm.indexingMode(o0), "NonArray");
    assert($vm.isHavingABadTime(g0), false);
    assert($vm.isHavingABadTime(g1), true);
    assert($vm.isHavingABadTime(g2), true);
    assert($vm.isHavingABadTime(g2), true);
})();