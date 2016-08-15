let typeCases = [
    "1",
    "Math.PI",
    "NaN",
    "undefined",
    "null",
    "true",
    "false",
];

let operators = ["<", "<=", ">", ">=", "==", "!=", "===", "!=="];

function opaqueSideEffect()
{
}
noInline(opaqueSideEffect);

let testCaseIndex = 0;
for (let operator of operators) {
    eval(`
        function testPolymorphic(a, b) {
            if (a ${operator} b) {
                opaqueSideEffect()
                return true;
            }
            return false;
        }
        noInline(testPolymorphic)`);

    for (let left of typeCases) {
        for (let right of typeCases) {
            let llintResult = eval(left + operator + right);
            eval(`
            function testMonomorphic${testCaseIndex}(a, b) {
                if (a ${operator} b) {
                    opaqueSideEffect()
                    return true;
                }
                return false;
            }
            noInline(testMonomorphic${testCaseIndex});

            function testMonomorphicLeftConstant${testCaseIndex}(b) {
                if (${left} ${operator} b) {
                    opaqueSideEffect()
                    return true;
                }
                return false;
            }
            noInline(testMonomorphicLeftConstant${testCaseIndex});

            function testMonomorphicRightConstant${testCaseIndex}(a) {
                if (a ${operator} ${right}) {
                    opaqueSideEffect()
                    return true;
                }
                return false;
            }
            noInline(testMonomorphicRightConstant${testCaseIndex});

            for (let i = 0; i < 500; ++i) {
                if (testMonomorphic${testCaseIndex}(${left}, ${right}) != ${llintResult})
                    throw "Failed testMonomorphic${testCaseIndex}(${left}, ${right})";
                if (testMonomorphicLeftConstant${testCaseIndex}(${right}) != ${llintResult})
                    throw "Failed testMonomorphicLeftConstant${testCaseIndex}(${right})";
                if (testMonomorphicRightConstant${testCaseIndex}(${left}) != ${llintResult})
                    throw "Failed testMonomorphicLeftConstant${testCaseIndex}(${left})";
                if (testPolymorphic(${left}, ${right}) !== ${llintResult})
                    throw "Failed polymorphicVersion(${left})";
            }
            `);
            ++testCaseIndex;
        }
    }
}