// MIT License

// Copyright (c) 2020 Robin Malfait

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

"use strict";

function ensureFunction(input) {
    return typeof input === 'function' ? input : () => input;
}

function pipe(...fns) {
    return fns.reduceRight((f, g) => {
        let g_ = ensureFunction(g);
        return (...args) => f(g_(...args));
    }, ensureFunction(fns.pop()));
}

function isIterable(input) {
    if (typeof input !== 'object' || input === null)
        return false;
    return input[Symbol.iterator] !== undefined;
}
function isAsyncIterable(input) {
    if (typeof input !== 'object' || input === null)
        return false;
    return input[Symbol.asyncIterator] !== undefined;
}

function every(predicate) {
    return function everyFn(data) {
        if (data == null)
            return false;
        if (isAsyncIterable(data) || data instanceof Promise) {
            return (async () => {
                let stream = data instanceof Promise ? await data : data;
                let i = 0;
                for await (let datum of stream) {
                    if (!predicate(datum, i++))
                        return false;
                }
                return true;
            })();
        }
        let i = 0;
        for (let datum of data) {
            if (!predicate(datum, i++))
                return false;
        }
        return true;
    };
}

function filter(fn) {
    return function filterFn(data) {
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    let i = 0;
                    for await (let datum of stream) {
                        // Ignore values that do not meet the criteria
                        if (!fn(datum, i++))
                            continue;
                        yield datum;
                    }
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                let i = 0;
                for (let datum of data) {
                    // Ignore values that do not meet the criteria
                    if (!fn(datum, i++))
                        continue;
                    yield datum;
                }
            },
        };
    };
}

function flatMap(fn) {
    return function flatMapFn(data) {
        if (data == null)
            return;
        // Handle the async version
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    let i = 0;
                    for await (let datum of stream) {
                        let result = fn(datum, i++);
                        if (isAsyncIterable(result) || result instanceof Promise) {
                            let stream = result instanceof Promise ? await result : result;
                            yield* stream;
                        }
                        else if (isIterable(result)) {
                            yield* result;
                        }
                        else {
                            yield result;
                        }
                    }
                },
            };
        }
        // Handle the sync version
        return {
            *[Symbol.iterator]() {
                let i = 0;
                for (let datum of data) {
                    let result = fn(datum, i++);
                    if (isIterable(result)) {
                        yield* result;
                    }
                    else {
                        yield result;
                    }
                }
            },
        };
    };
}

function reduce(fn, initial) {
    return function reduceFn(data) {
        if (data == null)
            return;
        let acc = initial;
        if (isAsyncIterable(data) || data instanceof Promise) {
            return (async () => {
                let stream = data instanceof Promise ? await data : data;
                let i = 0;
                for await (let datum of stream)
                    acc = fn(acc, datum, i++);
                return acc;
            })();
        }
        let i = 0;
        for (let datum of data)
            acc = fn(acc, datum, i++);
        return acc;
    };
}

function map(fn) {
    return function mapFn(data) {
        if (data == null)
            return;
        // Handle the async version
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    let i = 0;
                    for await (let datum of stream)
                        yield fn(datum, i++);
                },
            };
        }
        // Handle the sync version
        return {
            *[Symbol.iterator]() {
                let i = 0;
                for (let datum of data)
                    yield fn(datum, i++);
            },
        };
    };
}

function toArray() {
    return (data) => {
        return reduce((acc, current) => {
            acc.push(current);
            return acc;
        }, [])(data);
    };
}

function slice(begin = 0, end = Infinity) {
    return function sliceFn(data) {
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    let iterator = stream;
                    let local_begin = begin;
                    let local_end = end - local_begin;
                    // Skip the first X values
                    while (local_begin-- > 0)
                        iterator.next();
                    // Loop through the remaining items until the end is reached
                    for await (let datum of iterator) {
                        yield datum;
                        if (--local_end < 0)
                            return;
                    }
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                let iterator = Array.isArray(data)
                    ? data[Symbol.iterator]()
                    : data;
                let local_begin = begin;
                let local_end = end - local_begin;
                // Skip the first X values
                while (local_begin-- > 0)
                    iterator.next();
                // Loop through the remaining items until the end is reached
                for (let datum of iterator) {
                    yield datum;
                    if (--local_end < 0)
                        return;
                }
            },
        };
    };
}

