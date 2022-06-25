
const assert = require('assert');
const LazilyEvaluatedFunction = require('../public/v3/lazily-evaluated-function.js').LazilyEvaluatedFunction;

describe('LazilyEvaluatedFunction', () => {

    describe('evaluate', () => {
        it('should invoke the callback on the very first call with no arguments', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate();
            assert.deepEqual(calls, [[]]);
        });

        it('should retrun the cached results without invoking the callback on the second call with no arguments', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate();
            assert.deepEqual(calls, [[]]);
            lazyFunction.evaluate();
            assert.deepEqual(calls, [[]]);
        });

        it('should invoke the callback when calld with an argument after being called with no argument', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate();
            assert.deepEqual(calls, [[]]);
            lazyFunction.evaluate(1);
            assert.deepEqual(calls, [[], [1]]);
        });

        it('should invoke the callback when calld with no arguments after being called with an argument', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate('foo');
            assert.deepEqual(calls, [['foo']]);
            lazyFunction.evaluate();
            assert.deepEqual(calls, [['foo'], []]);
        });

        it('should invoke the callback when calld with null after being called with undefined', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(undefined);
            assert.deepEqual(calls, [[undefined]]);
            lazyFunction.evaluate(null);
            assert.deepEqual(calls, [[undefined], [null]]);
        });

        it('should invoke the callback when calld with 0 after being called with "0"', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(0);
            assert.deepEqual(calls, [[0]]);
            lazyFunction.evaluate("0");
            assert.deepEqual(calls, [[0], ["0"]]);
        });

        it('should invoke the callback when calld with an object after being called with another object with the same set of properties', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            const x = {};
            const y = {};
            lazyFunction.evaluate(x);
            assert.deepEqual(calls, [[x]]);
            lazyFunction.evaluate(y);
            assert.deepEqual(calls, [[x], [y]]);
        });

        it('should return the cached result without invoking the callback when calld with a string after being called with the same string', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate("foo");
            assert.deepEqual(calls, [["foo"]]);
            lazyFunction.evaluate("foo");
            assert.deepEqual(calls, [["foo"]]);
        });

        it('should invoke the callback when calld with a string after being called with another string', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate("foo");
            assert.deepEqual(calls, [["foo"]]);
            lazyFunction.evaluate("bar");
            assert.deepEqual(calls, [["foo"], ["bar"]]);
        });

        it('should return the cached result without invoking the callback when calld with a number after being called with the same number', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(8);
            assert.deepEqual(calls, [[8]]);
            lazyFunction.evaluate(8);
            assert.deepEqual(calls, [[8]]);
        });

        it('should invoke the callback when calld with a number after being called with another number', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(4);
            assert.deepEqual(calls, [[4]]);
            lazyFunction.evaluate(2);
            assert.deepEqual(calls, [[4], [2]]);
        });

        it('should return the cached result without invoking the callback when calld with ["hello", 3, "world"] for the second time', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate("hello", 3, "world");
            assert.deepEqual(calls, [["hello", 3, "world"]]);
            lazyFunction.evaluate("hello", 3, "world");
            assert.deepEqual(calls, [["hello", 3, "world"]]);
        });

        it('should invoke the callback when calld with ["hello", 3, "world"] after being called with ["hello", 4, "world"]', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate("hello", 3, "world");
            assert.deepEqual(calls, [["hello", 3, "world"]]);
            lazyFunction.evaluate("hello", 4, "world");
            assert.deepEqual(calls, [["hello", 3, "world"], ["hello", 4, "world"]]);
        });

        it('should return the cached result without invoking the callback when called with [null, null] for the second time', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(null, null);
            assert.deepEqual(calls, [[null, null]]);
            lazyFunction.evaluate(null, null);
            assert.deepEqual(calls, [[null, null]]);
        });

        it('should invoke the callback when calld with [null] after being called with [null, null]', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(null, null);
            assert.deepEqual(calls, [[null, null]]);
            lazyFunction.evaluate(null);
            assert.deepEqual(calls, [[null, null], [null]]);
        });

        it('should invoke the callback when calld with [null, 4] after being called with [null]', () => {
            const calls = [];
            const lazyFunction = new LazilyEvaluatedFunction((...args) => calls.push(args));

            assert.deepEqual(calls, []);
            lazyFunction.evaluate(null, 4);
            assert.deepEqual(calls, [[null, 4]]);
            lazyFunction.evaluate(null);
            assert.deepEqual(calls, [[null, 4], [null]]);
        });

    });

});

