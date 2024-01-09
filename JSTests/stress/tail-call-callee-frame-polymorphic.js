'use strict';

// Original: resources/stubs.js
function nop() {
  return false;
}

try {
  gc;
} catch (e) {
  this.gc = function () {
    for (let i = 0; i < 10000; i++) {
      let s = new String("AAAA" + Math.random());
    }
  };
}

try {
  uneval;
} catch (e) {
  this.uneval = this.nop;
}

try {
  WScript;
} catch (e) {
  this.WScript = new Proxy({}, {
    get(target, name) {
      switch (name) {
        case 'Echo':
          return print;

        default:
          return {};
      }
    }

  });
}

try {
  this.alert = console.log;
} catch (e) {}

try {
  this.print = console.log;
} catch (e) {}

// Original: v8/test/mjsunit/mjsunit.js
function MjsUnitAssertionError(message) {
  this.message = message;
  let prevPrepareStackTrace = Error.prepareStackTrace;

  try {
    Error.prepareStackTrace = MjsUnitAssertionError.prepareStackTrace;
    this.stack = new Error("MjsUnitA****tionError").stack;
  } finally {
    Error.prepareStackTrace = prevPrepareStackTrace;
  }
}

MjsUnitAssertionError.prototype.toString = function () {
  return this.message + "\n\nStack: " + this.stack;
};

var assertSame;
var assertNotSame;
var assertEquals;
var deepEquals;
var assertNotEquals;
var assertEqualsDelta;
var assertArrayEquals;
var assertPropertiesEqual;
var assertToStringEquals;
var assertTrue;
var assertFalse;
var assertNull;
var assertNotNull;
var assertThrows;
var assertThrowsEquals;
var assertThrowsAsync;
var assertDoesNotThrow;
var assertInstanceof;
var assertUnreachable;
var assertOptimized;
var assertUnoptimized;
var assertContains;
var assertMatches;
var assertPromiseResult;
var promiseTestChain;
var promiseTestCount = 0;
var V8OptimizationStatus = {
  kIsFunction: 1 << 0,
  kNeverOptimize: 1 << 1,
  kAlwaysOptimize: 1 << 2,
  kMaybeDeopted: 1 << 3,
  kOptimized: 1 << 4,
  kTurboFanned: 1 << 5,
  kInterpreted: 1 << 6,
  kMarkedForOptimization: 1 << 7,
  kMarkedForConcurrentOptimization: 1 << 8,
  kOptimizingConcurrently: 1 << 9,
  kIsExecuting: 1 << 10,
  kTopmostFrameIsTurboFanned: 1 << 11,
  kLiteMode: 1 << 12,
  kMarkedForDeoptimization: 1 << 13,
  kBaseline: 1 << 14,
  kTopmostFrameIsInterpreted: 1 << 15,
  kTopmostFrameIsBaseline: 1 << 16
};
var isNeverOptimizeLiteMode;
var isNeverOptimize;
var isAlwaysOptimize;
var isInterpreted;
var isBaseline;
var isUnoptimized;
var isOptimized;
var isTurboFanned;
var failWithMessage;
var formatFailureText;
var prettyPrinted;