function chunk(size) {
    return function chunkFn(data) {
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    // Let's have a placeholder for our current chunk
                    let chunk = [];
                    // Loop over our data
                    for await (let datum of stream) {
                        // Add item to our current chunk
                        chunk.push(datum);
                        if (chunk.length === size) {
                            // Our current chunk is full, let's yield it
                            yield chunk;
                            // Let's also clear our chunk for the next chunk
                            chunk = [];
                        }
                    }
                    // When the chunk is not full yet, but when we are at the end of the data we
                    // have to ensure that this one is also yielded
                    if (chunk.length > 0)
                        yield chunk;
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                // Let's have a placeholder for our current chunk
                let chunk = [];
                // Loop over our data
                for (let datum of data) {
                    // Add item to our current chunk
                    chunk.push(datum);
                    if (chunk.length === size) {
                        // Our current chunk is full, let's yield it
                        yield chunk;
                        // Let's also clear our chunk for the next chunk
                        chunk = [];
                    }
                }
                // When the chunk is not full yet, but when we are at the end of the data we
                // have to ensure that this one is also yielded
                if (chunk.length > 0)
                    yield chunk;
            },
        };
    };
}

function flatten(options = {}) {
    let { shallow = false } = options;
    return function flattenFn(data) {
        if (data == null)
            return;
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    for await (let datum of stream) {
                        if (shallow) {
                            // If the value itself is an iterator, we have to flatten that as
                            // well.
                            if (isAsyncIterable(datum)) {
                                yield* datum;
                            }
                            else {
                                yield datum;
                            }
                        }
                        else {
                            // Let's go recursive
                            yield* await flattenFn(datum);
                        }
                    }
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                if (!isIterable(data)) {
                    yield data;
                }
                else {
                    for (let datum of data) {
                        if (shallow) {
                            // If the value itself is an iterator, we have to flatten that as
                            // well.
                            if (isIterable(datum)) {
                                yield* datum;
                            }
                            else {
                                yield datum;
                            }
                        }
                        else {
                            // Let's go recursive
                            yield* flattenFn(datum);
                        }
                    }
                }
            },
        };
    };
}

function clamp(input, min = Number.MIN_SAFE_INTEGER, max = Number.MAX_SAFE_INTEGER) {
    return Math.min(max, Math.max(min, input));
}

function* range(lowerbound, upperbound, step = 1) {
    let fixed_step = clamp(lowerbound < upperbound ? (step > 0 ? step : -step) : step > 0 ? -step : step);
    let lowerbound_ = clamp(lowerbound);
    let upperbound_ = clamp(upperbound);
    for (let i = lowerbound_; lowerbound_ < upperbound_ ? i <= upperbound_ : i >= upperbound_; i += fixed_step) {
        yield i;
    }
}

function take(amount) {
    return slice(0, Math.max(0, amount - 1));
}

function takeWhile(fn) {
    return function takeWhileFn(data) {
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    let i = 0;
                    for await (let datum of stream) {
                        if (!fn(datum, i++))
                            return;
                        yield datum;
                    }
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                let i = 0;
                for (let datum of data) {
                    if (!fn(datum, i++))
                        return;
                    yield datum;
                }
            },
        };
    };
}

function unique() {
    return function uniqueFn(data) {
        let seen = new Set([]);
        if (isAsyncIterable(data) || data instanceof Promise) {
            return {
                async *[Symbol.asyncIterator]() {
                    let stream = data instanceof Promise ? await data : data;
                    for await (let datum of stream) {
                        if (!seen.has(datum)) {
                            seen.add(datum);
                            yield datum;
                        }
                    }
                },
            };
        }
        return {
            *[Symbol.iterator]() {
                for (let datum of data) {
                    if (!seen.has(datum)) {
                        seen.add(datum);
                        yield datum;
                    }
                }
            },
        };
    };
}

function windows(size) {
    return function* windowsFn(data) {
        let result = [];
        for (let item of data) {
            result.push(item);
            if (result.length === size) {
                yield result.slice(-size);
                result.shift();
            }
        }
        result.splice(0);
    };
}

