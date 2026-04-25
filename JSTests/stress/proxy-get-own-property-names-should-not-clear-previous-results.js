function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

a = {defineProperties:Object};
function opt() {
    a.__proto__ = new Proxy({}, {ownKeys:opt});
    return [];
}
for(var i=0;i<400;i=i+1) {
    var prop = null;
    var count = 0;
    for (t in a) {
        opt();
        prop = t;
        ++count;
    }
    shouldBe(prop, "defineProperties");
    shouldBe(count, 1);
}
