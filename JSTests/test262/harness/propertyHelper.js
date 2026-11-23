// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to safely verify the correctness of
    property descriptors.
defines:
  - verifyProperty
  - verifyCallableProperty
  - verifyAccessorProperty
  - verifyEqualTo # deprecated
  - verifyWritable # deprecated
  - verifyNotWritable # deprecated
  - verifyEnumerable # deprecated
  - verifyNotEnumerable # deprecated
  - verifyConfigurable # deprecated
  - verifyNotConfigurable # deprecated
  - verifyPrimordialProperty
  - verifyPrimordialCallableProperty
  - verifyPrimordialAccessorProperty
---*/

// @ts-check

var INFINITY = 1 / 0;

// Capture primordial functions and receiver-uncurried primordial methods that
// are used in verification but might be destroyed *by* that process itself.
var __String = String;
var __isArray = Array.isArray;
var __max = Math.max;
var __defineProperty = Object.defineProperty;
var __getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
var __getOwnPropertyNames = Object.getOwnPropertyNames;
var __join = Function.prototype.call.bind(Array.prototype.join);
var __push = Function.prototype.call.bind(Array.prototype.push);
var __hasOwnProperty = Function.prototype.call.bind(Object.prototype.hasOwnProperty);
var __propertyIsEnumerable = Function.prototype.call.bind(Object.prototype.propertyIsEnumerable);
var __getTypedArrayByteLength =
  Object.getPrototypeOf &&
  typeof Uint8Array !== "undefined" &&
  Function.prototype.call.bind(
    __getOwnPropertyDescriptor(Object.getPrototypeOf(Uint8Array.prototype), "byteLength").get
  );

/**
 * @param {object} obj
 * @param {string|symbol} name
 * @param {PropertyDescriptor|undefined} desc
 * @param {object} [options]
 * @param {boolean} [options.label]
 * @param {boolean} [options.restore] revert mutations from verifying writable/configurable
 */
function verifyProperty(obj, name, desc, options) {
  assert(
    arguments.length > 2,
    'verifyProperty should receive at least 3 arguments: obj, name, and descriptor'
  );
  var label = options && options.label || __String(name);

  var originalDesc = __getOwnPropertyDescriptor(obj, name);

  // Allows checking for undefined descriptor if it's explicitly given.
  if (desc === undefined) {
    assert.sameValue(
      originalDesc,
      undefined,
      label + " descriptor should be undefined"
    );

    // desc and originalDesc are both undefined, problem solved;
    return true;
  }

  assert(__hasOwnProperty(obj, name), label + " should be an own property");

  assert.notSameValue(
    desc,
    null,
    "The desc argument should be an object or undefined, null"
  );

  assert.sameValue(
    typeof desc,
    "object",
    "The desc argument should be an object or undefined, " + __String(desc)
  );

  var names = __getOwnPropertyNames(desc);
  for (var i = 0; i < names.length; i++) {
    assert(
      names[i] === "value" ||
        names[i] === "writable" ||
        names[i] === "enumerable" ||
        names[i] === "configurable" ||
        names[i] === "get" ||
        names[i] === "set",
      "Invalid descriptor field: " + names[i]
    );
  }

  var failures = [];

  if (__hasOwnProperty(desc, 'value')) {
    if (!isSameValue(desc.value, originalDesc.value)) {
      __push(failures, label + " descriptor value should be " + __String(desc.value));
    }
    if (!isSameValue(desc.value, obj[name])) {
      __push(failures, label + " value should be " + __String(desc.value));
    }
  }

  if (__hasOwnProperty(desc, 'enumerable') && desc.enumerable !== undefined) {
    if (desc.enumerable !== originalDesc.enumerable ||
        desc.enumerable !== isEnumerable(obj, name)) {
      __push(failures, label + " descriptor should " + (desc.enumerable ? '' : 'not ') + "be enumerable");
    }
  }

  // Operations past this point are potentially destructive!

  if (__hasOwnProperty(desc, 'writable') && desc.writable !== undefined) {
    if (desc.writable !== originalDesc.writable ||
        desc.writable !== isWritable(obj, name)) {
      __push(failures, label + " descriptor should " + (desc.writable ? '' : 'not ') + "be writable");
    }
  }

  if (__hasOwnProperty(desc, 'configurable') && desc.configurable !== undefined) {
    if (desc.configurable !== originalDesc.configurable ||
        desc.configurable !== isConfigurable(obj, name, originalDesc)) {
      __push(failures, label + " descriptor should " + (desc.configurable ? '' : 'not ') + "be configurable");
    }
  }

  if (failures.length) {
    assert(false, __join(failures, '; '));
  }

  if (options && options.restore) {
    __defineProperty(obj, name, originalDesc);
  }

  return true;
}