(function () {
  var ObjectPrototypeToString = Object.prototype.toString;
  var NumberPrototypeValueOf = Number.prototype.valueOf;
  var BooleanPrototypeValueOf = Boolean.prototype.valueOf;
  var StringPrototypeValueOf = String.prototype.valueOf;
  var DatePrototypeValueOf = Date.prototype.valueOf;
  var RegExpPrototypeToString = RegExp.prototype.toString;
  var ArrayPrototypeForEach = Array.prototype.forEach;
  var ArrayPrototypeJoin = Array.prototype.join;
  var ArrayPrototypeMap = Array.prototype.map;
  var ArrayPrototypePush = Array.prototype.push;
  var JSONStringify = JSON.stringify;
  var BigIntPrototypeValueOf;

  try {
    BigIntPrototypeValueOf = BigInt.prototype.valueOf;
  } catch (e) {}

  function classOf(object) {
    var string = ObjectPrototypeToString.call(object);
    return string.substring(8, string.length - 1);
  }

  function ValueOf(value) {
    switch (classOf(value)) {
      case "Number":
        return NumberPrototypeValueOf.call(value);

      case "BigInt":
        return BigIntPrototypeValueOf.call(value);

      case "String":
        return StringPrototypeValueOf.call(value);

      case "Boolean":
        return BooleanPrototypeValueOf.call(value);

      case "Date":
        return DatePrototypeValueOf.call(value);

      default:
        return value;
    }
  }

  prettyPrinted = function prettyPrinted(value) {
    switch (typeof value) {
      case "string":
        return JSONStringify(value);

      case "bigint":
        return String(value) + "n";

      case "number":
        if (value === 0 && 1 / value < 0) return "-0";

      case "boolean":
      case "undefined":
      case "function":
      case "symbol":
        return String(value);

      case "object":
        if (value === null) return "null";
        var objectClass = classOf(value);

        switch (objectClass) {
          case "Number":
          case "BigInt":
          case "String":
          case "Boolean":
          case "Date":
            return objectClass + "(" + prettyPrinted(ValueOf(value)) + ")";

          case "RegExp":
            return RegExpPrototypeToString.call(value);

          case "Array":
            var mapped = ArrayPrototypeMap.call(value, prettyPrintedArrayElement);
            var joined = ArrayPrototypeJoin.call(mapped, ",");
            return "[" + joined + "]";

          case "Uint8Array":
          case "Int8Array":
          case "Int16Array":
          case "Uint16Array":
          case "Uint32Array":
          case "Int32Array":
          case "Float32Array":
          case "Float64Array":
            var joined = ArrayPrototypeJoin.call(value, ",");
            return objectClass + "([" + joined + "])";

          case "Object":
            break;

          default:
            return objectClass + "(" + String(value) + ")";
        }

        var name = value.constructor.name;
        if (name) return name + "()";
        return "Object()";

      default:
        return "-- unknown value --";
    }
  };

  function prettyPrintedArrayElement(value, index, array) {
    if (value === undefined && !(index in array)) return "";
    return prettyPrinted(value);
  }

  failWithMessage = function failWithMessage(message) {
    throw new MjsUnitAssertionError(message);
  };

  formatFailureText = function (expectedText, found, name_opt) {
    var message = "Fail" + "ure";

    if (name_opt) {
      message += " (" + name_opt + ")";
    }

    var foundText = prettyPrinted(found);

    if (expectedText.length <= 40 && foundText.length <= 40) {
      message += ": expected <" + expectedText + "> found <" + foundText + ">";
    } else {
      message += ":\nexpected:\n" + expectedText + "\nfound:\n" + foundText;
    }

    return message;
  };

  function fail(expectedText, found, name_opt) {
    return failWithMessage(formatFailureText(expectedText, found, name_opt));
  }

  function deepObjectEquals(a, b) {
    var aProps = Object.keys(a);
    aProps.sort();
    var bProps = Object.keys(b);
    bProps.sort();

    if (!deepEquals(aProps, bProps)) {
      return false;
    }

    for (var i = 0; i < aProps.length; i++) {
      if (!deepEquals(a[aProps[i]], b[aProps[i]])) {
        return false;
      }
    }

    return true;
  }

  deepEquals = function deepEquals(a, b) {
    if (a === b) {
      if (a === 0) return 1 / a === 1 / b;
      return true;
    }

    if (typeof a !== typeof b) return false;
    if (typeof a === "number") return isNaN(a) && isNaN(b);
    if (typeof a !== "object" && typeof a !== "function") return false;
    var objectClass = classOf(a);
    if (objectClass !== classOf(b)) return false;

    if (objectClass === "RegExp") {
      return RegExpPrototypeToString.call(a) === RegExpPrototypeToString.call(b);
    }

    if (objectClass === "Function") return false;

    if (objectClass === "Array") {
      var elementCount = 0;

      if (a.length !== b.length) {
        return false;
      }

      for (var i = 0; i < a.length; i++) {
        if (!deepEquals(a[i], b[i])) return false;
      }

      return true;
    }

    if (objectClass === "String" || objectClass === "Number" || objectClass === "BigInt" || objectClass === "Boolean" || objectClass === "Date") {
      if (ValueOf(a) !== ValueOf(b)) return false;
    }

    return deepObjectEquals(a, b);
  };

  assertSame = function assertSame(expected, found, name_opt) {
    if (Object.is(expected, found)) return;
    fail(prettyPrinted(expected), found, name_opt);
  };

  assertNotSame = function assertNotSame(expected, found, name_opt) {
    if (!Object.is(expected, found)) return;
    fail("not same as " + prettyPrinted(expected), found, name_opt);
  };

  assertEquals = function assertEquals(expected, found, name_opt) {
    if (!deepEquals(found, expected)) {
      fail(prettyPrinted(expected), found, name_opt);
    }
  };

  assertNotEquals = function assertNotEquals(expected, found, name_opt) {
    if (deepEquals(found, expected)) {
      fail("not equals to " + prettyPrinted(expected), found, name_opt);
    }
  };

  assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) {
    if (Math.abs(expected - found) > delta) {
      fail(prettyPrinted(expected) + " +- " + prettyPrinted(delta), found, name_opt);
    }
  };

  assertArrayEquals = function assertArrayEquals(expected, found, name_opt) {
    var start = "";

    if (name_opt) {
      start = name_opt + " - ";
    }

    assertEquals(expected.length, found.length, start + "array length");

    if (expected.length === found.length) {
      for (var i = 0; i < expected.length; ++i) {
        assertEquals(expected[i], found[i], start + "array element at index " + i);
      }
    }
  };

  assertPropertiesEqual = function assertPropertiesEqual(expected, found, name_opt) {
    if (!deepObjectEquals(expected, found)) {
      fail(expected, found, name_opt);
    }
  };

  assertToStringEquals = function assertToStringEquals(expected, found, name_opt) {
    if (expected !== String(found)) {
      fail(expected, found, name_opt);
    }
  };

  assertTrue = function assertTrue(value, name_opt) {
    assertEquals(true, value, name_opt);
  };

  assertFalse = function assertFalse(value, name_opt) {
    assertEquals(false, value, name_opt);
  };

  assertNull = function assertNull(value, name_opt) {
    if (value !== null) {
      fail("null", value, name_opt);
    }
  };

  assertNotNull = function assertNotNull(value, name_opt) {
    if (value === null) {
      fail("not null", value, name_opt);
    }
  };

  function executeCode(code) {
    if (typeof code === 'function') return code();
    if (typeof code === 'string') return eval(code);
    failWithMessage('Given code is neither function nor string, but ' + typeof code + ': <' + prettyPrinted(code) + '>');
  }

  function checkException(e, type_opt, cause_opt) {
    if (type_opt !== undefined) {
      assertEquals('function', typeof type_opt);
      assertInstanceof(e, type_opt);
    }

    if (RegExp !== undefined && cause_opt instanceof RegExp) {
      assertMatches(cause_opt, e.message, 'Error message');
    } else if (cause_opt !== undefined) {
      assertEquals(cause_opt, e.message, 'Error message');
    }
  }

  assertThrows = function assertThrows(code, type_opt, cause_opt) {
    if (arguments.length > 1 && type_opt === undefined) {
      failWithMessage('invalid use of assertThrows, unknown type_opt given');
    }

    if (type_opt !== undefined && typeof type_opt !== 'function') {
      failWithMessage('invalid use of assertThrows, maybe you want assertThrowsEquals');
    }

    try {
      executeCode(code);
    } catch (e) {
      checkException(e, type_opt, cause_opt);
      return;
    }

    let msg = 'Did not throw exception';
    if (type_opt !== undefined && type_opt.name !== undefined) msg += ', expected ' + type_opt.name;
    failWithMessage(msg);
  };

  assertThrowsEquals = function assertThrowsEquals(fun, val) {
    try {
      fun();
    } catch (e) {
      assertSame(val, e);
      return;
    }

    failWithMessage('Did not throw exception, expected ' + prettyPrinted(val));
  };

  assertThrowsAsync = function assertThrowsAsync(promise, type_opt, cause_opt) {
    if (arguments.length > 1 && type_opt === undefined) {
      failWithMessage('invalid use of assertThrows, unknown type_opt given');
    }

    if (type_opt !== undefined && typeof type_opt !== 'function') {
      failWithMessage('invalid use of assertThrows, maybe you want assertThrowsEquals');
    }

    let msg = 'Promise did not throw exception';
    if (type_opt !== undefined && type_opt.name !== undefined) msg += ', expected ' + type_opt.name;
    return assertPromiseResult(promise, res => setTimeout(_ => fail('<throw>', res, msg), 0), e => checkException(e, type_opt, cause_opt));
  };

  assertInstanceof = function assertInstanceof(obj, type) {
    if (!(obj instanceof type)) {
      var actualTypeName = null;
      var actualConstructor = obj && Object.getPrototypeOf(obj).constructor;

      if (typeof actualConstructor === 'function') {
        actualTypeName = actualConstructor.name || String(actualConstructor);
      }

      failWithMessage('Object <' + prettyPrinted(obj) + '> is not an instance of <' + (type.name || type) + '>' + (actualTypeName ? ' but of <' + actualTypeName + '>' : ''));
    }
  };

  assertDoesNotThrow = function assertDoesNotThrow(code, name_opt) {
    try {
      executeCode(code);
    } catch (e) {
      if (e instanceof MjsUnitAssertionError) throw e;
      failWithMessage("threw an exception: " + (e.message || e));
    }
  };

  assertUnreachable = function assertUnreachable(name_opt) {
    var message = "Fail" + "ure: unreachable";

    if (name_opt) {
      message += " - " + name_opt;
    }

    failWithMessage(message);
  };

  assertContains = function (sub, value, name_opt) {
    if (value == null ? sub != null : value.indexOf(sub) == -1) {
      fail("contains '" + String(sub) + "'", value, name_opt);
    }
  };

  assertMatches = function (regexp, str, name_opt) {
    if (!(regexp instanceof RegExp)) {
      regexp = new RegExp(regexp);
    }

    if (!str.match(regexp)) {
      fail("should match '" + regexp + "'", str, name_opt);
    }
  };

  function concatenateErrors(stack, exception) {
    if (!exception.stack) exception = new Error(exception);

    if (typeof exception.stack !== 'string') {
      return exception;
    }

    exception.stack = stack + '\n\n' + exception.stack;
    return exception;
  }

  assertPromiseResult = function (promise, success, fail) {
    if (success !== undefined) assertEquals('function', typeof success);
    if (fail !== undefined) assertEquals('function', typeof fail);
    assertInstanceof(promise, Promise);
    const stack = new Error().stack;
    var test_promise = promise.then(result => {
      try {
        if (--promiseTestCount == 0) testRunner.notifyDone();
        if (success !== undefined) success(result);
      } catch (e) {
        setTimeout(_ => {
          throw concatenateErrors(stack, e);
        }, 0);
      }
    }, result => {
      try {
        if (--promiseTestCount == 0) testRunner.notifyDone();
        if (fail === undefined) throw result;
        fail(result);
      } catch (e) {
        setTimeout(_ => {
          throw concatenateErrors(stack, e);
        }, 0);
      }
    });
    if (!promiseTestChain) promiseTestChain = Promise.resolve();
    testRunner.waitUntilDone();
    ++promiseTestCount;
    return promiseTestChain.then(test_promise);
  };

  var OptimizationStatusImpl = undefined;

  var OptimizationStatus = function (fun) {
    if (OptimizationStatusImpl === undefined) {
      try {
        OptimizationStatusImpl = new Function("fun", "return %GetOptimizationStatus(fun);");
      } catch (e) {
        throw new Error("natives syntax not allowed");
      }
    }

    return OptimizationStatusImpl(fun);
  };

  assertUnoptimized = function assertUnoptimized(fun, name_opt, skip_if_maybe_deopted = true) {
    var opt_status = OptimizationStatus(fun);
    assertFalse((opt_status & V8OptimizationStatus.kAlwaysOptimize) !== 0, "test does not make sense with --always-opt");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, name_opt);

    if (skip_if_maybe_deopted && (opt_status & V8OptimizationStatus.kMaybeDeopted) !== 0) {
      return;
    }

    var is_optimized = (opt_status & V8OptimizationStatus.kOptimized) !== 0;
    assertFalse(is_optimized, name_opt);
  };

  assertOptimized = function assertOptimized(fun, name_opt, skip_if_maybe_deopted = true) {
    var opt_status = OptimizationStatus(fun);

    if (opt_status & V8OptimizationStatus.kLiteMode) {
      print("Warning: Test uses assertOptimized in Lite mode, skipping test.");
      testRunner.quit(0);
    }

    assertFalse((opt_status & V8OptimizationStatus.kNeverOptimize) !== 0, "test does not make sense with --no-opt");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, 'should be a function: ' + name_opt);

    if (skip_if_maybe_deopted && (opt_status & V8OptimizationStatus.kMaybeDeopted) !== 0) {
      return;
    }

    assertTrue((opt_status & V8OptimizationStatus.kOptimized) !== 0, 'should be optimized: ' + name_opt);
  };

  isNeverOptimizeLiteMode = function isNeverOptimizeLiteMode() {
    var opt_status = OptimizationStatus(undefined, "");
    return (opt_status & V8OptimizationStatus.kLiteMode) !== 0;
  };

  isNeverOptimize = function isNeverOptimize() {
    var opt_status = OptimizationStatus(undefined, "");
    return (opt_status & V8OptimizationStatus.kNeverOptimize) !== 0;
  };

  isAlwaysOptimize = function isAlwaysOptimize() {
    var opt_status = OptimizationStatus(undefined, "");
    return (opt_status & V8OptimizationStatus.kAlwaysOptimize) !== 0;
  };

  isInterpreted = function isInterpreted(fun) {
    var opt_status = OptimizationStatus(fun, "");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, "not a function");
    return (opt_status & V8OptimizationStatus.kOptimized) === 0 && (opt_status & V8OptimizationStatus.kInterpreted) !== 0;
  };

  isBaseline = function isBaseline(fun) {
    var opt_status = OptimizationStatus(fun, "");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, "not a function");
    return (opt_status & V8OptimizationStatus.kOptimized) === 0 && (opt_status & V8OptimizationStatus.kBaseline) !== 0;
  };

  isUnoptimized = function isUnoptimized(fun) {
    return isInterpreted(fun) || isBaseline(fun);
  };

  isOptimized = function isOptimized(fun) {
    var opt_status = OptimizationStatus(fun, "");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, "not a function");
    return (opt_status & V8OptimizationStatus.kOptimized) !== 0;
  };

  isTurboFanned = function isTurboFanned(fun) {
    var opt_status = OptimizationStatus(fun, "");
    assertTrue((opt_status & V8OptimizationStatus.kIsFunction) !== 0, "not a function");
    return (opt_status & V8OptimizationStatus.kOptimized) !== 0 && (opt_status & V8OptimizationStatus.kTurboFanned) !== 0;
  };

  MjsUnitAssertionError.prepareStackTrace = function (error, stack) {
    try {
      let filteredStack = [];
      let inMjsunit = true;

      for (let i = 0; i < stack.length; i++) {
        let frame = stack[i];

        if (inMjsunit) {
          let file = frame.getFileName();

          if (!file || !file.endsWith("mjsunit.js")) {
            inMjsunit = false;
            if (i > 0) ArrayPrototypePush.call(filteredStack, stack[i - 1]);
            ArrayPrototypePush.call(filteredStack, stack[i]);
          }

          continue;
        }

        ArrayPrototypePush.call(filteredStack, frame);
      }

      stack = filteredStack;
      let max_name_length = 0;
      ArrayPrototypeForEach.call(stack, each => {
        let name = each.getFunctionName();
        if (name == null) name = "";

        if (each.isEval()) {
          name = name;
        } else if (each.isConstructor()) {
          name = "new " + name;
        } else if (each.isNative()) {
          name = "native " + name;
        } else if (!each.isToplevel()) {
          name = each.getTypeName() + "." + name;
        }

        each.name = name;
        max_name_length = Math.max(name.length, max_name_length);
      });
      stack = ArrayPrototypeMap.call(stack, each => {
        let frame = "    at " + each.name.padEnd(max_name_length);
        let fileName = each.getFileName();
        if (each.isEval()) return frame + " " + each.getEvalOrigin();
        frame += " " + (fileName ? fileName : "");
        let line = each.getLineNumber();
        frame += " " + (line ? line : "");
        let column = each.getColumnNumber();
        frame += column ? ":" + column : "";
        return frame;
      });
      return "" + error.message + "\n" + ArrayPrototypeJoin.call(stack, "\n");
    } catch (e) {}

    ;
    return error.stack;
  };
})();

