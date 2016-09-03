//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Async Await tests -- verifies functionality of async/await

function echo(str) {
    WScript.Echo(str);
}

var tests = [
    {
        name: "Async keyword with a lambda expressions",
        body: function (index) {
            var x = 12;
            var y = 14;
            var lambdaParenNoArg = async() => x < y;
            var lambdaArgs = async(a, b, c) => a + b + c;

            lambdaParenNoArg().then(result => {
                echo(`Test #${index} - Success lambda expression with no argument called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with no argument called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with no argument called with err = ${err}`);
            });

            lambdaArgs(10, 20, 30).then(result => {
                echo(`Test #${index} - Success lambda expression with several arguments called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with several arguments called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with several arguments called with err = ${err}`);
            });
        }
    },
    {
        name: "Async keyword with a lambda expressions and local variable captured and shadowed",
        body: function (index) {
            var x = 12;
            var lambdaSingleArgNoParen = async x => x;
            var lambdaSingleArg = async(x) => x;

            lambdaSingleArgNoParen(x).then(result => {
                echo(`Test #${index} - Success lambda expression with single argument and no paren called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with single argument and no paren called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with single argument and no paren called with err = ${err}`);
            });

            lambdaSingleArg(x).then(result => {
                echo(`Test #${index} - Success lambda expression with a single argument a called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error lambda expression with a single argument called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch lambda expression with a single argument called with err = ${err}`);
            });
        }
    },
    {
        name: "Async function in a statement",
        body: function (index) {
            {
                var namedNonAsyncMethod = function async(x, y) { return x + y; }
                var unnamedAsyncMethod = async function (x, y) { return x + y; }
                async function async(x, y) { return x - y; }

                var result = namedNonAsyncMethod(10, 20);
                echo(`Test #${index} - Success function #1 called with result = '${result}'`);

                unnamedAsyncMethod(10, 20).then(result => {
                    echo(`Test #${index} - Success function #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #2 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #2 called with err = ${err}`);
                });

                async(10, 20).then(result => {
                    echo(`Test #${index} - Success function #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #3 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #3 called with err = ${err}`);
                });
            }
            {
                async function async() { return 12; }

                async().then(result => {
                   echo(`Test #${index} - Success function #4 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error function #4 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch function #4 called with err = ${err}`);
                });
            }
            {
                function async() { return 12; }

                var result = namedNonAsyncMethod(10, 20);
                echo(`Test #${index} - Success function #5 called with result = '${result}'`);
            }
        }
    },
    {
        name: "Async function in an object",
        body: function (index) {
            var object = {
                async async() { return 12; }
            };

            var object2 = {
                async() { return 12; }
            };

            var object3 = {
                async "a"() { return 12; },
                async 0() { return 12; },
                async 3.14() { return 12; },
                async else() { return 12; },
            };

            object.async().then(result => {
                echo(`Test #${index} - Success function in a object #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #1 called with err = ${err}`);
            });

            var result = object2.async();
            echo(`Test #${index} - Success function in a object #2 called with result = '${result}'`);

            object3.a().then(result => {
                echo(`Test #${index} - Success function in a object #3 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #3 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #3 called with err = ${err}`);
            });

            object3['0']().then(result => {
                echo(`Test #${index} - Success function in a object #4 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #4 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #4 called with err = ${err}`);
            });

            object3['3.14']().then(result => {
                echo(`Test #${index} - Success function in a object #5 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #5 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #5 called with err = ${err}`);
            });

            object3.else().then(result => {
                echo(`Test #${index} - Success function in a object #6 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error function in a object #6 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch function in a object #6 called with err = ${err}`);
            });
        }
    },
    {
        name: "Async classes",
        body: function (index) {
            class MyClass {
                async asyncMethod(a) { return a; }
                async async(a) { return a; }
                async "a"() { return 12; }
                async 0() { return 12; }
                async 3.14() { return 12; }
                async else() { return 12; }
                static async staticAsyncMethod(a) { return a; }
            }

            class MySecondClass {
                async(a) { return a; }
            }

            class MyThirdClass {
                static async(a) { return a; }
            }

            var x = "foo";
            class MyFourthClass {
                async [x](a) { return a; }
            }

            var myClassInstance = new MyClass();
            var mySecondClassInstance = new MySecondClass();
            var myFourthClassInstance = new MyFourthClass();

            myClassInstance.asyncMethod(10).then(result => {
                echo(`Test #${index} - Success async in a class #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #1 called with err = ${err}`);
            });

            myClassInstance.async(10).then(result => {
                echo(`Test #${index} - Success async in a class #2 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #2 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #2 called with err = ${err}`);
            });

            myClassInstance.a().then(result => {
                echo(`Test #${index} - Success async in a class #3 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #3 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #3 called with err = ${err}`);
            });

            myClassInstance['0']().then(result => {
                echo(`Test #${index} - Success async in a class #4 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #4 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #4 called with err = ${err}`);
            });

            myClassInstance['3.14']().then(result => {
                echo(`Test #${index} - Success async in a class #5 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #5 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #5 called with err = ${err}`);
            });

            myClassInstance.else().then(result => {
                echo(`Test #${index} - Success async in a class #6 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #6 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #6 called with err = ${err}`);
            });

            MyClass.staticAsyncMethod(10).then(result => {
                echo(`Test #${index} - Success async in a class #7 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #7 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #7 called with err = ${err}`);
            });

            var result = mySecondClassInstance.async(10);
            echo(`Test #${index} - Success async in a class #8 called with result = '${result}'`);

            var result = MyThirdClass.async(10);
            echo(`Test #${index} - Success async in a class #9 called with result = '${result}'`);

            myFourthClassInstance.foo(10).then(result => {
                echo(`Test #${index} - Success async in a class #10 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error async in a class #10 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch async in a class #10 called with err = ${err}`);
            });
        }
    },
    {
        name: "Await in an async function",
        body: function (index) {
            async function asyncMethod(val, factor) {
                val = val * factor;
                if (val > 0)
                     val = await asyncMethod(val, -1);
                return val;
            }

            function await(x) {
                return x;
            }

            async function secondAsyncMethod(x) {
                return await(x);
            }

            function rejectedPromiseMethod() {
                return new Promise(function (resolve, reject) {
                    reject(Error('My Error'));
                });
            }

            async function rejectAwaitMethod() {
                return await rejectedPromiseMethod();
            }

            async function asyncThrowingMethod() {
                throw 32;
            }

            async function throwAwaitMethod() {
                return await asyncThrowingMethod();
            }

            asyncMethod(2, 2).then(result => {
                echo(`Test #${index} - Success await in an async function #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error await in an async function #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch await in an async function #1 called with err = ${err}`);
            });

            secondAsyncMethod(2).then(result => {
                echo(`Test #${index} - Success await in an async function #2 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error await in an async function #2 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch await in an async function #2 called with err = ${err}`);
            });

            rejectAwaitMethod(2).then(result => {
                echo(`Test #${index} - Failed await in an async function doesn't catch a rejected Promise. Result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Success await in an async function catch a rejected Promise in 'err'. Error = '${err}'`);
            }).catch(err => {
                echo(`Test #${index} - Success await in an async function catch a rejected Promise in 'catch'. Error = '${err}'`);
            });

            throwAwaitMethod(2).then(result => {
                echo(`Test #${index} - Failed await in an async function doesn't catch an error. Result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Success await in an async function catch an error in 'err'. Error = '${err}'`);
            }).catch(err => {
                echo(`Test #${index} - Success await in an async function catch an error in 'catch'. Error = '${err}'`);
            });
        }
    },
    {
        name: "Await keyword with a lambda expressions",
        body: function (index) {
            {
                async function asyncMethod(x, y, z) {
                    var lambdaExp = async(a, b, c) => a * b * c; 
                    var lambdaResult = await lambdaExp(x, y, z);
                    return lambdaResult;
                }

                asyncMethod(5, 5, 5).then(result => {
                    echo(`Test #${index} - Success await keyword with a lambda expressions #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error await keyword with a lambda expressions #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch await keyword with a lambda expressions #1 called with err = ${err}`);
                });
            };
            {
                async function await(lambda, x, y, z) {
                    return await lambda(x, y, z);
                }

                await(async(a, b, c) => a + b + c, 10, 20, 30).then(result => {
                    echo(`Test #${index} - Success await keyword with a lambda expressions #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error await keyword with a lambda expressions #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch await keyword with a lambda expressions #1 called with err = ${err}`);
                });
            };
        }
    },
    {
        name: "Async function with default arguments's value",
        body: function (index) {
            {
                function thrower() {
                    throw "expected error"
                }

                async function asyncMethod(argument = thrower()) {
                    return true;
                }

                async function secondAsyncMethod(argument = false) {
                    return true;
                }

                asyncMethod(true).then(result => {
                    echo(`Test #${index} - Success async function with default arguments's value overwritten #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error async function with default arguments's value overwritten #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch async function with default arguments's value overwritten #1 called with err = ${err}`);
                });

                asyncMethod().then(result => {
                    echo(`Test #${index} - Failed async function with default arguments's value has not been rejected as expected #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Success async function with default arguments's value has been rejected as expected by 'err' #2 called with err = '${err}'`);
                }).catch(err => {
                    echo(`Test #${index} - Success async function with default arguments's value has been rejected as expected by 'catch' #2 called with err = '${err}'`);
                });

                secondAsyncMethod().then(result => {
                    echo(`Test #${index} - Success async function with default arguments's value #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error async function with default arguments's value #3 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch async function with default arguments's value #3 called with err = ${err}`);
                });
            };
        }
    },
    {
        name: "Promise in an Async function",
        body: function (index) {
            {
                async function asyncMethodResolved() {
                    let p = new Promise(function (resolve, reject) {
                        resolve("resolved");
                    });

                    return p.then(function (result) {
                        return result;
                    });
                }

                async function asyncMethodResolvedWithAwait() {
                    let p = new Promise(function (resolve, reject) {
                        resolve("resolved");
                    });

                    return await p;
                }

                async function asyncMethodRejected() {
                    let p = new Promise(function (resolve, reject) {
                        reject("rejected");
                    });

                    return p.then(function (result) {
                        return result;
                    });
                }

                asyncMethodResolved().then(result => {
                    echo(`Test #${index} - Success resolved promise in an async function #1 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error resolved promise in an async function #1 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch resolved promise in an async function #1 called with err = ${err}`);
                });

                asyncMethodResolvedWithAwait().then(result => {
                    echo(`Test #${index} - Success resolved promise in an async function #2 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Error resolved promise in an async function #2 called with err = ${err}`);
                }).catch(err => {
                    echo(`Test #${index} - Catch resolved promise in an async function #2 called with err = ${err}`);
                });

                asyncMethodRejected().then(result => {
                    echo(`Test #${index} - Failed promise in an async function has not been rejected as expected #3 called with result = '${result}'`);
                }, err => {
                    echo(`Test #${index} - Success promise in an async function has been rejected as expected by 'err' #3 called with err = '${err}'`);
                }).catch(err => {
                    echo(`Test #${index} - Success promise in an async function has been rejected as expected by 'catch' #3 called with err = '${err}'`);
                });
            };
        }
    },
    {
        name: "%AsyncFunction% constructor creates async functions analogous to Function constructor",
        body: function (index) {
            var AsyncFunction = Object.getPrototypeOf(async function () { }).constructor;

            var af = new AsyncFunction('return await Promise.resolve(0);');

            af().then(result => {
                echo(`Test #${index} - Success %AsyncFunction% created async function #1 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error %AsyncFunction% created async function #1 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch %AsyncFunction% created async function #1 called with err = ${err}`);
            });

            af = new AsyncFunction('a', 'b', 'c', 'a = await a; b = await b; c = await c; return a + b + c;');

            af(Promise.resolve(1), Promise.resolve(2), Promise.resolve(3)).then(result => {
                echo(`Test #${index} - Success %AsyncFunction% created async function #2 called with result = '${result}'`);
            }, err => {
                echo(`Test #${index} - Error %AsyncFunction% created async function #2 called with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch %AsyncFunction% created async function #2 called with err = ${err}`);
            });
        }
    },
    {
        name: "local variables with same names as formal parameters have proper redeclaration semantics (non-error cases, var and function)",
        body: function (index) {
            async function af1(x) { var y = x; var x = 'b'; return y + x; }

            af1('a').then(result => {
                if (result === 'ab') {
                    echo(`Test #${index} - Success inner var x overwrote formal parameter x only after the declaration statement`);
                } else {
                    echo(`Test #${index} - Failure x appears to have an unexpected value x = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error var redeclaration with err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch var redeclaration with err = ${err}`);
            });

            async function af2(x) { var xx = x(); function x() { return 'afx'; } return xx; }

            af2(function () { return ''; }).then(result => {
                if (result === 'afx') {
                    echo(`Test #${index} - Success inner function x() overwrote formal parameter x`);
                } else {
                    echo(`Test #${index} - Failure x appears not assigned with inner function x(), x = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch err = ${err}`);
            });
        }
    },
    {
        name: "this value in async functions behaves like it does in normal functions",
        body: function (index) {
            async function af() {
                return this;
            }

            af.call(5).then(result => {
                if (result == 5) {
                    echo(`Test #${index} - Success this value set to 5`);
                } else {
                    echo(`Test #${index} - Failure this value is not 5, this = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch err = ${err}`);
            });

            var o = {
                af: af,
                b: "abc"
            };

            o.af().then(result => {
                if (result.af === af && result.b === "abc") {
                    echo(`Test #${index} - Success this value set to { af: af, b: "abc" }`);
                } else {
                    echo(`Test #${index} - Failure this value set to something else, this = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch err = ${err}`);
            });
        }
    },
    {
        name: "arguments value in async functions behaves like it does in normal functions",
        body: function (index) {
            async function af() {
                return arguments[0] + arguments[1];
            }

            af('a', 'b').then(result => {
                if (result === 'ab') {
                    echo(`Test #${index} - Success result is 'ab' from arguments 'a' + 'b'`);
                } else {
                    echo(`Test #${index} - Failure result is not 'ab', result = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch err = ${err}`);
            });
        }
    },
    {
        name: "super value in async methods behaves like it does in normal methods",
        body: function (index) {
            class B {
                af() {
                    return "base";
                }
            }

            class C extends B {
                async af() {
                    return super.af() + " derived";
                }
            }

            var c = new C();

            c.af().then(result => {
                if (result === 'base derived') {
                    echo(`Test #${index} - Success result is 'base derived' from derived method call`);
                } else {
                    echo(`Test #${index} - Failure result is not 'base derived', result = '${result}'`);
                }
            }, err => {
                echo(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                echo(`Test #${index} - Catch err = ${err}`);
            });
        }
    },
    {
        name:"Async function with formal captured in a lambda",
        body: function (index) {
            async function af(d = 1) {
                return () => d;
            }

            af().then(result => {
                if (result() === 1) {
                    print(`Test #${index} - Success lambda returns 1 when no arguments passed`);
                } else {
                    print(`Test #${index} - Failure result is not 1, result = '${result()}'`);
                }
            }, err => {
                print(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                print(`Test #${index} - Catch err = ${err}`);
            });  
        }
    },
    {
        name:"Async function with formal captured in a nested function",
        body: function (index) {
            async function af(d = 1) {
                return function () { return d; };
            }

            af().then(result => {
                if (result() === 1) {
                    print(`Test #${index} - Success nested function returns 1 when no arguments passed`);
                } else {
                    print(`Test #${index} - Failure result is not 1, result = '${result()}'`);
                }
            }, err => {
                print(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                print(`Test #${index} - Catch err = ${err}`);
            });  
        }
    },
    {
        name:"Async function with formal captured in eval",
        body: function (index) {
            async function af(d = 1) {
                return eval("d");
            }

            af().then(result => {
                if (result === 1) {
                    print(`Test #${index} - Success eval returns 1 when no arguments passed`);
                } else {
                    print(`Test #${index} - Failure result is not 1, result = '${result}'`);
                }
            }, err => {
                print(`Test #${index} - Error err = ${err}`);
            }).catch(err => {
                print(`Test #${index} - Catch err = ${err}`);
            });  
        }
    },
];

var index = 1;

function runTest(test) {
    echo('Executing test #' + index + ' - ' + test.name);

    try {
        test.body(index);
    } catch(e) {
        echo('Caught exception: ' + e);
    }

    index++;
}

tests.forEach(runTest);

echo('\nCompletion Results:');