function _toNumber(value) {
  // https://tc39.es/ecma262/multipage/numbers-and-dates.html#sec-math.max
  return __max(value);
}

// https://tc39.es/ecma262/multipage/abstract-operations.html#sec-canonicalnumericindexstring
function _isCanonicalNumericIndexString(value) {
  if (typeof value !== "string") return false;
  if (value === "-0") return true;
  var n = _toNumber(value);
  return __String(n) === value;
}

function _isTypedArray(value) {
  try {
    // https://tc39.es/ecma262/multipage/indexed-collections.html#sec-get-%typedarray%.prototype.bytelength
    __getTypedArrayByteLength(value);
    return true;
  } catch (_err) {
    return false;
  }
}

function isConfigurable(obj, name, originalDesc) {
  try {
    delete obj[name];
  } catch (e) {
    if (!(e instanceof TypeError)) {
      throw new Test262Error("Expected TypeError, got " + e);
    }
  }
  var deleted = !__hasOwnProperty(obj, name);

  // TypedArray canonical numeric string properties are never *actually* deleted,
  // so we skip post-hoc verification for them
  // https://tc39.es/ecma262/multipage/ordinary-and-exotic-objects-behaviours.html#sec-typedarray-delete
  if (_isCanonicalNumericIndexString(name) && _isTypedArray(obj) && originalDesc) {
    if (deleted) throw new Test262Error("Expected TypedArray index deletion to be ignored");
    return originalDesc.configurable;
  }

  return deleted;
}

function isEnumerable(obj, name) {
  var stringCheck = false;

  if (typeof name === "string") {
    for (var x in obj) {
      if (x === name) {
        stringCheck = true;
        break;
      }
    }
  } else {
    // skip it if name is not string, works for Symbol names.
    stringCheck = true;
  }

  return stringCheck && __hasOwnProperty(obj, name) && __propertyIsEnumerable(obj, name);
}

function isSameValue(a, b) {
  if (a === 0 && b === 0) return 1 / a === 1 / b;
  if (a !== a && b !== b) return true;

  return a === b;
}

function isWritable(obj, name, verifyProp, value) {
  var hadValue = __hasOwnProperty(obj, name);
  var oldValue = obj[name];
  var newValue = value;
  if (arguments.length < 4) {
    // Initialize newValue, preserving a numeric type from oldValue with a
    // replacement that moves towards zero (to support the special behaviour of
    // Array and TypedArray "length" and index properties).
    switch (typeof oldValue) {
      case "number":
        // As a special case, never decrease array length (which would delete
        // properties not covered by the new length).
        if (oldValue <= 0 || __isArray(obj) && name === "length") {
          newValue = oldValue === -INFINITY ? 0 : oldValue + 1;
        } else if (oldValue < INFINITY) {
          newValue = oldValue - 1;
        } else {
          newValue = 0;
        }
        break;
      case "bigint":
        newValue = oldValue <= BigInt(0) ? oldValue + BigInt(1) : oldValue - BigInt(1);
        break;
      default:
        newValue = oldValue !== "unlikelyValue" ? "unlikelyValue" : "unlikelyValue2";
        break;
    }
  }
  var writeSucceeded;

  try {
    obj[name] = newValue;
  } catch (e) {
    if (!(e instanceof TypeError)) {
      throw new Test262Error("Expected TypeError, got " + e);
    }
  }

  writeSucceeded = isSameValue(obj[verifyProp || name], newValue);

  // Revert the change only if it was successful (in other cases, reverting
  // is unnecessary and may trigger exceptions for certain property
  // configurations)
  if (writeSucceeded) {
    if (hadValue) {
      obj[name] = oldValue;
    } else {
      delete obj[name];
    }
  }

  return writeSucceeded;
}