// Original: resources/jstest_stubs.js
function description(msg) {}

function debug(msg) {}

function shouldBe(_a) {
  print(typeof _a == "function" ? _a() : eval(_a));
}

function shouldBeTrue(_a) {
  shouldBe(_a);
}

function shouldBeFalse(_a) {
  shouldBe(_a);
}

function shouldBeNaN(_a) {
  shouldBe(_a);
}

function shouldBeNull(_a) {
  shouldBe(_a);
}

function shouldNotThrow(_a) {
  shouldBe(_a);
}

function shouldThrow(_a) {
  shouldBe(_a);
}

function noInline() {}

function finishJSTest() {}

try {
  $vm;
} catch (e) {
  const handler = {
    get: function (x, prop) {
      if (prop == Symbol.toPrimitive) {
        return function () {
          return undefined;
        };
      }

      return dummy;
    }
  };
  const dummy = new Proxy(function () {
    return dummy;
  }, handler);
  this.$vm = dummy;
}

function ensureArrayStorage() {}

function transferArrayBuffer() {}

// Original: resources/fuzz_library.js
function __isPropertyOfType(obj, name, type) {
  let desc;

  try {
    desc = Object.getOwnPropertyDescriptor(obj, name);
  } catch (e) {
    return false;
  }

  if (!desc) return false;
  return typeof type === 'undefined' || typeof desc.value === type;
}

