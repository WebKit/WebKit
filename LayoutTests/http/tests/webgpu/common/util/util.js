/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { Logger } from '../internal/logging/logger.js';import { timeout } from './timeout.js';

/**
 * Error with arbitrary `extra` data attached, for debugging.
 * The extra data is omitted if not running the test in debug mode (`?debug=1`).
 */
export class ErrorWithExtra extends Error {


  /**
   * `extra` function is only called if in debug mode.
   * If an `ErrorWithExtra` is passed, its message is used and its extras are passed through.
   */


  constructor(baseOrMessage, newExtra) {
    const message = typeof baseOrMessage === 'string' ? baseOrMessage : baseOrMessage.message;
    super(message);

    const oldExtras = baseOrMessage instanceof ErrorWithExtra ? baseOrMessage.extra : {};
    this.extra = Logger.globalDebugMode ?
    { ...oldExtras, ...newExtra() } :
    { omitted: 'pass ?debug=1' };
  }}


/**
 * Asserts `condition` is true. Otherwise, throws an `Error` with the provided message.
 */
export function assert(condition, msg) {
  if (!condition) {
    throw new Error(msg && (typeof msg === 'string' ? msg : msg()));
  }
}

/** If the argument is an Error, throw it. Otherwise, pass it back. */
export function assertOK(value) {
  if (value instanceof Error) {
    throw value;
  }
  return value;
}

/**
 * Resolves if the provided promise rejects; rejects if it does not.
 */
export async function assertReject(p, msg) {
  try {
    await p;
    unreachable(msg);
  } catch (ex) {
    // Assertion OK
  }
}

/**
 * Assert this code is unreachable. Unconditionally throws an `Error`.
 */
export function unreachable(msg) {
  throw new Error(msg);
}

/**
 * The `performance` interface.
 * It is available in all browsers, but it is not in scope by default in Node.
 */
const perf = typeof performance !== 'undefined' ? performance : require('perf_hooks').performance;

/**
 * Calls the appropriate `performance.now()` depending on whether running in a browser or Node.
 */
export function now() {
  return perf.now();
}

/**
 * Returns a promise which resolves after the specified time.
 */
export function resolveOnTimeout(ms) {
  return new Promise((resolve) => {
    timeout(() => {
      resolve();
    }, ms);
  });
}

export class PromiseTimeoutError extends Error {}

/**
 * Returns a promise which rejects after the specified time.
 */
export function rejectOnTimeout(ms, msg) {
  return new Promise((_resolve, reject) => {
    timeout(() => {
      reject(new PromiseTimeoutError(msg));
    }, ms);
  });
}

/**
 * Takes a promise `p`, and returns a new one which rejects if `p` takes too long,
 * and otherwise passes the result through.
 */
export function raceWithRejectOnTimeout(p, ms, msg) {
  // Setup a promise that will reject after `ms` milliseconds. We cancel this timeout when
  // `p` is finalized, so the JavaScript VM doesn't hang around waiting for the timer to
  // complete, once the test runner has finished executing the tests.
  const timeoutPromise = new Promise((_resolve, reject) => {
    const handle = timeout(() => {
      reject(new PromiseTimeoutError(msg));
    }, ms);
    p = p.finally(() => clearTimeout(handle));
  });
  return Promise.race([p, timeoutPromise]);
}

/**
 * Makes a copy of a JS `object`, with the keys reordered into sorted order.
 */
export function sortObjectByKey(v) {
  const sortedObject = {};
  for (const k of Object.keys(v).sort()) {
    sortedObject[k] = v[k];
  }
  return sortedObject;
}

/**
 * Determines whether two JS values are equal, recursing into objects and arrays.
 */
export function objectEquals(x, y) {
  if (typeof x !== 'object' || typeof y !== 'object') return x === y;
  if (x === null || y === null) return x === y;
  if (x.constructor !== y.constructor) return false;
  if (x instanceof Function) return x === y;
  if (x instanceof RegExp) return x === y;
  if (x === y || x.valueOf() === y.valueOf()) return true;
  if (Array.isArray(x) && Array.isArray(y) && x.length !== y.length) return false;
  if (x instanceof Date) return false;
  if (!(x instanceof Object)) return false;
  if (!(y instanceof Object)) return false;

  const x1 = x;
  const y1 = y;
  const p = Object.keys(x);
  return Object.keys(y).every((i) => p.indexOf(i) !== -1) && p.every((i) => objectEquals(x1[i], y1[i]));
}

/**
 * Generates a range of values `fn(0)..fn(n-1)`.
 */
export function range(n, fn) {
  return [...new Array(n)].map((_, i) => fn(i));
}

/**
 * Generates a range of values `fn(0)..fn(n-1)`.
 */
export function* iterRange(n, fn) {
  for (let i = 0; i < n; ++i) {
    yield fn(i);
  }
}

































function subarrayAsU8(
buf,
{ start = 0, length })
{
  if (buf instanceof ArrayBuffer) {
    return new Uint8Array(buf, start, length);
  } else {
    const sub = buf.subarray(start, length !== undefined ? start + length : undefined);
    return new Uint8Array(sub.buffer, sub.byteOffset, sub.byteLength);
  }
}

/**
 * Copy a range of bytes from one ArrayBuffer or TypedArray to another.
 *
 * `start`/`length` are in elements (or in bytes, if ArrayBuffer).
 */
export function memcpy(
src,
dst)
{
  subarrayAsU8(dst.dst, dst).set(subarrayAsU8(src.src, src));
}
//# sourceMappingURL=util.js.map