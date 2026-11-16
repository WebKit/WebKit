
function shouldBe(actual, expected) {
    if (String(actual) !== String(expected))
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

{
    let getSum = 0;
    let obj = {
        p1: "p1"
    };
    let obj1 = Object.assign({}, obj);
    let expected = "p1";
    shouldBe(Object.values(obj), expected);
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 0);
}

{
    let getSum = 0;
    let obj = {
        get 1() {
            getSum += 1;
            return 1;
        },
        set 2(value) {
            return 2;
        },
    };
    let obj1 = Object.assign({}, obj);      // get 1
    let expected = [1, undefined];
    shouldBe(Object.values(obj), expected); // get 1
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 2);
}

{
    let getSum = 0;
    let obj = {
        get 5000() {
            getSum += 5000;
            return 5000;
        },
        set 6000(value) {
            return 6000;
        },
    };
    let obj1 = Object.assign({}, obj);      // get 5000
    let expected = [5000, undefined];
    shouldBe(Object.values(obj), expected); // get 5000
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 10000);
}

{
    let getSum = 0;
    let obj = {};
    Object.defineProperty(obj, "a", {
        value: 'a',
        enumerable: true,
    });
    Object.defineProperty(obj, 3, {
        value: '3',
        enumerable: false,
    });

    let obj1 = Object.assign({}, obj);
    let expected = 'a';
    shouldBe(Object.values(obj), expected);
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 0);
}

{
    let getSum = 0;
    let obj = {};
    Object.defineProperty(obj, 4, {
        get: function () {
            getSum += 4;
            return 4;
        }
    });
    Object.defineProperty(obj, 5, {
        set: function (value) {
            return 5;
        },
        enumerable: true,
    });
    Object.defineProperty(obj, 6, {
        get: function () {
            getSum += 6;
            return 6;
        },
        enumerable: true,
    });

    let obj1 = Object.assign({}, obj);      // get 6
    let expected = [undefined, 6];
    shouldBe(Object.values(obj), expected); // get 6
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 12);
}

{
    let getSum = 0;
    let obj = {};
    Object.defineProperty(obj, 1000, {
        value: 1000,
        writable: false,
        enumerable: true,
    });
    Object.defineProperty(obj, 2000, {
        value: 2000,
        enumerable: false,
    });

    let obj1 = Object.assign({}, obj);
    let expected = [1000];
    shouldBe(Object.values(obj), expected);
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 0);
}

{
    let getSum = 0;
    let obj = {};
    Object.defineProperty(obj, 3000, {
        get: function () {
            getSum += 3000;
            return 3000;
        }
    });
    Object.defineProperty(obj, 4000, {
        set: function (value) {
            return 4000;
        },
        enumerable: true,
    });
    Object.defineProperty(obj, 5000, {
        get: function () {
            getSum += 5000;
            return 5000;
        },
        enumerable: true,
    });

    let obj1 = Object.assign({}, obj);      // get 5000
    let expected = [undefined, 5000];
    shouldBe(Object.values(obj), expected); // get 5000
    shouldBe(Object.values(obj1), expected);
    shouldBe(getSum, 10000);
}