function __getProperties(obj, type) {
  if (typeof obj === "undefined" || obj === null) return [];
  let properties = [];

  for (let name of Object.getOwnPropertyNames(obj)) {
    if (__isPropertyOfType(obj, name, type)) properties.push(name);
  }

  let proto = Object.getPrototypeOf(obj);

  while (proto && proto != Object.prototype) {
    Object.getOwnPropertyNames(proto).forEach(name => {
      if (name !== 'constructor') {
        if (__isPropertyOfType(proto, name, type)) properties.push(name);
      }
    });
    proto = Object.getPrototypeOf(proto);
  }

  return properties;
}

function* __getObjects(root = this, level = 0) {
  if (level > 4) return;

  let obj_names = __getProperties(root, 'object');

  for (let obj_name of obj_names) {
    let obj = root[obj_name];
    if (obj === root) continue;
    yield obj;
    yield* __getObjects(obj, level + 1);
  }
}

function __getRandomObject(seed) {
  let objects = [];

  for (let obj of __getObjects()) {
    objects.push(obj);
  }

  return objects[seed % objects.length];
}

function __getRandomProperty(obj, seed) {
  let properties = __getProperties(obj);

  if (!properties.length) return undefined;
  return properties[seed % properties.length];
}

function __callRandomFunction(obj, seed, ...args) {
  let functions = __getProperties(obj, 'function');

  if (!functions.length) return;
  let random_function = functions[seed % functions.length];

  try {
    obj[random_function](...args);
  } catch (e) {}
}

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }

  ;

  try {
    return t();
  } catch (e) {}
}