/**
 * Verify that there is a function of specified name, length, and containing
 * descriptor associated with `obj[name]` and following the conventions for
 * built-in objects.
 *
 * @param {object} obj
 * @param {string|symbol} name
 * @param {string} [functionName] defaults to name for strings, `[${name.description}]` for symbols
 * @param {number} functionLength
 * @param {PropertyDescriptor} [desc] defaults to data property conventions (writable, non-enumerable, configurable)
 * @param {object} [options]
 * @param {boolean} [options.label]
 * @param {typeof verifyProperty} [options.verifyProperty]
 * @param {boolean} [options.restore] revert mutations from verifying writable/configurable
 */
function verifyCallableProperty(obj, name, functionName, functionLength, desc, options) {
  var label = options && options.label || __String(name);
  var propertyVerifier = options && options.verifyProperty || verifyProperty;

  var value = obj && obj[name];

  assert.sameValue(typeof value, "function", label + " should be a function");

  // Every other data property described in clauses 19 through 28 and in
  // Annex B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
  // [[Configurable]]: true } unless otherwise specified.
  // https://tc39.es/ecma262/multipage/ecmascript-standard-built-in-objects.html
  if (desc === undefined) {
    desc = {
      writable: true,
      enumerable: false,
      configurable: true,
      value: value
    };
  } else if (!__hasOwnProperty(desc, "value") && !__hasOwnProperty(desc, "get")) {
    desc.value = value;
  }

  propertyVerifier(obj, name, desc, options);

  if (functionName === undefined) {
    if (typeof name === "symbol") {
      functionName = "[" + name.description + "]";
    } else {
      functionName = name;
    }
  }
  // Unless otherwise specified, the "name" property of a built-in function
  // object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  // [[Configurable]]: true }.
  // https://tc39.es/ecma262/multipage/ecmascript-standard-built-in-objects.html#sec-ecmascript-standard-built-in-objects
  // https://tc39.es/ecma262/multipage/ordinary-and-exotic-objects-behaviours.html#sec-setfunctionname
  propertyVerifier(value, "name", {
    value: functionName,
    writable: false,
    enumerable: false,
    configurable: desc.configurable
  }, { label: label + " name", restore: options && options.restore });

  // Unless otherwise specified, the "length" property of a built-in function
  // object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  // [[Configurable]]: true }.
  // https://tc39.es/ecma262/multipage/ecmascript-standard-built-in-objects.html#sec-ecmascript-standard-built-in-objects
  // https://tc39.es/ecma262/multipage/ordinary-and-exotic-objects-behaviours.html#sec-setfunctionlength
  propertyVerifier(value, "length", {
    value: functionLength,
    writable: false,
    enumerable: false,
    configurable: desc.configurable
  }, { label: label + " length", restore: options && options.restore });
}

/**
 * Verify that there is an accessor property associated with `obj[name]` and
 * following the conventions for built-in objects.
 *
 * @param {object} obj
 * @param {string|symbol} name
 * @param {object} desc
 * @param {boolean} [desc.enumerable] defaults to accessor property conventions (non-enumerable)
 * @param {boolean} [desc.configurable] defaults to accessor property conventions (configurable)
 * @param {undefined | Function | {name?: string|symbol, length?: number}} [desc.get] if an object,
 *   absent fields default to getter conventions (name derived from the property key with a "get "
 *   prefix, length 0)
 * @param {undefined | Function | {name?: string|symbol, length?: number}} [desc.set] if an object,
 *   absent fields default to getter conventions (name derived from the property key with a "set "
 *   prefix, length 1)
 * @param {object} [options]
 * @param {boolean} [options.label]
 * @param {typeof verifyProperty} [options.verifyProperty]
 * @param {typeof verifyCallableProperty} [options.verifyCallableProperty]
 * @param {boolean} [options.restore] revert mutations from verifying property attributes
 */
