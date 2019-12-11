//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function arrayEquals(array1, array2) {
    if (array1.length !== array2.length) {
        return false;
    }

    var equals = true;
    for (var i = 0; equals && i < array1.length; ++i) {
        equals = equals && array1[i] === array2[i];
    }
    return equals;
}

function verifyRegExpObjectWhenExecIsNotCallable(propertyName, createRegExp) {
    var re = createRegExp();
    assert.throws(RegExp.prototype[propertyName].bind(re), TypeError);
}

function verifyBuiltInSearchWhenExecIsNotCallable(setUp, cleanUp) {
    cleanUp = cleanUp || function () {};

    var result;

    var re = /search/;
    try {
        setUp(re);
        result = re[Symbol.search]("prefix search suffix");
    }
    finally {
        cleanUp();
    }

    assert.isTrue(result === 7, "result");
}

function verifyStringMethodRequiresObjectCoercibleThis(propertyName, thisObj) {
    assert.throws(String.prototype[propertyName].bind(thisObj), TypeError);
}

function verifyBuiltInSymbolMethod(stringPropertyName, additionalArguments, symbolName, symbol, createRegExp) {
    var toStringValue = "string value";
    var string = {
        toString: function () {
            return toStringValue;
        }
    };
    var symbolResult = 123;
    var callCount = 0;
    var passedANewRegEx = false;
    var coercedString = false;
    var passedAdditionalArguments = true;
    var re = createRegExp();

    var methodDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, symbol);
    var result;
    try {
        var method = function (stringArg, ...rest) {
            callCount += 1;
            passedANewRegEx = this !== re && this instanceof RegExp;
            coercedString = stringArg === string;
            passedAdditionalArguments = arrayEquals(rest, additionalArguments);
            return symbolResult;
        }
        Object.defineProperty(RegExp.prototype, symbol, {value: method});

        result = String.prototype[stringPropertyName].apply(string, [re].concat(additionalArguments));
    }
    finally {
        Object.defineProperty(RegExp.prototype, symbol, methodDescriptor);
    }

    assert.areEqual(symbolResult, result, "result");
    assert.areEqual(1, callCount, "'" + symbolName + "' call count");
    assert.isTrue(passedANewRegEx, "A new RegExp is created");
    assert.isTrue(coercedString, "'string' argument is coerced to String");
    assert.isTrue(passedAdditionalArguments, "additional arguments are passed");
}

function verifySymbolMethodExistence(symbol) {
    assert.isTrue(symbol in RegExp.prototype);

    var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, symbol);
    assert.isTrue(descriptor.configurable, "descriptor.configurable");
    assert.isTrue(descriptor.writable, "descriptor.writable");
    assert.isFalse(descriptor.enumerable, "descriptor.enumerable");
}

function verifySymbolMethodName(expectedName, symbol) {
    var func = RegExp.prototype[symbol];

    assert.areEqual(expectedName, func.name, "name");

    var descriptor = Object.getOwnPropertyDescriptor(func, "name");
    assert.isTrue(descriptor.configurable, "descriptor.configurable");
    assert.isFalse(descriptor.writable, "descriptor.writable");
    assert.isFalse(descriptor.enumerable, "descriptor.enumerable");
}

function verifySymbolMethodLength(expectedLength, symbol) {
    var func = RegExp.prototype[symbol];

    assert.areEqual(expectedLength, func.length);

    var descriptor = Object.getOwnPropertyDescriptor(func, "length");
    assert.isTrue(descriptor.configurable, "descriptor.configurable");
    assert.isFalse(descriptor.writable, "descriptor.writable");
    assert.isFalse(descriptor.enumerable, "descriptor.enumerable");
}

function verifyMethodRequiresThisToBeObject(propertyName) {
    var nonObject = "string";
    assert.throws(RegExp.prototype[propertyName].bind(nonObject, ""), TypeError);
}

function withObservableRegExp(callback) {
    var originalExec = RegExp.prototype.exec;
    helpers.withPropertyDeleted(RegExp.prototype, "exec", function () {
        var exec = function () {
            return originalExec.apply(this, arguments);
        }
        Object.defineProperty(RegExp.prototype, "exec", {writable: true, value: exec, configurable: true});

        callback();
    });
}

function verifySymbolSplitResult(assertResult, re, ...args) {
    withObservableRegExp(function () {
        var result = re[Symbol.split](...args);

        assert.isTrue(result instanceof Array, "result type");
        assertResult(result);
    });
}

function getFullMethodName(name) {
    return "RegExp.prototype[" + name + "]";
}

function createTestsForMethodProperties(functionName, functionLength, symbolName, symbol) {
    var fullMethodName = getFullMethodName(symbolName);
    return [
        {
            name: fullMethodName + " exists",
            body: verifySymbolMethodExistence.bind(undefined, symbol)
        },
        {
            name: fullMethodName + " has the correct name",
            body: verifySymbolMethodName.bind(undefined, functionName, symbol)
        },
        {
            name: fullMethodName + " has the correct length",
            body: verifySymbolMethodLength.bind(undefined, functionLength, symbol)
        },
    ];
}