let __callGC;

(function () {
  let countGC = 0;

  __callGC = function () {
    if (countGC++ < 50) {
      gc();
    }
  };
})();

try {
  this.failWithMessage = nop;
} catch (e) {}

try {
  this.triggerAssertFalse = nop;
} catch (e) {}

try {
  this.quit = nop;
} catch (e) {}

// Original: v8/test/mjsunit/es6/array-concat-spreadable-arraylike-proxy.js
try {
  var __v_0 = [];
} catch (e) {}

try {
  var __v_1 = {};
} catch (e) {}

try {
  var __v_2 = new Proxy({}, __v_1);
} catch (e) {}

try {
  __v_1.get = function (__v_6, __v_7, __v_8) {
    return function (...__v_9) {
      try {
        __v_0.push([__v_7, ...__v_9]);
      } catch (e) {}

      return Reflect[__v_7](...__v_9);
    };
  };
} catch (e) {}

try {
  var __v_3 = ["a", "b"];
} catch (e) {}

try {
  /* VariableMutator: Replaced __v_3 with __v_2 */
  __v_2[Symbol.isConcatSpreadable] = undefined;
} catch (e) {}

try {
  var __v_4 = new Proxy(__v_3, __v_2);
} catch (e) {}

try {
  __v_0.length = 0;
} catch (e) {}

