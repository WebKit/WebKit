function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const handler = {
  get : function (t, p, r) { return Reflect.get(t, p, r); },
  has : function (t, p, r) { return Reflect.has(t, p, r); }
};

const proxyArray = new Proxy([5, 6], handler);

const array = [1, 2, 3, 4, proxyArray, 7, 8];
shouldBe(array.length, 7);

const flattened = array.flat();
shouldBe(flattened.length, 8);
shouldBe(flattened[0], 1);
shouldBe(flattened[1], 2);
shouldBe(flattened[2], 3);
shouldBe(flattened[3], 4);
shouldBe(flattened[4], 5);
shouldBe(flattened[5], 6);
shouldBe(flattened[6], 7);
shouldBe(flattened[7], 8);