const naturalNumbers = () => range(1, Infinity);
const isPrime = n => every(x => n % x)(range(2, n - 1));
const factors = n => filter(x => !(n % x))(range(1, n));
const powers = (start, power) => map(x => x ** power)(range(start, Infinity));

function* factorial() {
  let a = 1n, b = 1n;

  for (;;) {
    b *= a++;
    yield b;
  }
}

function* fibonacci() {
  let a = 0, b = 1, temp;

  for (;;) {
    yield a;
    temp = a;
    a = b;
    b += temp;
  }
}

class BinaryTree {
  constructor(value, left, right) {
    this.value = value;
    this.left = left;
    this.right = right;
  }

  * [Symbol.iterator]() {
    yield this.value;
    yield* this.left ?? [];
    yield* this.right ?? [];
  }
}

const binaryRoot = new BinaryTree(0,
  new BinaryTree(1,
    new BinaryTree(2,
      new BinaryTree(4,
        new BinaryTree(6,
          new BinaryTree(10)),
        new BinaryTree(7,
          new BinaryTree(3,
            new BinaryTree(25))),
      ),
      new BinaryTree(5,
        new BinaryTree(8),
        new BinaryTree(9,
          new BinaryTree(11,
            new BinaryTree(24))),
      ),
    ),
  ),
  new BinaryTree(12,
    new BinaryTree(13,
      new BinaryTree(20,
        new BinaryTree(15,
          new BinaryTree(14,
            new BinaryTree(22,
              new BinaryTree(23))),
          ),
        new BinaryTree(21),
      ),
      new BinaryTree(19,
        new BinaryTree(16),
        new BinaryTree(18,
          new BinaryTree(17)),
      ),
    ),
  ),
);

class Benchmark {
  runIteration() {
    this.totalLength = 0;

    const example = pipe(
      range(0, 10_000_000),
      filter(x => x % 100 === 0),
      filter(x => x % 4 === 0),
      filter(x => x % 400 === 0),
      takeWhile(x => x < 1_000),
      toArray(),
    );

    this.totalLength += example().length;

    const factorials = pipe(
      factorial(),
      chunk(3),
      flatten(),
      take(100),
      toArray(),
    );

    this.totalLength += factorials().length;

    const doubleFibonaccis = pipe(
      fibonacci(),
      take(78),
      windows(3),
      flatMap(x => x * 2),
      unique(),
      toArray(),
    );

    this.totalLength += doubleFibonaccis().length;

    const mersennePrimes = pipe(
      naturalNumbers(),
      map(x => 2 ** (x + 1) - 1),
      filter(isPrime),
      take(5),
      toArray(),
    );

    this.totalLength += mersennePrimes().length;

    const primes = pipe(
      naturalNumbers(),
      filter(isPrime),
      take(150),
      toArray(),
    );

    this.totalLength += primes().length;

    const powersOfTwo = pipe(
      powers(1, 2),
      chunk(2),
      flatten(),
      take(100),
      toArray(),
    );

    this.totalLength += powersOfTwo().length;

    const primeFactors = pipe(
      naturalNumbers(),
      flatMap(factors),
      filter(isPrime),
      unique(),
      take(150),
      toArray(),
    );

    this.totalLength += primeFactors().length;

    const fizzbuzz = pipe(
      naturalNumbers(),
      map(x => {
        if (!(x % 15)) return "fizzbuzz";
        if (!(x % 5)) return "buzz";
        if (!(x % 3)) return "fizz";
        return x;
      }),
      take(100),
      toArray(),
    );

    this.totalLength += fizzbuzz().length;

    const binaryTree = pipe(
      binaryRoot[Symbol.iterator](),
      flatMap(factors),
      windows(4),
      flatten(),
      map(x => String.fromCharCode("a".charCodeAt() + x)),
      unique(),
      toArray(),
    );

    this.totalLength += binaryTree().length;
  }

  validate() {
    if (this.totalLength !== 635)
      throw new Error(`this.totalLength of ${this.totalLength} is invalid!`);
  }
}
