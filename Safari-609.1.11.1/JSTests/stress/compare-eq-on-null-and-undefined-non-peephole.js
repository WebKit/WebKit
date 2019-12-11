"use strict"

function useForMath(undefinedArgument, nullArgument, polymorphicArgument) {
    var a = (null == undefinedArgument) + (undefinedArgument == null) + (undefined == undefinedArgument) + (undefinedArgument == undefined);
    var b = (null == nullArgument) + (nullArgument == null) + (undefined == nullArgument) + (nullArgument == undefined);
    var c = (null == polymorphicArgument) + (polymorphicArgument == null) + (undefined == polymorphicArgument) + (polymorphicArgument == undefined);
    var d = (5 == null) + (null == true) + (undefined == Math.LN2) + ("const" == undefined);
    var e = (5 == undefinedArgument) + (nullArgument == true) + (nullArgument == Math.LN2) + ("const" == undefinedArgument);

    return a + b - c + d - e;
}
noInline(useForMath);

function testUseForMath() {
    for (let i = 0; i < 1e4; ++i) {
        var value = useForMath(undefined, null, 5);
        if (value != 8)
            throw "Failed useForMath(undefined, null, 5), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, null);
        if (value != 4)
            throw "Failed useForMath(undefined, null, null), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, undefined);
        if (value != 4)
            throw "Failed useForMath(undefined, null, undefined), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, { foo: "bar" });
        if (value != 8)
            throw "Failed useForMath(undefined, null, { foo: \"bar\" }), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, true);
        if (value != 8)
            throw "Failed useForMath(undefined, null, true), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, [1, 2, 3]);
        if (value != 8)
            throw "Failed useForMath(undefined, null, true), value = " + value + " with i = " + i;

        var value = useForMath(undefined, null, "WebKit!");
        if (value != 8)
            throw "Failed useForMath(undefined, null, true), value = " + value + " with i = " + i;
    }
}
testUseForMath();