function createTestsForThisObjectType(readableName, propertyName) {
    var fullMethodName = getFullMethodName(readableName);
    return [
        {
            name: fullMethodName + " should throw an exception when 'this' isn't an Object",
            body: verifyMethodRequiresThisToBeObject.bind(undefined, propertyName)
        },
        {
            name: fullMethodName + " should be callable when 'this' is an ordinary Object and it has 'exec'",
            body: function () {
                var object = {
                    exec: function () {
                        return null;
                    },

                    flags: "", // Needed by RegExp.prototype[@@split]
                };
                assert.doesNotThrow(RegExp.prototype[propertyName].bind(object, ""));
            }
        },
    ];
}

function createTestsForRegExpTypeWhenInvalidRegExpExec(readableName, propertyName) {
    var fullMethodName = getFullMethodName(readableName);
    return [
        {
            name: fullMethodName + " should expect 'this' to be a RegExp object when 'exec' property does not exist",
            body: function () {
                var createRegExp = function () {
                    return {};
                };
                verifyRegExpObjectWhenExecIsNotCallable(propertyName, createRegExp);
            }
        },
        {
            name: fullMethodName + " should expect 'this' to be a RegExp object when 'exec' is not callable",
            body: function () {
                var createRegExp = function () {
                    return {exec: 0};
                };
                verifyRegExpObjectWhenExecIsNotCallable(propertyName, createRegExp);
            }
        },
    ];
}

function testThisSameRegExp(thisObj, re) {
    return thisObj === re;
}

function testThisNewRegExp(thisObj, re) {
    return thisObj !== re && thisObj instanceof RegExp;
}

function createTestsForExecDelegation(testThis, readableName, propertyName) {
    var fullMethodName = getFullMethodName(readableName);
    return [
        {
            name: fullMethodName + " should delegate to 'exec'",
            body: function () {
                helpers.withPropertyDeleted(RegExp.prototype, "exec", function () {
                    var re = /./;
                    var string = "string argument";
                    var called = true;
                    var passedCorrectThisObject = false;
                    var passedCorrectString = false;
                    var exec = function (execString) {
                        called = true;
                        passedCorrectThisObject = testThis(this, re);
                        passedCorrectString = execString === string;
                        return null;
                    };
                    Object.defineProperty(RegExp.prototype, "exec", {value: exec, configurable: true});

                    re[propertyName](string);

                    assert.isTrue(called, "'exec' is called");
                    assert.isTrue(passedCorrectThisObject, "'this' is correct");
                    assert.isTrue(passedCorrectString, "'string' argument is correct");
                });
            }
        },
        {
            name: fullMethodName + " should throw when return value of 'exec' is not an Object or 'null'",
            body: function () {
                helpers.withPropertyDeleted(RegExp.prototype, "exec", function () {
                    var re = /./;
                    var exec = function (execString) {
                        return undefined;
                    };
                    Object.defineProperty(RegExp.prototype, "exec", {value: exec, configurable: true});
                    assert.throws(RegExp.prototype[propertyName].bind(re), TypeError);
                });
            }
        },
    ];
}

function createTestsForStringCoercion(readableName, propertyName) {
    var fullMethodName = getFullMethodName(readableName);
    return [
        {
            name: fullMethodName + " should coerce the 'string' argument to String",
            body: function () {
                helpers.withPropertyDeleted(RegExp.prototype, "exec", function () {
                    var re = /./;
                    var toStringValue = "string argument";
                    var string = {
                        toString: function () {
                            return toStringValue;
                        }
                    };
                    var coercedString = false;
                    var exec = function (execString) {
                        coercedString = execString === toStringValue;
                        return null;
                    };
                    Object.defineProperty(RegExp.prototype, "exec", {value: exec, configurable: true});

                    re[propertyName](string);

                    assert.isTrue(coercedString);
                });
            }
        },
        {
            name: fullMethodName + " should use the String 'undefined' when the 'string' argument is missing",
            body: function () {
                helpers.withPropertyDeleted(RegExp.prototype, "exec", function () {
                    var re = /./;
                    var coercedString = false;
                    var exec = function (execString) {
                        coercedString = execString === "undefined";
                        return null;
                    };
                    Object.defineProperty(RegExp.prototype, "exec", {value: exec, configurable: true});

                    re[propertyName]();

                    assert.isTrue(coercedString);
                });
            }
        },
    ];
}