try {
  assertEquals(["a", "b"], [].concat(__v_4));
} catch (e) {}

try {
  assertEquals(6, __v_0.length);
} catch (e) {}

/* CrossOverMutator: Crossover from v8/test/mjsunit/es6/destructuring-assignment.js */
try {
  __v_3.push("compute name: " + __v_4);
} catch (e) {}

try {
  for (var __v_5 in __v_0) {
    /* ExpressionMutator: Repeated */
    try {
      assertSame(__v_3, __v_0[__v_5][1]);
    } catch (e) {}

    try {
      assertSame(__v_3, __v_0[__v_5][1]);
    } catch (e) {}

    /* VariableOrObjectMutator: Random mutation */
    try {
      if (__v_4 != null && typeof __v_4 == "object") try {
        Object.defineProperty(__v_4, __getRandomProperty(__v_4, 922707), {
          value: __v_2
        });
      } catch (e) {}
    } catch (e) {}

    /* ExpressionMutator: Cloned sibling */
    try {
      assertSame(__v_3, __v_0[__v_5][1]);
    } catch (e) {}
  }
} catch (e) {}

/* VariableOrObjectMutator: Random mutation */
try {
  delete __v_4[__getRandomProperty(__v_4, 892290)], __callGC();
} catch (e) {}

try {
  assertEquals(["get", __v_3, Symbol.isConcatSpreadable, __v_4], __v_0[0]);
} catch (e) {}

/* VariableOrObjectMutator: Random mutation */
try {
  if (__v_2 != null && typeof __v_2 == "object") try {
    Object.defineProperty(__v_2, __getRandomProperty(__v_2, 394476), {
      value: 4294967296
    });
  } catch (e) {}
} catch (e) {}

