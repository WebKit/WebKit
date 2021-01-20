function assert(b) {
    if (!b)
        throw new Error;

}

const originalLength = 10000;
let arr = new Proxy([], {
    has(...args) {
        assert(parseInt(args[1]) < originalLength);
        assert(args[0].length - 10 === originalLength);
        return Reflect.has(...args);
    }
});

for (var i = 0; i < originalLength; i++)
    arr[i] = [];

arr.lastIndexOf(new Object(), {
    valueOf: function () {
        arr.length += 10;
        return 0;
    }
});