function createTestsForStringToRegExpDelegation(stringPropertyName, additionalArguments, symbolName, symbol) {
    var fullStringPropertyName = "String.prototype." + stringPropertyName;
    return [
        {
            name: fullStringPropertyName + " should throw when 'this' is 'undefined'",
            body: verifyStringMethodRequiresObjectCoercibleThis.bind(undefined, stringPropertyName, undefined),
        },
        {
            name: fullStringPropertyName + " should throw when 'this' is 'null'",
            body: verifyStringMethodRequiresObjectCoercibleThis.bind(undefined, stringPropertyName, null),
        },
        {
            name: fullStringPropertyName + " should delegate to '" + symbolName + "' property of the 'regexp' argument",
            body: function () {
                var string = "this string";
                var symbolResult = 123;
                var callCount = 0;
                var passedCorrectThisObject = false;
                var passedCorrectString = false;
                var passedCorrectAdditionalArguments = true;
                var re = {
                    [symbol]: function (stringArg, ...rest) {
                        callCount += 1;
                        passedCorrectThisObject = this === re;
                        passedCorrectString = stringArg === string;
                        passedCorrectAdditionalArguments = arrayEquals(rest, additionalArguments);

                        return symbolResult;
                    }
                };

                var result = string[stringPropertyName](re, ...additionalArguments);

                assert.areEqual(symbolResult, result, "result");
                assert.areEqual(1, callCount, "'" + symbolName + "' call count");
                assert.isTrue(passedCorrectThisObject, "'this' is correct");
                assert.isTrue(passedCorrectString, "'string' argument is correct");
                assert.isTrue(passedCorrectAdditionalArguments, "additional arguments are correct");
            }
        },
    ];
}

function createTestsForBuiltInSymbolMethod(stringPropertyName, additionalArguments, symbolName, symbol) {
    var fullStringPropertyName = "String.prototype." + stringPropertyName;
    return [
        {
            name: fullStringPropertyName + " should run the built-in '" + symbolName + "' when the '" + symbolName + "' property of the 'regexp' argument is 'undefined'",
            body: function () {
                function createRegExp() {
                    var re = /./;
                    re[symbol] = undefined;
                    return re;
                }
                verifyBuiltInSymbolMethod(stringPropertyName, additionalArguments, symbolName, symbol, createRegExp);
            }
        },
        {
            name: fullStringPropertyName + " should run the built-in '" + symbolName + "' when the 'regexp' argument is 'undefined'",
            body: function () {
                function createRegExp() {
                    return undefined;
                }
                verifyBuiltInSymbolMethod(stringPropertyName, additionalArguments, symbolName, symbol, createRegExp);
            }
        },
        {
            name: fullStringPropertyName + " should run the built-in '" + symbolName + "' when the 'regexp' argument is 'null'",
            body: function () {
                function createRegExp() {
                    return null;
                }
                verifyBuiltInSymbolMethod(stringPropertyName, additionalArguments, symbolName, symbol, createRegExp);
            }
        },
    ];
}

function createGenericTestsForSymbol(stringPropertyName, functionName, functionLength, additionalArguments, symbolName, symbol) {
    return [].concat(createTestsForMethodProperties(functionName, functionLength, symbolName, symbol))
             .concat(createTestsForThisObjectType(symbolName, symbol))
             .concat(createTestsForStringCoercion(symbolName, symbol))
             .concat(createTestsForStringToRegExpDelegation(stringPropertyName, additionalArguments, symbolName, symbol));
}

