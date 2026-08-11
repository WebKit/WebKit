"use strict";

var assert = require("assert");
var testFulfilled = require("./helpers/testThreeCases").testFulfilled;

var adapter = global.adapter;
var deferred = adapter.deferred;

var dummy = { dummy: "dummy" }; // we fulfill or reject with this when we don't intend to test against it

describe("2.1.2.1: When fulfilled, a promise: must not transition to any other state.", function () {
    testFulfilled(dummy, function (promise, done) {
        var onFulfilledCalled = false;

        promise.then(function onFulfilled() {
            onFulfilledCalled = true;
        }, function onRejected() {
            assert.strictEqual(onFulfilledCalled, false);
            done();
        });

        setTimeout(done, 100);
    });

    specify("trying to fulfill then immediately reject", function (done) {
        var d = deferred();
        var onFulfilledCalled = false;

        d.promise.then(function onFulfilled() {
            onFulfilledCalled = true;
        }, function onRejected() {
            assert.strictEqual(onFulfilledCalled, false);
            done();
        });

        d.resolve(dummy);
        d.reject(dummy);
        setTimeout(done, 100);
    });

    specify("trying to fulfill then reject, delayed", function (done) {
        var d = deferred();
        var onFulfilledCalled = false;

        d.promise.then(function onFulfilled() {
            onFulfilledCalled = true;
        }, function onRejected() {
            assert.strictEqual(onFulfilledCalled, false);
            done();
        });

        setTimeout(function () {
            d.resolve(dummy);
            d.reject(dummy);
        }, 50);
        setTimeout(done, 100);
    });

    specify("trying to fulfill immediately then reject delayed", function (done) {
        var d = deferred();
        var onFulfilledCalled = false;

        d.promise.then(function onFulfilled() {
            onFulfilledCalled = true;
        }, function onRejected() {
            assert.strictEqual(onFulfilledCalled, false);
            done();
        });

        d.resolve(dummy);
        setTimeout(function () {
            d.reject(dummy);
        }, 50);
        setTimeout(done, 100);
    });
});
