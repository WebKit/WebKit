function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} should be ${String(expected)}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)} expected ${errorMessage}`);
}

// ----- Use the fast path ----- 
function argumentLength() {
    shouldBe(arguments.length, 2);
}
argumentLength(1, 2);

function argumentsWithNestedNormalFunction() {
    function nested() {
        var arguments = 0;
    }
    nested();
    shouldBe(arguments.length, 2);
}
argumentsWithNestedNormalFunction(1, 2);

function declareLexicalArgumentsWithBlock() {
    {
        let arguments = 1;
    }
    shouldBe(arguments.length, 3);
}
declareLexicalArgumentsWithBlock(1, 2, 3);

function forInLetArguments() {
    for (let arguments in [1, 2]) {
    }
    shouldBe(arguments.length, 2);
}
forInLetArguments(1, 2);

function forOfLetArguments() {
    for (let arguments of [1, 2]) {
    }
    shouldBe(arguments.length, 0);
}
forOfLetArguments();

function argumentsDotLengthDot() {
    arguments.length.p1 = 1;
    shouldBe(arguments.length.p1, undefined);
}
argumentsDotLengthDot();

function argumentsDotLengthOpenBracket() {
    arguments.length["p1"] = 1;
    shouldBe(arguments.length["p1"], undefined);
}
argumentsDotLengthOpenBracket();

function assignArgumentsDotLengthDot() {
    let a = arguments.length.p1;
    shouldBe(a, undefined);
}
assignArgumentsDotLengthDot();

function assignArgumentsDotLengthOpenBracket() {
    let a = arguments.length["p1"];
    shouldBe(a, undefined);
}
assignArgumentsDotLengthOpenBracket();

function callArgumentsLength() {
    arguments.length();
}
shouldThrow(callArgumentsLength, `TypeError: arguments.length is not a function. (In 'arguments.length()', 'arguments.length' is 0)`);

function callArgumentsLengthWithArgs() {
    arguments.length(1, 2, 3, 4, 5);
}
shouldThrow(callArgumentsLengthWithArgs, `TypeError: arguments.length is not a function. (In 'arguments.length(1, 2, 3, 4, 5)', 'arguments.length' is 0)`);

// ----- Should use the fast path but currently not supported ----- 
function declareArrowFunctionArgumentsWithBlock() {
    {
        let arguments = () => { };
        arguments();
    }
    shouldBe(arguments.length, 3);
}
declareArrowFunctionArgumentsWithBlock(1, 2, 3);

// ----- Shouldn't use the fast path ----- 
function argumentsNonLengthAccess() {
    arguments.callee;
    shouldBe(arguments.length, 1);
}
argumentsNonLengthAccess(1);

function argumentsLengthAssignment() {
    arguments.length = 3;
    shouldBe(arguments.length, 3);
}
argumentsLengthAssignment(1);

function argumentsAssignmentLHS() {
    arguments = 0;
    shouldBe(arguments.length, undefined);
}
argumentsAssignmentLHS();

function argumentsAssignmentRHS() {
    var a = arguments;
    shouldBe(arguments.length, 0);
}
argumentsAssignmentRHS();

function argumentsLengthIncAndDec() {
    arguments.length++;
    arguments.length--;
    ++arguments.length;
    --arguments.length;
    shouldBe(arguments.length, 0);
}
argumentsLengthIncAndDec();

function argumentsDestructuringAssignment() {
    [arguments.length, arguments.length] = [1, 1];
    shouldBe(arguments.length, 1);
}
argumentsDestructuringAssignment();

function declareVarArguments() {
    var arguments = 0;
    shouldBe(arguments.length, undefined);
}
declareVarArguments();

function declareVarArgumentsWithBlock() {
    {
        var arguments = 0;
    }
    shouldBe(arguments.length, undefined);
}
declareVarArgumentsWithBlock();

function declareLexicalArguments() {
    var arguments = 0;
    shouldBe(arguments.length, undefined);
}
declareLexicalArguments();

function declareArrowFunctionArguments() {
    var arguments = () => { };
    arguments();
    shouldBe(arguments.length, 0);
}
declareArrowFunctionArguments(1, 2, 3);

function declareFunctionArguments() {
    function arguments() { }
    arguments();
    shouldBe(arguments.length, 0);
}
declareFunctionArguments();

function declareFunctionArgumentsWithBlock() {
    {
        function arguments() { }
        arguments();
    }
    shouldBe(arguments.length, 0);
}
declareFunctionArgumentsWithBlock();

function declareParameterArguments(arguments) {
    shouldBe(arguments.length, undefined);
}
declareParameterArguments(1);

function returnArguments() {
    shouldBe(arguments.length, 1);
    return arguments;
}
returnArguments(1);

function helper() { }
function passArgumentsToFunctionCall() {
    helper(arguments);
    shouldBe(arguments.length, 1);
}
passArgumentsToFunctionCall(1);

function ifArguments() {
    if (arguments) { }
    shouldBe(arguments.length, 1);
}
ifArguments(1);

function forInArguments() {
    for (arguments in [1, 2]) {
    }
    shouldBe(arguments, "1");
    shouldBe(arguments.length, 1);
}
forInArguments(1, 2);

function forInVarArguments() {
    for (var arguments in [1, 2]) {
    }
    shouldBe(arguments.length, 1);
}
forInVarArguments(1, 2);

function forOfArguments() {
    for (arguments of [1, 2]) {
    }
    shouldBe(arguments.length, undefined);
}
forOfArguments();

function forOfVarArguments() {
    for (var arguments of [1, 2]) {
    }
    shouldBe(arguments.length, undefined);
}
forOfVarArguments();

function withArguments() {
    with (arguments) { }
    shouldBe(arguments.length, 1);
}
withArguments(1);

function argumentsWithNestedFunctions() {
    var nestedArrowFunction = () => {
        shouldBe(arguments.length, 1);
    };
    nestedArrowFunction(1, 2);

    var nestedAsyncArrowFunction = () => {
        shouldBe(arguments.length, 1);
        arguments = 1;
    };
    nestedAsyncArrowFunction(1, 2);

    shouldBe(arguments.length, undefined);
}
argumentsWithNestedFunctions(1);

function argumentsLengthTryCatch() {
    try {
        throw ["foo"];
    } catch (arguments) {
        var arguments = ["foo", "bar"];
    }
    shouldBe(arguments.length, 3);
}
argumentsLengthTryCatch(1, 2, 3);

function argumentsLengthWith() {
    var object = { 'arguments': ["foo"] };

    with (object) {
        var arguments = ["foo", "bar"];
    }

    shouldBe(arguments.length, 0);
}
argumentsLengthWith();

function shouldBeAsync(expected, run, msg) {
    let actual;
    var hadError = false;
    run().then(function (value) { actual = value; },
        function (error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (hadError)
        throw actual;
    shouldBe(expected, actual, msg);
}

async function argumentsWithAsyncFunction() {
    if (arguments.length === 3 &&
        arguments.callee.name === "argumentsWithAsyncFunction")
        return 14;
}
shouldBeAsync(14, () => argumentsWithAsyncFunction(1, 2, 3));

{
    var arguments = [3, 2];
    shouldBe(arguments.length, 2);
}

function deleteArgumentsLength() {
    return (delete arguments.length);
}

shouldBe(deleteArgumentsLength(), true);