try {
  __callRandomFunction(__v_1, 718101, {
    valueOf: function () {
      return "0";
    }
  }, Math.PI, __v_1, __v_0, __getRandomObject(497502), __v_0);
} catch (e) {}

try {
  __v_0[__getRandomProperty(__v_0, 123736)] = -9007199254740992, __callGC();
} catch (e) {}

try {
  __v_4[__getRandomProperty(__v_4, 399972)] = __getRandomObject(940738), __callGC();
} catch (e) {}

try {
  assertEquals(["get", __v_3, "length", __v_4], __v_0[
  /* NumberMutator: Replaced 1 with -7 */
  -7]);
} catch (e) {}

try {
  assertEquals(["has", __v_3, "0"], __v_0[
  /* NumberMutator: Replaced 2 with 13 */
  13]);
} catch (e) {}

try {
  assertEquals(["get",
  /* VariableMutator: Replaced __v_3 with __v_0 */
  __v_0, "0", __v_4], __v_0[3]);
} catch (e) {}

try {
  assertEquals(["has", __v_3, "1"], __v_0[4]);
} catch (e) {}

try {
  assertEquals(["get", __v_3, "1", __v_4], __v_0[5]);
} catch (e) {}

try {
  __v_0.length = 0;
} catch (e) {}

try {
  assertEquals(["a", "b"], Array.prototype.concat.apply(__v_4));
} catch (e) {}

try {
  assertEquals(7, __v_0.length);
} catch (e) {}

try {
  for (var __v_5 in __v_0) try {
    assertSame(__v_3, __v_0[
    /* VariableMutator: Replaced __v_5 with __v_3 */
    __v_3][1]);
  } catch (e) {}
} catch (e) {}

try {
  assertEquals(["get", __v_3, "constructor", __v_4], __v_0[0]);
} catch (e) {}

/* VariableOrObjectMutator: Random mutation */
try {
  delete __v_1[__getRandomProperty(__v_1, 139211)], __callGC();
} catch (e) {}

try {
  __callRandomFunction(__v_1, 824982, __getRandomObject(189689), __getRandomObject(775221), __v_5, __v_5, new String(""), new Boolean(true), __v_5);
} catch (e) {}

try {
  __v_5 = __v_5, __callGC();
} catch (e) {}

try {
  if (__v_1 != null && typeof __v_1 == "object") try {
    Object.defineProperty(__v_1, __getRandomProperty(__v_1, 855388), {
      get: function () {
        try {
          __v_5[__getRandomProperty(__v_5, 692304)] = __getRandomObject(804923), __callGC();
        } catch (e) {}

        return __v_4;
      },
      set: function (value) {
        try {
          if (__getRandomObject(546310) != null && typeof __getRandomObject(546310) == "object") try {
            Object.defineProperty(__getRandomObject(546310), __getRandomProperty(__getRandomObject(546310), 2977), {
              get: function () {
                return 1073741823;
              },
              set: function (value) {}
            });
          } catch (e) {}
        } catch (e) {}
      }
    });
  } catch (e) {}
} catch (e) {}

try {
  __v_1[__getRandomProperty(__v_1, 400498)] = __getRandomObject(246785), __callGC();
} catch (e) {}

try {
  __callRandomFunction(__v_5, 977722);
} catch (e) {}

try {
  assertEquals(["get", __v_3, Symbol.isConcatSpreadable, __v_4],
  /* VariableMutator: Replaced __v_0 with __v_0 */
  __v_0[1]);
} catch (e) {}

try {
  assertEquals(["get", __v_3, "length", __v_4], __v_0[2]);
} catch (e) {}

try {
  assertEquals(["has", __v_3, "0"], __v_0[3]);
} catch (e) {}

try {
  assertEquals(["get", __v_3, "0",
  /* VariableMutator: Replaced __v_4 with __v_4 */
  __v_4], __v_0[4]);
} catch (e) {}

try {
  assertEquals(
  /* ArrayMutator: Duplicate an element (replaced) */
  ["has", __v_3, "1"], __v_0[5]);
} catch (e) {}

/* VariableOrObjectMutator: Random mutation */
try {
  delete __v_2[__getRandomProperty(__v_2, 632999)], __callGC();
} catch (e) {}

try {
  if (__v_1 != null && typeof __v_1 == "object") try {
    Object.defineProperty(__v_1, __getRandomProperty(__v_1, 182556), {
      value: __getRandomObject(490483)
    });
  } catch (e) {}
} catch (e) {}

try {
  __callRandomFunction(__v_4, 520713, __v_3);
} catch (e) {}

