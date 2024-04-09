// BigInt64 - BigInt64
{
    function foo1(a, b) {
        let left = a + b;
        let right = 1n;
        return left == right;
    }

    function foo2(a, b) {
        let left = a + 1n;
        let right = b + 2n;
        return left == right;
    }

    for (var i = 0; i < 1e5; ++i) {
        foo1(1n, 2n);
        foo2(2n, 1n);
    }
}

// BigInt64 - String

// BigInt64 - Number

// BigInt64 - Boolean

// BigInt64 - Object

// BigInt64 - Symbol

