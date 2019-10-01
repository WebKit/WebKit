// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of assertion functions used throughout test262
defines: [assert]
---*/


function assert(mustBeTrue, message) {
  if (mustBeTrue === true) {
    return;
  }

  if (message === undefined) {
    message = 'Expected true but got ' + assert._toString(mustBeTrue);
  }
  $ERROR(message);
}

assert._isSameValue = function (a, b) {
  if (a === b) {
    // Handle +/-0 vs. -/+0
    return a !== 0 || 1 / a === 1 / b;
  }

  // Handle NaN vs. NaN
  return a !== a && b !== b;
};

assert.sameValue = function (actual, expected, message) {
  try {
    if (assert._isSameValue(actual, expected)) {
      return;
    }
  } catch (error) {
    $ERROR(message + ' (_isSameValue operation threw) ' + error);
    return;
  }

  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  message += 'Expected SameValue(«' + assert._toString(actual) + '», «' + assert._toString(expected) + '») to be true';

  $ERROR(message);
};

assert.notSameValue = function (actual, unexpected, message) {
  if (!assert._isSameValue(actual, unexpected)) {
    return;
  }

  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  message += 'Expected SameValue(«' + assert._toString(actual) + '», «' + assert._toString(unexpected) + '») to be false';

  $ERROR(message);
};

assert.throws = function (expectedErrorConstructor, func, message) {
  if (typeof func !== "function") {
    $ERROR('assert.throws requires two arguments: the error constructor ' +
      'and a function to run');
    return;
  }
  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  try {
    func();
  } catch (thrown) {
    if (typeof thrown !== 'object' || thrown === null) {
      message += 'Thrown value was not an object!';
      $ERROR(message);
    } else if (thrown.constructor !== expectedErrorConstructor) {
      message += 'Expected a ' + expectedErrorConstructor.name + ' but got a ' + thrown.constructor.name;
      $ERROR(message);
    }
    return;
  }

  message += 'Expected a ' + expectedErrorConstructor.name + ' to be thrown but no exception was thrown at all';
  $ERROR(message);
};

assert._formatValue = (value, seen) => {
  switch (typeof value) {
    case 'string':
      return typeof JSON !== "undefined" ? JSON.stringify(value) : `"${value}"`;
    case 'number':
    case 'boolean':
    case 'symbol':
    case 'bigint':
      return value.toString();
    case 'undefined':
      return 'undefined';
    case 'function':
      return `[Function${value.name ? `: ${value.name}` : ''}]`;
    case 'object':
      if (value === null) return 'null';
      if (value instanceof Date) return `Date "${value.toISOString()}"`;
      if (value instanceof RegExp) return value.toString();
      if (!seen) {
        seen = {
          counter: 0,
          map: new Map()
        };
      }

      let usage = seen.map.get(value);
      if (usage) {
        usage.used = true;
        return `[Ref: #${usage.id}]`;
      }

      usage = { id: ++seen.counter, used: false };
      seen.map.set(value, usage);

      if (typeof Set !== "undefined" && value instanceof Set) {
        return `Set {${Array.from(value).map(value => assert._formatValue(value, seen)).join(', ')}}${usage.used ? ` as #${usage.id}` : ''}`;
      }
      if (typeof Map !== "undefined" && value instanceof Map) {
        return `Map {${Array.from(value).map(pair => `${assert._formatValue(pair[0], seen)} => ${assert._formatValue(pair[1], seen)}}`).join(', ')}}${usage.used ? ` as #${usage.id}` : ''}`;
      }
      if (Array.isArray ? Array.isArray(value) : value instanceof Array) {
        return `[${value.map(value => assert._formatValue(value, seen)).join(', ')}]${usage.used ? ` as #${usage.id}` : ''}`;
      }
      let tag = Symbol.toStringTag in value ? value[Symbol.toStringTag] : 'Object';
      if (tag === 'Object' && Object.getPrototypeOf(value) === null) {
        tag = '[Object: null prototype]';
      }
      return `${tag ? `${tag} ` : ''}{ ${Object.keys(value).map(key => `${key.toString()}: ${assert._formatValue(value[key], seen)}`).join(', ')} }${usage.used ? ` as #${usage.id}` : ''}`;
    default:
      return typeof value;
  }
};

assert._toString = function (value) {
  try {
    return String(value);
  } catch (err) {
    if (err.name === 'TypeError') {
      return Object.prototype.toString.call(value);
    }

    throw err;
  }
};
