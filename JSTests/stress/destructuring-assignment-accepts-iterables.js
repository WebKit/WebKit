function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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
        throw new Error(`bad error: ${String(error)}`);
}

(function () {
    var [a, b, c] = [1, 2, 3];
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(c, 3);
}());

(function () {
    var [a, b, c] = [1, 2, 3].keys();
    shouldBe(a, 0);
    shouldBe(b, 1);
    shouldBe(c, 2);
}());

(function () {
    var [a, b, c] = [1, 2, 3].values();
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(c, 3);
}());

(function () {
    var [a, , c] = [1, 2, 3].values();
    shouldBe(a, 1);
    shouldBe(c, 3);
}());

(function () {
    var [a, b, c] = [1, , 3].values();
    shouldBe(a, 1);
    shouldBe(b, undefined);
    shouldBe(c, 3);
}());

(function () {
    var [, b, c] = [1, 2, 3, 4, 5, 6].values();
    shouldBe(b, 2);
    shouldBe(c, 3);
}());

(function () {
    var [a, b, c] = [1].values();
    shouldBe(a, 1);
    shouldBe(b, undefined);
    shouldBe(c, undefined);
}());

(function ([a, b, c]) {
    shouldBe(a, 1);
    shouldBe(b, undefined);
    shouldBe(c, undefined);
}([1].values()));

(function () {
    var [a = 0, b = 2, c = 3] = [1].values();
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(c, 3);
}());

(function () {
    var [a = 1, b = 2, c = 3] = [undefined, undefined, undefined];
    shouldBe(a, 1);
    shouldBe(b, 2);
    shouldBe(c, 3);
}());

// String with a surrogate pair.
(function () {
    var string = "𠮷野家";
    var [a, b, c] = string;
    shouldBe(string.length, 4);
    shouldBe(a, '𠮷');
    shouldBe(b, '野');
    shouldBe(c, '家');
}());

(function () {
    var set = new Set([1, 2, 3]);
    var [a, b, c] = set;
    shouldBe(set.has(a), true);
    shouldBe(set.has(b), true);
    shouldBe(set.has(c), true);
}());

(function () {
    var map = new Map([[1, 1], [2, 2], [3, 3]]);
    var [a, b, c] = map;
    shouldBe(Array.isArray(a), true);
    shouldBe(Array.isArray(b), true);
    shouldBe(Array.isArray(c), true);
    shouldBe(map.has(a[0]), true);
    shouldBe(map.has(b[0]), true);
    shouldBe(map.has(c[0]), true);
}());

// Errors

shouldThrow(function () {
    var [a, b, c] = {
        [Symbol.iterator]() {
            return 42;
        }
    };
}, "TypeError: Iterator result interface is not an object.");

shouldThrow(function () {
    var [a, b, c] = {
        [Symbol.iterator]() {
            return {};
        }
    };
}, "TypeError: undefined is not a function (near '...[a, b, c]...')");

shouldThrow(function () {
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        next() {
            throw new Error('out');
        }
    };
}, 'Error: out');

shouldThrow(function () {
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        next() {
            return 42;
        }
    };
}, 'TypeError: Iterator result interface is not an object.');

(function () {
    var ok = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            return() {
                ok++;
            },

            next() {
                return 42;
            }
        };
    }, 'TypeError: Iterator result interface is not an object.');

    shouldBe(ok, 0);
}());

(function () {
    var ok = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            return() {
                ok++;
            },

            next() {
                return { value: 20, done: false };
            }
        };
    }, 'TypeError: Iterator result interface is not an object.');

    shouldBe(ok, 1);
}());

(function () {
    var ok = 0;

    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        return() {
            ok++;
        },

        next() {
            return { value: 20, done: true };
        }
    };

    shouldBe(a, undefined);
    shouldBe(b, undefined);
    shouldBe(c, undefined);
    shouldBe(ok, 0);
}());

(function () {
    var ok = 0;
    var n = 0;

    var done = false;
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        return() {
            ok++;
        },

        next() {
            var prev = done;
            done = true;
            ++n;
            return { value: 20, done: prev };
        }
    };

    shouldBe(a, 20);
    shouldBe(b, undefined);
    shouldBe(c, undefined);
    shouldBe(n, 2);
    shouldBe(ok, 0);
}());

(function () {
    var ok = 0;
    var n = 0;

    var done = false;
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        return() {
            ++ok;
            return { done: true };
        },

        next() {
            ++n;
            return { value: 20, done: false };
        }
    };

    shouldBe(a, 20);
    shouldBe(b, 20);
    shouldBe(c, 20);
    shouldBe(n, 3);
    shouldBe(ok, 1);
}());

(function () {
    var ok = 0;
    var n = 0;

    var done = false;
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        return() {
            ++ok;
            return { done: true };
        },

        count: 0,

        next() {
            ++n;
            var done = ++this.count === 3;
            return { value: 20, done };
        }
    };

    shouldBe(a, 20);
    shouldBe(b, 20);
    shouldBe(c, undefined);
    shouldBe(n, 3);
    shouldBe(ok, 0);
}());

(function () {
    var ok = 0;
    var n = 0;

    var done = false;
    var [a, b, c] = {
        [Symbol.iterator]() {
            return this;
        },

        return() {
            ++ok;
            return { done: true };
        },

        count: 0,

        next() {
            ++n;
            var done = ++this.count === 4;
            return { value: 20, done };
        }
    };

    shouldBe(a, 20);
    shouldBe(b, 20);
    shouldBe(c, 20);
    shouldBe(n, 3);
    shouldBe(ok, 1);
}());

(function () {
    var ok = 0;
    var n = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            return() {
                ok++;
                throw new Error('out');
            },

            next() {
                n++;
                return { value: 20, done: false };
            }
        };
    }, 'Error: out');

    shouldBe(n, 3);
    shouldBe(ok, 1);
}());

(function () {
    var ok = 0;
    var n = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            get return() {
                ok++;
                throw new Error('out');
            },

            next() {
                n++;
                return { value: 20, done: false };
            }
        };
    }, 'Error: out');

    shouldBe(n, 3);
    shouldBe(ok, 1);
}());

(function () {
    var ok = 0;
    var n = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            get return() {
                ok++;
                throw new Error('ng');
            },

            next() {
                n++;
                throw new Error('out');
            }
        };
    }, 'Error: out');

    shouldBe(n, 1);
    shouldBe(ok, 0);
}());

(function () {
    var ok = 0;
    var n = 0;
    shouldThrow(function () {
        var [a, b, c] = {
            [Symbol.iterator]() {
                return this;
            },

            get return() {
                ok++;
                throw new Error('ng');
            },

            get next() {
                ++n;
                throw new Error('out');
            }
        };
    }, 'Error: out');

    shouldBe(n, 1);
    shouldBe(ok, 0);
}());