try {
  if (__v_3 != null && typeof __v_3 == "object") try {
    Object.defineProperty(__v_3, __getRandomProperty(__v_3, 858698), {
      get: function () {
        return 2147483647;
      },
      set: function (value) {
        try {
          __v_5 = __v_0, __callGC();
        } catch (e) {}
      }
    });
  } catch (e) {}
} catch (e) {}

try {
  assertEquals(["get", __v_3, "1", __v_4], __v_0[6]);
} catch (e) {}

// Original: WebKit/JSTests/stress/dataview-jit-neuter.js
function __f_0(__v_11) {
  if (!__v_11) throw new Error("Bad!");
}

function __f_1() {
  let __v_12 = new ArrayBuffer(
  /* NumberMutator: Replaced 2 with -4294967295 */
  -4294967295);

  let __v_13 = new Int16Array(__v_12);

  try {
    __v_13[0] = 0x0102;
  } catch (e) {}

  let __v_14 = new DataView(__v_12);

  return __v_14.getInt16(0, true) === 0x0102;
}
try {
let __v_10 = __f_1();
} catch { }

function __f_2(__v_15) {
  try {
    if (__v_10) return __v_15;
  } catch (e) {}

  let __v_16 = new ArrayBuffer(4);

  let __v_17 = new Uint32Array(__v_16);

  try {
    __v_17[0] = __v_15;
  } catch (e) {}

  let __v_18 = new DataView(__v_16);

  return __v_18.getUint32(0, true);
}

function __f_3() {
  function __f_5(__v_23, __v_24) {
    return __v_23.getUint8(__v_24);
  }

  try {
    noInline(__f_5);
  } catch (e) {}

  let __v_19 = new ArrayBuffer(4);

  let __v_20 = new Uint32Array(__v_19);

  try {
    __v_20[0] = __f_2(0xa070fa01);
  } catch (e) {}

  let __v_21 = new DataView(__v_19);

  try {
    for (let __v_25 = 0; __v_25 < 1000; ++__v_25) {
      try {
        __f_0(__f_5(__v_21, 0) === 0x01);
      } catch (e) {}
    }
  } catch (e) {}

  try {
    transferArrayBuffer(__v_19);
  } catch (e) {}

  let __v_22 = null;

  try {
    try {
      __f_5(__v_21, 0);
    } catch (e) {}
  } catch (__v_26) {
    __v_22 = __v_26;
  }

  try {
    __f_0(__v_22 instanceof TypeError);
  } catch (e) {}
}

try {
  __f_3();
} catch (e) {}

function __f_4() {
  function __f_6(__v_31, __v_32) {
    return __v_31.getUint8(__v_32);
  }

  try {
    noInline(__f_6);
  } catch (e) {}

  /* VariableOrObjectMutator: Random mutation */
  try {
    if (__v_4 != null && typeof __v_4 == "object") try {
      Object.defineProperty(__v_4, __getRandomProperty(__v_4, 762578), {
        value: __getRandomObject(86715)
      });
    } catch (e) {}
  } catch (e) {}

  /* CrossOverMutator: Crossover from spidermonkey/non262/generators/delegating-yield-2.js */
  try {
    __v_5.throw(42);
  } catch (e) {}

  let __v_27 = new ArrayBuffer(4);

  let __v_28 = new Uint32Array(__v_27);

  try {
    __v_28[0] = __f_2(0xa070fa01);
  } catch (e) {}

  let __v_29 = new DataView(__v_27);

  for (let __v_33 = 0; __v_33 < 10000; ++__v_33) {
    /* CrossOverMutator: Crossover from v8/test/mjsunit/compiler/regress-1067544.js */
    Int8Array.prototype.values.call([__v_5]);

    __f_0(__f_6(__v_29, 0) === 0x01);
  }

  try {
    transferArrayBuffer(__v_27);
  } catch (e) {}

  /* VariableOrObjectMutator: Random mutation */
  try {
    if (__v_0 != null && typeof __v_0 == "object") try {
      Object.defineProperty(__v_0, __getRandomProperty(__v_0, 815353), {
        value: []
      });
    } catch (e) {}
  } catch (e) {}

  let __v_30 = null;

  try {
    try {
      /* FunctionCallMutator: Run to stack limit __f_6 */
      runNearStackLimit(() => {
        return __f_6(__v_29, 0);
      });
    } catch (e) {}
  } catch (__v_34) {
    try {
      __v_30 = __v_34;
    } catch (e) {}
  }

  try {
    __f_0(__v_30 instanceof TypeError);
  } catch (e) {}
}

try {
  __f_4();
} catch (e) {}
