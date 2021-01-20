function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var order = [];

var source = {
    get [Symbol.iterator]()
    {
        order.push(`Symbol.iterator`);
        return `Symbol.iterator`;
    },

    get 1()
    {
        order.push(`1`);
        return `1`;
    },

    get cocoa()
    {
        order.push(`cocoa`);
        return `cocoa`;
    },
};

var result = Object.assign({}, source);
shouldBe(result[1], `1`);
shouldBe(result.cocoa, `cocoa`);
shouldBe(result[Symbol.iterator], `Symbol.iterator`);
shouldBe(order.join(','), `1,cocoa,Symbol.iterator`);
