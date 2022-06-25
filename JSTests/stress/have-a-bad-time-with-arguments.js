//@ runFTLNoCJIT

// Tests accessing an element in arguments before and after having a bad time.
// This test should not crash.

let verbose = false;

var utilities =                 'function shouldEqual(testId, actual, expected) {' + '\n' +
                                '    if (actual != expected)' + '\n' +
                                '        throw testId + ": ERROR: expect " + expected + ", actual " + actual;' + '\n' +
                                '}' + '\n';

var haveABadTime =              'Object.defineProperty(Object.prototype, 20, { get() { return 20; } });' + '\n';

var directArgumentsDecl =       '    var args = arguments;' + '\n';

var scopedArgumentsDecl =       '    var args = arguments;' + '\n' +
                                '    function closure() { return x; }' + '\n';

var clonedArgumentsDecl =       '    "use strict";' + '\n' +
                                '    var args = arguments;' + '\n';

function testFunction(argsDecl, insertElementAction, indexToReturn) {
    var script =                'function test(x) {' + '\n' +
                                     argsDecl +
                                     insertElementAction +
                                '    return args[' + indexToReturn + '];' + '\n' +
                                '}' + '\n' +
                                'noInline(test);' + '\n';
    return script;
}

function warmupFunction(tierWarmupCount, testArgs) {
    var script =                'function warmup() {' + '\n' +
                                '    for (var i = 0; i < ' + tierWarmupCount + '; i++) {' + '\n' +
                                '        test(' + testArgs + ');' + '\n' +
                                '    }' + '\n' +
                                '}' + '\n';
    return script;
}

let argumentsDecls = {
    direct: directArgumentsDecl,
    scoped: scopedArgumentsDecl,
    cloned: clonedArgumentsDecl
};

let indicesToReturn = {
    inBounds: 0,
    outOfBoundsInsertedElement: 10,
    outOfBoundsInPrototype: 20
};

let tierWarmupCounts = {
    llint: 1,
    baseline: 50,
    dfg: 1000,
    ftl: 10000
};

let testArgsList = {
    noArgs:   {
        args: '',
        result: {
            inBounds: { beforeBadTime: 'undefined', afterBadTime: 'undefined', },
            outOfBoundsInsertedElement: { beforeBadTime: '10', afterBadTime: '10', },
            outOfBoundsInPrototype: { beforeBadTime: 'undefined', afterBadTime: '20', },
        }
    },
    someArgs: {
        args: '1, 2, 3',
        result: {
            inBounds: { beforeBadTime: '1', afterBadTime: '1', },
            outOfBoundsInsertedElement: { beforeBadTime: '10', afterBadTime: '10', },
            outOfBoundsInPrototype: { beforeBadTime: 'undefined', afterBadTime: '20', },
        }
    }
};

let insertElementActions = {
    insertElement:      '    args[10] = 10;' + '\n',
    dontInsertElement:  ''
};

for (let argsDeclIndex in argumentsDecls) {
    let argsDecl = argumentsDecls[argsDeclIndex];

    for (let indexToReturnIndex in indicesToReturn) {
        let indexToReturn = indicesToReturn[indexToReturnIndex];

        for (let insertElementActionIndex in insertElementActions) {
            let insertElementAction = insertElementActions[insertElementActionIndex];

            for (let tierWarmupCountIndex in tierWarmupCounts) {
                let tierWarmupCount = tierWarmupCounts[tierWarmupCountIndex];

                for (let testArgsIndex in testArgsList) {
                    let testArgs = testArgsList[testArgsIndex].args;
                    let expectedResult = testArgsList[testArgsIndex].result[indexToReturnIndex];

                    if (indexToReturnIndex == 'outOfBoundsInsertedElement'
                        && insertElementActionIndex == 'dontInsertElement')
                        expectedResult = 'undefined';

                    let testName =
                        argsDeclIndex + '-' +
                        indexToReturnIndex + '-' +
                        insertElementActionIndex + '-' +
                        tierWarmupCountIndex + '-' +
                        testArgsIndex;


                    let script = utilities +
                                 testFunction(argsDecl, insertElementAction, indexToReturn) +
                                 warmupFunction(tierWarmupCount, testArgs) +
                                 'warmup()' + '\n' +
                                 'shouldEqual(10000, test(' + testArgs + '), ' + expectedResult['beforeBadTime'] + ');' + '\n' +
                                 haveABadTime +
                                 'shouldEqual(20000, test(' + testArgs + '), ' + expectedResult['afterBadTime'] + ');' + '\n';

                    if (verbose) {
                        print('Running test configuration: ' + testName);
                        print(
                            'Test script: ====================================================\n' +
                            script +
                            '=== END script ==================================================');
                    }

                    try {
                        runString(script);
                    } catch (e) {
                        print('FAILED test configuration: ' + testName);
                        print('FAILED test script:\n' + script);
                        throw e;
                    }
                }
            }
        }
    }
}