function verifyAccessorProperty(obj, name, desc, options) {
  var checkGet = __hasOwnProperty(desc, "get");
  var checkSet = __hasOwnProperty(desc, "set");
  assert(
    checkGet || checkSet,
    'verifyAccessorProperty requires at least one of "get" and "set"'
  );
  var label = options && options.label || __String(name);
  var propertyVerifier = options && options.verifyProperty || verifyProperty;
  var callabilityVerifier = options && options.verifyCallableProperty || verifyCallableProperty;

  var originalDesc = __getOwnPropertyDescriptor(obj, name);

  // Every built-in function object, including constructors, has a "name"
  // property whose value is a String. Unless otherwise specified, this value is
  // the name that is given to the function in this specification. Functions
  // that are identified as anonymous functions use the empty String as the
  // value of the "name" property. For functions that are specified as
  // properties of objects, the name value is the property name string used to
  // access the function. Functions that are specified as get or set accessor
  // functions of built-in properties have "get" or "set" (respectively) passed
  // to the prefix parameter when calling CreateBuiltinFunction.
  //
  // The value of the "name" property is explicitly specified for each built-in
  // functions whose property key is a Symbol value. If such an explicitly
  // specified value starts with the prefix "get " or "set " and the function
  // for which it is specified is a get or set accessor function of a built-in
  // property, the value without the prefix is passed to the name parameter, and
  // the value "get" or "set" (respectively) is passed to the prefix parameter
  // when calling CreateBuiltinFunction.
  // https://tc39.es/ecma262/multipage/ecmascript-standard-built-in-objects.html
  if (checkGet) {
    var expectGetter = desc.get;
    var getterLabel = label + " getter";
    if (expectGetter === undefined || typeof expectGetter === "function") {
      assert.sameValue(originalDesc.get, expectGetter, getterLabel);
    } else {
      var getterName = expectGetter.name;
      if (getterName === undefined) {
        getterName = "get " + (typeof name === "symbol" ? "[" + name.description + "]" : name);
      }
      var getterLength = expectGetter.length !== undefined ? expectGetter.length : 0;
      var getterOptions = { label: getterLabel };
      callabilityVerifier(originalDesc, "get", getterName, getterLength, {}, getterOptions);
    }
  }
  if (checkSet) {
    var expectSetter = desc.set;
    var setterLabel = label + " setter";
    if (expectSetter === undefined || typeof expectSetter === "function") {
      assert.sameValue(originalDesc.set, expectSetter, setterLabel);
    } else {
      var setterName = expectSetter.name;
      if (setterName === undefined) {
        setterName = "set " + (typeof name === "symbol" ? "[" + name.description + "]" : name);
      }
      var setterLength = expectSetter.length !== undefined ? expectSetter.length : 1;
      var setterOptions = { label: setterLabel };
      callabilityVerifier(originalDesc, "set", setterName, setterLength, {}, setterOptions);
    }
  }

  // Every accessor property described in clauses 19 through 28 and in Annex B.2
  // has the attributes { [[Enumerable]]: false, [[Configurable]]: true } unless
  // otherwise specified.
  // https://tc39.es/ecma262/multipage/ecmascript-standard-built-in-objects.html
  var resolvedDesc = { get: originalDesc.get, set: originalDesc.set };
  if (!__hasOwnProperty(desc, "enumerable")) {
    resolvedDesc.enumerable = false;
  } else if (desc.enumerable !== undefined) {
    resolvedDesc.enumerable = desc.enumerable;
  }
  if (!__hasOwnProperty(desc, "configurable")) {
    resolvedDesc.configurable = true;
  } else if (desc.configurable !== undefined) {
    resolvedDesc.configurable = desc.configurable;
  }
  propertyVerifier(obj, name, resolvedDesc, options);
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyEqualTo(obj, name, value) {
  if (!isSameValue(obj[name], value)) {
    throw new Test262Error("Expected obj[" + __String(name) + "] to equal " + value +
           ", actually " + obj[name]);
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyWritable(obj, name, verifyProp, value) {
  if (!verifyProp) {
    assert(__getOwnPropertyDescriptor(obj, name).writable,
         "Expected obj[" + __String(name) + "] to have writable:true.");
  }
  var result = arguments.length < 4
    ? isWritable(obj, name, verifyProp)
    : isWritable(obj, name, verifyProp, value);
  if (!result) {
    throw new Test262Error("Expected obj[" + __String(name) + "] to be writable, but was not.");
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyNotWritable(obj, name, verifyProp, value) {
  if (!verifyProp) {
    assert(!__getOwnPropertyDescriptor(obj, name).writable,
         "Expected obj[" + __String(name) + "] to have writable:false.");
  }
  var result = arguments.length < 4
    ? isWritable(obj, name, verifyProp)
    : isWritable(obj, name, verifyProp, value);
  if (result) {
    throw new Test262Error("Expected obj[" + __String(name) + "] NOT to be writable, but was.");
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyEnumerable(obj, name) {
  assert(__getOwnPropertyDescriptor(obj, name).enumerable,
       "Expected obj[" + __String(name) + "] to have enumerable:true.");
  if (!isEnumerable(obj, name)) {
    throw new Test262Error("Expected obj[" + __String(name) + "] to be enumerable, but was not.");
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyNotEnumerable(obj, name) {
  assert(!__getOwnPropertyDescriptor(obj, name).enumerable,
       "Expected obj[" + __String(name) + "] to have enumerable:false.");
  if (isEnumerable(obj, name)) {
    throw new Test262Error("Expected obj[" + __String(name) + "] NOT to be enumerable, but was.");
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyConfigurable(obj, name) {
  assert(__getOwnPropertyDescriptor(obj, name).configurable,
       "Expected obj[" + __String(name) + "] to have configurable:true.");
  if (!isConfigurable(obj, name)) {
    throw new Test262Error("Expected obj[" + __String(name) + "] to be configurable, but was not.");
  }
}

/**
 * Deprecated; please use `verifyProperty` in new tests.
 */
function verifyNotConfigurable(obj, name) {
  assert(!__getOwnPropertyDescriptor(obj, name).configurable,
       "Expected obj[" + __String(name) + "] to have configurable:false.");
  if (isConfigurable(obj, name)) {
    throw new Test262Error("Expected obj[" + __String(name) + "] NOT to be configurable, but was.");
  }
}

/**
 * Use this function to verify the properties of a primordial object.
 * For non-primordial objects, use verifyProperty.
 * See: https://github.com/tc39/how-we-work/blob/main/terminology.md#primordial
 */
var verifyPrimordialProperty = verifyProperty;

/**
 * Use this function to verify the primordial function-valued properties.
 * For non-primordial functions, use verifyCallableProperty.
 * See: https://github.com/tc39/how-we-work/blob/main/terminology.md#primordial
 *
 * @type {typeof verifyCallableProperty}
 */
function verifyPrimordialCallableProperty(obj, name, functionName, functionLength, desc, options) {
  var resolvedOptions = {
    verifyProperty: options && options.verifyProperty !== undefined
      ? options.verifyProperty
      : verifyPrimordialProperty
  };
  if (options && options.label !== undefined) resolvedOptions.label = options.label;
  if (options && options.restore !== undefined) resolvedOptions.restore = options.restore;

  return verifyCallableProperty(obj, name, functionName, functionLength, desc, resolvedOptions);
}

/**
 * Use this function to verify the primordial accessor properties.
 * For non-primordial functions, use verifyAccessorProperty.
 * See: https://github.com/tc39/how-we-work/blob/main/terminology.md#primordial
 *
 * @type {typeof verifyAccessorProperty}
 */
function verifyPrimordialAccessorProperty(obj, name, desc, options) {
  var resolvedOptions = {
    verifyProperty: options && options.verifyProperty !== undefined
      ? options.verifyProperty
      : verifyPrimordialProperty,
    verifyCallableProperty: options && options.verifyCallableProperty !== undefined
      ? options.verifyCallableProperty
      : verifyPrimordialCallableProperty
  };
  if (options && options.label !== undefined) resolvedOptions.label = options.label;
  if (options && options.restore !== undefined) resolvedOptions.restore = options.restore;

  return verifyAccessorProperty(obj, name, desc, resolvedOptions);
}
