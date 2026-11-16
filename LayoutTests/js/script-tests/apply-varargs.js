description('Test that we properly fill in missing args with "undefined" in JIT code.');

// Regression test for <rdar://problem/10763509>


function callee(a1, a2, a3, a4, a5, a6, a7, a8)
{
    // We expect that the unused actual parameters will be filled
    // with undefined.
    if (a1 !== undefined)
        return "Arg1 is wrong";
    if (a2 !== undefined)
        return "Arg2 is wrong";
    if (a3 !== undefined)
        return "Arg3 is wrong";
    if (a4 !== undefined)
        return "Arg4 is wrong";
    if (a5 !== undefined)
        return "Arg5 is wrong";
    if (a6 !== undefined)
        return "Arg6 is wrong";
    if (a7 !== undefined)
        return "Arg7 is wrong";
    if (a8 !== undefined)
        return "Arg8 is wrong";

    return undefined;
}

function dummy(a1, a2, a3, a4, a5, a6, a7, a8)
{
}

function BaseObj()
{
}

function caller(testArgCount)
{
    var baseObj = new BaseObj();

    var allArgs = [0, "String", callee, true, null, 2.5, [1, 2, 3], {'a': 1, 'b' : 2}];
    argCounts = [8, testArgCount];

    for (argCountIndex = 0; argCountIndex < argCounts.length; argCountIndex++) {
        argCount = argCounts[argCountIndex];

        var varArgs = [];
        for (i = 0; i < argCount; i++)
            varArgs[i] = undefined;

        for (numCalls = 0; numCalls < 10; numCalls++) {
            // Run multiple times so that the JIT kicks in
            dummy.apply(baseObj, allArgs);
            var result = callee.apply(baseObj, varArgs);
            if (result != undefined)
                return result;
        }
    }

    return undefined;
}

shouldBe("caller(0)", 'undefined');
shouldBe("caller(1)", 'undefined');
shouldBe("caller(2)", 'undefined');
shouldBe("caller(3)", 'undefined');
shouldBe("caller(4)", 'undefined');
shouldBe("caller(5)", 'undefined');
shouldBe("caller(6)", 'undefined');
shouldBe("caller(7)", 'undefined');
shouldBe("caller(8)", 'undefined');

var successfullyParsed = true;