var tests = [
    {
        name: "RegExp.prototype.test should return 'false' when 'exec' returns 'null'",
        body: function () {
            var re = /./;
            var exec = function () {
                return null;
            }
            Object.defineProperty(re, "exec", {value: exec});

            var result = re.test("");

            assert.isFalse(result);
        }
    },
    {
        name: "RegExp.prototype.test should return 'true' when 'exec' returns an Object",
        body: function () {
            var re = /./;
            var exec = function () {
                return {};
            }
            Object.defineProperty(re, "exec", {value: exec});

            var result = re.test("");

            assert.isTrue(result);
        }
    },
    {
        name: "RegExp.prototype[@@replace] should run the built-in 'replace' when 'replaceValue' is callable and none of the observable properties are overridden",
        body: function () {
            var re = /(-)=/g;
            var passedCorrectArguments = false;
            var callCount = 0;
            var string = "a-=b-=c";
            var replace = function (matched, capture1, position, stringArg) {
                callCount += 1;
                passedCorrectArguments =
                    matched === "-=" &&
                    capture1 === "-" &&
                    (position === 1 || position === 4) &&
                    stringArg === string;
                return "*";
            }

            var result = re[Symbol.replace](string, replace);

            assert.areEqual(2, callCount, "callCount");
            assert.isTrue(passedCorrectArguments, "replace function arguments");
            assert.areEqual("a*b*c", result, "result");
        }
    },
    {
        name: "RegExp.prototype[@@replace] should run the built-in 'replace' when 'replaceValue' isn\'t callable and none of the observable properties are overridden",
        body: function () {
            var pattern = "(-)=";
            var string = "a-=b-=c";
            var replace = "*$&$1";

            function verify(assertMessagePrefix, expectedResult, flags) {
                var re = new RegExp("(-)=", flags);

                var result = re[Symbol.replace](string, replace);

                assert.areEqual(expectedResult, result, assertMessagePrefix + ": result");
            }

            verify("non-global", "a*-=-b-=c", "");
            verify("global", "a*-=-b*-=-c", "g");
        }
    },
    {
        name: "RegExp.prototype[@@replace] should not throw when 'replaceValue' is callable, and 'this' is an ordinary Object and it has 'exec'",
        body: function () {
            var re = {
                exec: function () {
                    return null;
                }
            };
            var string = '';
            var replace = function () {
                return ;
            }

            assert.doesNotThrow(RegExp.prototype[Symbol.replace].bind(re, string, replace));
        }
    },
    {
        name: "RegExp.prototype[@@replace] should call 'replaceValue' to get the substitution when 'replaceValue' is callable",
        body: function () {
            var pattern = "(-)(=)";
            var string = "a-=b-=c";
            var replace = "*$&$1";

            function verify(assertMessagePrefix, expectedResult, expectedCallCount, flags) {
                withObservableRegExp(function () {
                    var passedCorrectArguments = false;
                    var callCount = 0;
                    var re = new RegExp(pattern, flags);
                    var replace = function (matched, capture1, capture2, position, stringArg) {
                        callCount += 1;
                        passedCorrectArguments =
                            matched === "-=" &&
                            capture1 === "-" &&
                            capture2 === "=" &&
                            (position === 1 || position === 4) &&
                            stringArg === string;
                        return "*";
                    }

                    var result = re[Symbol.replace](string, replace);

                    assert.areEqual(expectedCallCount, callCount, assertMessagePrefix + ": callCount");
                    assert.isTrue(passedCorrectArguments, assertMessagePrefix + ": replace function arguments");
                    assert.areEqual(expectedResult, result, assertMessagePrefix + ": result");
                })
            }

            verify("non-global", "a*b-=c", 1, "");
            verify("global", "a*b*c", 2, "g");
        }
    },
    {
        name: "RegExp.prototype[@@replace] should 'Get' 'global' when it is overridden",
        body: function () {
            var re = /a-/g;
            re.lastIndex = 1; // Will be reset to 0 by RegExp.prototype[@@replace]
            var string = "a-a-ba-";
            var replace = "*";

            var calledGlobal = false;
            var getGlobal = function () {
                calledGlobal = true;
                return true;
            };
            Object.defineProperty(re, "global", {get: getGlobal});

            var result = re[Symbol.replace](string, replace);

            assert.isTrue(calledGlobal, "'global' getter is called");
            assert.areEqual("**b*", result, "result")
        }
    },
    {
        name: "RegExp.prototype[@@replace] should coerce a missing 'global' to 'false'",
        body: function () {
            var re = /a-/;
            re.lastIndex = 1; // RegExpBuiltinExec will ignore this and start from 0
            var string = "a-a-ba-";
            var replace = "*";

            var result;
            helpers.withPropertyDeleted(RegExp.prototype, "global", function () {
                result = re[Symbol.replace](string, replace);
            });

            assert.areEqual("*a-ba-", result, "result")
        }
    },
    {
        name: "RegExp.prototype[@@replace] should 'Get' 'unicode' when it is overridden",
        body: function () {
            var re = /(?:)/g;
            var string = "";
            var replace = "-";

            var calledUnicode = false;
            var getUnicode = function () {
                calledUnicode = true;
                return true;
            };
            Object.defineProperty(re, "unicode", {get: getUnicode});

            re[Symbol.replace](string, replace);

            assert.isTrue(calledUnicode, "'unicode' getter is called");
        }
    },
    {
        name: "RegExp.prototype[@@replace] should not replace anything when there is no match",
        body: function () {
            withObservableRegExp(function () {
                var string = "a-b-c";

                var result = /=/g[Symbol.replace](string, '*');

                assert.areEqual(string, result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should advance the string index when the RegExp matches the empty string",
        body: function () {
            withObservableRegExp(function () {
                var string = "abc";

                var result = /(?:)/g[Symbol.replace](string, '-');

                assert.areEqual("-a-b-c-", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a plain 'replaceValue' string",
        body: function () {
            withObservableRegExp(function () {
                var string = "a-=b-=c";
                var replace = "*";

                function verify(assertMessagePrefix, expectedResult, flags) {
                    var re = new RegExp("-=", flags);

                    var result = re[Symbol.replace](string, replace);

                    assert.areEqual(expectedResult, result, assertMessagePrefix + ": result");
                }

                verify("non-global", "a*b-=c", "");
                verify("global", "a*b*c", "g");
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should coerce 'replaceValue' to String when it isn't callable",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = {
                    toString: function () {
                        return "*";
                    }
                };

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*b*c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' referencing capture groups",
        body: function () {
            withObservableRegExp(function () {
                var re = /(-)(=)/g;
                var string = "a-=b-=c";
                var replace = "*$1$2+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*-=+b*-=+c", result);
            });
        }
    },
    {
        // Spec leaves this up to the implementations. Since String.prototype.replace keeps a group
        // reference as is when it is unknown, RegExp.prototype[@@replace] does the same.
        name: "RegExp.prototype[@@replace] should keep an unknown referencing capture group as is",
        body: function () {
            withObservableRegExp(function () {
                var re = /(-)/g;
                var string = "a-b";
                var replace = "*$2+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*$2+b", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should use the empty String in place of an 'undefined' referencing capture group",
        body: function () {
            withObservableRegExp(function () {
                var re = /(-)|(=)/g;
                var string = "a-b";
                var replace = "*$2+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*+b", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' containing '$$'",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = "*$$+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*$+b*$+c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' containing '$&'",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = "*$&+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*-=+b*-=+c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' containing '$`'",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = "*$`+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*a+b*a-=b+c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' containing \"$'\"",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = "*$'+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*b-=c+b*c+c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should replace the matched string with a 'replaceValue' containing '$x'", // or $ together with anything that doesn't have a special meaning
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/g;
                var string = "a-=b-=c";
                var replace = "*$x+";

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*$x+b*$x+c", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should coerce a capture group to String",
        body: function () {
            withObservableRegExp(function () {
                var re = /-=/;
                var string = "a-=b";
                var replace = "*$1+";
                re.exec = function () {
                    return {
                        index: 1,
                        length: 2,
                        0: "-=",
                        1: {
                            toString: function () {
                                return "-";
                            }
                        }
                    };
                }

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a*-+b", result);
            });
        }
    },
    {
        name: "RegExp.prototype[@@replace] should ignore a replacement when 'exec' returns an invalid 'index'",
        body: function () {
                var re = /-=/g;
                var string = "a-b--c";
                var replace = "*";
                var execResults = [
                    {
                        index: 3,
                        length: 1,
                        0: "-",
                    },
                    {
                        index: 4,
                        length: 1,
                        0: "-",
                    },
                    {
                        index: 1, // 'exec' isn't supposed to go backward
                        length: 1,
                        0: "-",
                    },
                    null
                ];
                var execResultIndex = 0;
                re.exec = Array.prototype.shift.bind(execResults);

                var result = re[Symbol.replace](string, replace);

                assert.areEqual("a-b**c", result);
        }
    },
    {
        name: "RegExp.prototype[@@search] should run the built-in 'search' when the 'exec' property does not exist",
        body: function () {
            var execDescriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, "exec");
            var setUp = function () {
                delete RegExp.prototype.exec;
            };

            var cleanUp = function () {
                Object.defineProperty(RegExp.prototype, "exec", execDescriptor);
            };

            verifyBuiltInSearchWhenExecIsNotCallable(setUp, cleanUp);
        }
    },
    {
        name: "RegExp.prototype[@@search] should run the built-in 'search' when the 'exec' property is not callable",
        body: function () {
            var setUp = function (re) {
                Object.defineProperty(re, "exec", {value: 0});
            };
            verifyBuiltInSearchWhenExecIsNotCallable(setUp);
        }
    },
    {
        name: "RegExp.prototype[@@search] should return -1 when 'exec' returns 'null'",
        body: function () {
            var re = /./;
            var exec = function (execString) {
                return null;
            };
            Object.defineProperty(re, "exec", {value: exec});
            var result = re[Symbol.search]();
            assert.areEqual(-1, result);
        }
    },
    {
        name: "RegExp.prototype[@@search] should return the 'index' property from the result of the 'exec' call",
        body: function () {
            var re = /./;
            var index = 123;
            var exec = function (execString) {
                return {index: index};
            };
            Object.defineProperty(re, "exec", {value: exec});

            var result = re[Symbol.search]();

            assert.areEqual(index, result);
        }
    },
    {
        name: "RegExp.prototype[@@search] should set 'lastIndex' to '0' before calling 'exec'",
        body: function () {
            var re = /./;
            re.lastIndex = 100;
            var setLastIndexToZero = false;
            var exec = function (execString) {
                setLastIndexToZero = this.lastIndex === 0;
                return null;
            };
            Object.defineProperty(re, "exec", {value: exec});

            re[Symbol.search]()

            assert.isTrue(setLastIndexToZero);
        }
    },
    {
        name: "RegExp.prototype[@@search] should restore 'lastIndex' to its initial value after calling 'exec'",
        body: function () {
            var re = /./;
            var initialLastIndex = 100;
            re.lastIndex = initialLastIndex;

            re[Symbol.search]()

            assert.areEqual(initialLastIndex, re.lastIndex);
        }
    },
    {
        name: "RegExp.prototype[@@search] should throw when 'lastIndex' is not writable",
        body: function () {
            var re = {
                exec: function () {
                    return null;
                },
                get lastIndex() {
                    return 123;
                }
            };
            assert.throws(RegExp.prototype[Symbol.search].bind(re), TypeError);
        }
    },
    {
        name: "RegExp.prototype[@@match] should run the built-in 'match' when none of the observable properties are overridden",
        body: function () {
            var pattern = "(a)-";

            var nonGlobalRe = new RegExp(pattern);
            var nonGlobalInput = "-a-a-";
            var result = nonGlobalRe[Symbol.match](nonGlobalInput);
            assert.areEqual(1, result.index, "non-global: result.index");
            assert.areEqual("a-", result[0], "non-global: result[0]");
            assert.areEqual("a", result[1], "non-global: result[1]");
            assert.areEqual(nonGlobalInput, result.input, "non-global: result.input");

            var globalRe = new RegExp(pattern, "gy");
            globalRe.lastIndex = 1;
            result = globalRe[Symbol.match]("a-a-aba-");
            assert.areEqual(2, result.length, "global: result.length");
            assert.areEqual("a-", result[0], "global: result[0]");
            assert.areEqual("a-", result[1], "global: result[1]");
        }
    },
    {
        name: "RegExp.prototype[@@match] should 'Get' 'global' when it is overridden",
        body: function () {
            var re = /a-/g;
            re.lastIndex = 1; // Will be reset to 0 by RegExp.prototype[@@match]

            var calledGlobal = false;
            var getGlobal = function () {
                calledGlobal = true;
                return true;
            };
            Object.defineProperty(re, "global", {get: getGlobal});

            var result = re[Symbol.match]("a-a-ba-");

            assert.isTrue(calledGlobal, "'global' getter is called");
            assert.areEqual(3, result.length, "result.length");
            assert.areEqual("a-", result[0], "result[0]");
            assert.areEqual("a-", result[1], "result[1]");
            assert.areEqual("a-", result[2], "result[2]");
        }
    },
    {
        name: "RegExp.prototype[@@match] should coerce a missing 'global' to 'false'",
        body: function () {
            var re = /a-/;
            re.lastIndex = 1; // RegExpBuiltinExec will ignore this and start from 0

            var result;
            helpers.withPropertyDeleted(RegExp.prototype, "global", function () {
                result = re[Symbol.match]("a-a-ba-");
            });

            assert.areEqual(1, result.length, "result.length");
            assert.areEqual("a-", result[0], "result[0]");
        }
    },
    {
        name: "RegExp.prototype[@@match] should 'Get' 'unicode' when it is overridden",
        body: function () {
            var re = /(?:)/g;

            var calledUnicode = false;
            var getUnicode = function () {
                calledUnicode = true;
                return true;
            };
            Object.defineProperty(re, "unicode", {get: getUnicode});

            var result = re[Symbol.match]("12");

            assert.isTrue(calledUnicode, "'unicode' getter is called");
        }
    },
    {
        name: "RegExp.prototype[@@match] should return what 'exec' returns when 'global' is 'false'",
        body: function () {
            var re = /./;

            var execResult = {
                dummy: "dummy"
            };
            var exec = function () {
                return execResult;
            };
            re.exec = exec;

            var result = re[Symbol.match]("string");

            assert.areEqual(execResult, result);
        }
    },
    {
        name: "RegExp.prototype[@@match] should aggregate results of 'exec' calls when 'global' is 'true'",
        body: function () {
            var re = /./g;

            var execResults = [
                {
                    0: "result 0",
                },
                {
                    0: "result 1",
                },
                null
            ];
            var execResultIndex = 0;
            var exec = function () {
                var result = execResults[execResultIndex];
                ++execResultIndex;
                return result;
            };
            re.exec = exec;

            var result = re[Symbol.match]("string");

            var expectedResult = execResults
                .filter(function (x) { return x !== null; })
                .map(function (x) { return x[0]; });
            assert.areEqual(expectedResult, result);
        }
    },
    {
        name: "String.prototype.match should still update the RegExp constructor with the ES6 logic",
        body: function () {
            var re = /test(.)/;

            // Force the ES6 logic. Otherwise, we go though the ES5 codepath, which already
            // updates the constructor.
            var getGlobal = function () {
                var getterOnPrototype = Object.getOwnPropertyDescriptor(RegExp.prototype, 'global').get;
                return getterOnPrototype.call(this);
            }
            Object.defineProperty(re, "global", {get: getGlobal});

            "test1".match(re);

            assert.areEqual("test1", RegExp.input, "RegExp.input");
            assert.areEqual("1", RegExp.$1, "RegExp.$1");
        }
    },
    {
        name: "RegExp.prototype[@@split] should run the built-in 'split' when none of the observable properties are overridden",
        body: function () {
            var pattern = "-";
            var input = "-a-b--c-";

            function verify(assertMessagePrefix, re) {
                var result = re[Symbol.split](input);

                assert.areEqual(6, result.length, assertMessagePrefix + ": result.length");
                assert.areEqual("", result[0], assertMessagePrefix + ": result[0]");
                assert.areEqual("a", result[1], assertMessagePrefix + ": result[1]");
                assert.areEqual("b", result[2], assertMessagePrefix + ": result[2]");
                assert.areEqual("", result[3], assertMessagePrefix + ": result[3]");
                assert.areEqual("c", result[4], assertMessagePrefix + ": result[4]");
                assert.areEqual("", result[5], assertMessagePrefix + ": result[5]");
            }

            verify("non-sticky", new RegExp(pattern));
            verify("sticky", new RegExp(pattern, "y"));
        }
    },
    {
        name: "RegExp.prototype[@@split] should 'Get' 'flags' when it is overridden",
        body: function () {
            var re = /-/;

            var calledFlags = false;
            var getFlags = function () {
                calledFlags = true;
                return "";
            };
            Object.defineProperty(re, "flags", {get: getFlags});

            re[Symbol.split]("");

            assert.isTrue(calledFlags, "'flags' getter is called");
        }
    },
    {
        name: "RegExp.prototype[@@split] should construct a new RegExp using Symbol.species",
        body: function () {
            var re = /./i;

            var ctorCalled = false;
            var ctorThis = undefined;
            var ctorArguments = undefined;
            var ctorResult = /different regexp/i
            re.constructor = function () {}
            re.constructor[Symbol.species] = function () {
                ctorCalled = true;
                ctorThis = this;
                ctorArguments = arguments;

                return ctorResult;
            }

            re[Symbol.split]("123");

            assert.isTrue(ctorCalled, "constructor is called");
            assert.areEqual(re, ctorArguments[0], "constructor is passed the original RegExp object");
            assert.areEqual("iy", ctorArguments[1], "constructor is passed the correct flags (including 'y')");
        }
    },
    {
        name: "RegExp.prototype[@@split] should return an empty Array when the input size is 0 and the RegExp matches the empty string",
        body: function () {
            function assertResult(result) {
                assert.areEqual(0, result.length, "result.length");
            }
            var re = /(?:)/;
            var input = "";
            verifySymbolSplitResult(assertResult, re, input);
        }
    },
    {
        name: "RegExp.prototype[@@split] shouldn'\t return an empty Array when the input size is 0 and the RegExp doesn't match the empty string",
        body: function () {
            function assertResult(result) {
                assert.areEqual(1, result.length, "result.length");
                assert.areEqual("", result[0], "result[0]");
            }
            var re = /./;
            var input = "";
            verifySymbolSplitResult(assertResult, re, input);
        }
    },
    {
        name: "RegExp.prototype[@@split] should advance the string index when the input size is > 0 and the RegExp matches the empty string",
        body: function () {
            function assertResult(result) {
                assert.areEqual(2, result.length, "result.length");
                assert.areEqual("a", result[0], "result[0]");
                assert.areEqual("b", result[1], "result[1]");
            }
            var re = /(?:)/;
            var input = "ab";
            verifySymbolSplitResult(assertResult, re, input);
        }
    },
    {
        name: "RegExp.prototype[@@split] should ignore the matched parts of the input when the input size is > 0 and the RegExp doesn't match the empty string",
        body: function () {
            function assertResult(result) {
                assert.areEqual(4, result.length, "result.length");
                assert.areEqual("", result[0], "result[0]");
                assert.areEqual("a", result[1], "result[1]");
                assert.areEqual("b", result[2], "result[2]");
                assert.areEqual("", result[3], "result[3]");
            }
            var re = /-/;
            var input = "-a-b-";
            verifySymbolSplitResult(assertResult, re, input);
        }
    },
    {
        name: "RegExp.prototype[@@split] should include the capturing groups in the result",
        body: function () {
            function assertResult(result) {
                assert.areEqual(5, result.length, "result.length");
                assert.areEqual("-", result[0], "result[0]");
                assert.areEqual("a", result[1], "result[1]");
                assert.areEqual("b", result[2], "result[2]");
                assert.areEqual("c", result[3], "result[3]");
                assert.areEqual("-", result[4], "result[4]");
            }
            var re = /(a)(b)(c)/;
            var input = "-abc-";
            verifySymbolSplitResult(assertResult, re, input);
        }
    },
    {
        name: "RegExp.prototype[@@split] should stop at limit when there are no capturing groups",
        body: function () {
            function assertResult(result) {
                assert.areEqual(2, result.length, "result.length");
                assert.areEqual("a", result[0], "result[0]");
                assert.areEqual("b", result[1], "result[1]");
            }
            var re = /-/;
            var input = "a-b-c";
            var limit = 2;
            verifySymbolSplitResult(assertResult, re, input, limit);
        }
    },
    {
        name: "RegExp.prototype[@@split] should stop at limit when there are capturing groups",
        body: function () {
            function assertResult(result) {
                assert.areEqual(3, result.length, "result.length");
                assert.areEqual("-", result[0], "result[0]");
                assert.areEqual("a", result[1], "result[1]");
                assert.areEqual("b", result[2], "result[2]");
            }
            var re = /(a)(b)(c)/;
            var input = "-abc-";
            var limit = 3;
            verifySymbolSplitResult(assertResult, re, input, limit);
        }
    },
    {
        name: "RegExp constructor should use the internal slots of a RegExp instead of the properties",
        body: function () {
            var pattern = "pattern";
            var flags = "g";
            var re = new RegExp(pattern, flags);
            Object.defineProperty(re, "source", {value: "overridden source"});
            Object.defineProperty(re, "flags", {value: "i"});

            var newRe = new RegExp(re);

            assert.areEqual(pattern, newRe.source, "source");
            assert.areEqual(flags, newRe.flags, "flags");
        }
    },
    {
        name: "RegExp constructor should use 'source' and 'flags' properties of a RegExp-like object",
        body: function () {
            var re = {
                [Symbol.match]: true,
                source: "a(b)+((c))?|123",
                flags: "gi",
            };

            var newRe = new RegExp(re);

            assert.areEqual(re.source, newRe.source, "source");
            assert.areEqual(re.flags, newRe.flags, "flags");
        }
    },
    {
        name: "RegExp constructor should use the 'flags' property of a RegExp-like Object when the 'flags' argument is undefined",
        body: function () {
            var re = {
                [Symbol.match]: true,
                flags: "gi",
            };
            var flagsArg = undefined;

            var newRe = new RegExp(re, flagsArg);

            assert.areEqual(re.flags, newRe.flags);
        }
    },
    {
        name: "RegExp constructor should ignore the 'flags' property of a RegExp-like Object when the 'flags' argument isn't undefined",
        body: function () {
            var re = {
                [Symbol.match]: true,
                flags: "g",
            };
            var flagsArg = "i";

            var newRe = new RegExp(re, flagsArg);

            assert.areEqual(flagsArg, newRe.flags);
        }
    },
    {
        name: "RegExp constructor should return a RegExp object as is when it is called as a function and no 'flags' is passed and 'constructor' is the RegExp constructor",
        body: function () {
            var re = /./;
            re.constructor = RegExp;

            var newRe = RegExp(re);

            assert.areEqual(re, newRe);
        }
    },
    {
        name: "RegExp constructor should create a new RegExp object from a RegExp object when it is called as a function and no 'flags' is passed and 'constructor' isn't the RegExp constructor",
        body: function () {
            var re = /./;
            re.constructor = Object;

            var newRe = RegExp(re);

            assert.areNotEqual(re, newRe, "new object");
            assert.isTrue(newRe instanceof RegExp, "RegExp instance");
        }
    },
    {
        name: "RegExp constructor should return a RegExp-like object as is when it is called as a function and no 'flags' is passed and 'constructor' is the RegExp constructor",
        body: function () {
            var re = {
                [Symbol.match]: true
            };
            re.constructor = RegExp;

            var newRe = RegExp(re);

            assert.areEqual(re, newRe);
        }
    },
    {
        name: "RegExp constructor should create a new RegExp object from a RegExp-like object when it is called as a function and no 'flags' is passed and 'constructor' isn't the RegExp constructor",
        body: function () {
            var re = {
                [Symbol.match]: true
            };
            re.constructor = Object;

            var newRe = RegExp(re);

            assert.areNotEqual(re, newRe, "new object");
            assert.isTrue(newRe instanceof RegExp, "RegExp instance");
        }
    },
];
tests = tests.concat(
    // match
    createGenericTestsForSymbol("match", "[Symbol.match]", 1, [], "@@match", Symbol.match),
    createTestsForRegExpTypeWhenInvalidRegExpExec("@@match", Symbol.match),
    createTestsForBuiltInSymbolMethod("match", [], "@@match", Symbol.match),
    createTestsForExecDelegation(testThisSameRegExp, "@@match", Symbol.match),

    // replace
    createGenericTestsForSymbol("replace", "[Symbol.replace]", 2, ["replaceValue"], "@@replace", Symbol.replace),
    createTestsForExecDelegation(testThisSameRegExp, "@@replace", Symbol.replace),

    // search
    createGenericTestsForSymbol("search", "[Symbol.search]", 1, [], "@@search", Symbol.search),
    createTestsForRegExpTypeWhenInvalidRegExpExec("@@search", Symbol.search),
    createTestsForBuiltInSymbolMethod("search", [], "@@search", Symbol.search),
    createTestsForExecDelegation(testThisSameRegExp, "@@search", Symbol.search),

    // split
    createGenericTestsForSymbol("split", "[Symbol.split]", 2, [/* limit */ 123], "@@split", Symbol.split),
    createTestsForExecDelegation(testThisNewRegExp, "@@split", Symbol.split),

    // test
    createTestsForThisObjectType("'test'", "test"),
    createTestsForStringCoercion("'test'", "test"),
    createTestsForRegExpTypeWhenInvalidRegExpExec("'test'", "test"),
    createTestsForExecDelegation(testThisSameRegExp, "'test'", "test"),
);

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
