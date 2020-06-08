//@ if $buildType == "release" then runDefault else skip end

function shouldThrowStackOverflow(fn) {
    let caught;
    try {
        fn();
    } catch (error) {
        caught = error;
    }

    if (!caught)
        throw new Error("Did not throw");
    if (String(caught) !== "RangeError: Maximum call stack size exceeded.")
        throw new Error(`Expected stack overflow error, but got: ${caught}`);
}

{
    const obj = {};
    for (let nextObj = obj, i = 0; i < 1e6; ++i)
        nextObj = nextObj[`k${i}`] = {};

    shouldThrowStackOverflow(() => {
        JSON.stringify(obj);
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify([1, 2, [obj]]);
    });
}

{
    const arr = {};
    for (let nextArr = arr, i = 0; i < 1e6; ++i)
        nextArr = nextArr[0] = [];

    shouldThrowStackOverflow(() => {
        JSON.stringify(arr);
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify({a: {b: arr}});
    });
}

{
    const obj = {
        toJSON() {
            return {key: obj};
        },
    };

    shouldThrowStackOverflow(() => {
        JSON.stringify(obj);
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify({a: {b: obj}});
    });
}

{
    const arr = [];
    arr.toJSON = () => [arr];

    shouldThrowStackOverflow(() => {
        JSON.stringify(arr);
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify([[1, 2, arr]]);
    });
}

{
    const obj = {};

    shouldThrowStackOverflow(() => {
        JSON.stringify(null, () => {
            return {key: obj};
        });
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify({a: {b: 1}}, (key, val) => {
            return key === "b" ? {b: obj} : val;
        });
    });
}

{
    const arr = [];

    shouldThrowStackOverflow(() => {
        JSON.stringify(null, () => {
            return [arr];
        });
    });

    shouldThrowStackOverflow(() => {
        JSON.stringify([[1, 2, 3]], (key, val) => {
            return key === "2" ? [1, 2, arr] : val;
        });
    });
}
