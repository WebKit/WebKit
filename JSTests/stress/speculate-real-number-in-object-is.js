function test() {
    function object_is_opt(value) {
        const tmp = {p0: value};

        if (Object.is(value, NaN))
            return 0;

        return value;
    }

    object_is_opt(NaN);

    for (let i = 0; i < 0x20000; i++)
        object_is_opt(1.1);

    return isNaN(object_is_opt(NaN));
}

resultIsNaN = test();
if (resultIsNaN)
    throw "FAILED";

