'use strict';

global.CommonComponentBase = require('../public/shared/common-component-base.js').CommonComponentBase;
global.MarkupPage = require('../tools/js/markup-component.js').MarkupPage;
global.MarkupComponentBase = require('../tools/js/markup-component.js').MarkupComponentBase;

const assert = require('assert');
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const AnalysisResultsNotifier = require('../tools/js/analysis-results-notifier.js').AnalysisResultsNotifier;
const assertThrows = require('../server-tests/resources/common-operations.js').assertThrows;

describe('AnalysisResultsNotifier', () => {
    describe('_matchesRule', () => {

        const trunkMacBook = 'Trunk MacBook';
        const trunkMacBookPro = 'Trunk MacBook Pro';
        const trunkMacBookAir = 'Trunk MacBook Air';
        const speedometer = 'speedometer';
        const speedometer2 = 'speedometer-2';
        const jetStream = 'JetStream';

        it('should return a group of matching function based on configuration', () => {
            const rule = {platforms: [trunkMacBook, trunkMacBookPro], tests: [speedometer, speedometer2]};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, true, rule));

        });

        it('should match rule only contains tests correctly', () => {
            const rule = {tests: [jetStream]};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, jetStream, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, true, rule));
        });

        it('should match rule only contains platforms correctly', () => {
            const rule = {platforms: [trunkMacBook]};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, jetStream, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, true, rule));
        });

        it('should match rule with "userInitiated" set to true', () => {
            const rule = {platforms: [trunkMacBook, trunkMacBookPro], tests: [speedometer, speedometer2], userInitiated: true};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
        });

        it('should match rule with "userInitiated" set to false', () => {
            const rule = {platforms: [trunkMacBook, trunkMacBookPro], tests: [speedometer, speedometer2], userInitiated: false};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));

        });

        it('should match the rule regardless whether or not "userInitiated" is set if rule does not specify', () => {
            const rule = {platforms: [trunkMacBook, trunkMacBookPro], tests: [speedometer, speedometer2]};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
        });

        it('should match rule when platforms, tests, and userInitiated are undefined', () => {
            const rule = {};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
        });

        it('should match rule when platforms and tests are undefined and userInitiated set to true', () => {
            const rule = {userInitiated: true};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
        });

        it('should match rule when platforms and tests are undefined and userInitiated set to false', () => {
            const rule = {userInitiated: false};
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
            assert.ok(AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));

        });

        it('should not match rule when platforms, and tests are empty arrays and userInitiated is undefined', () => {
            const rule = {tests: [], platforms: []};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
        });

        it('should not match rule when platforms and tests are are empty arrays and userInitiated set to true', () => {
            const rule = {tests: [], platforms: [], userInitiated: true};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));
        });

        it('should not match rule when platforms and tests are empty and userInitiated set to false', () => {
            const rule = {tests: [], platforms: [], userInitiated: false};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer, false, rule));

        });

        it('should not match rule when platforms is empty, tests match, userInitiated is undefined', () => {
            const rule = {platforms: [], tests: [speedometer2, jetStream]};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, jetStream, false, rule));
        });

        it('should not match rule when tests is empty, platforms match, userInitiated is undefined', () => {
            const rule = {tests: [], platforms: [trunkMacBook, trunkMacBookAir]};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, false, rule));
        });

        it('should not match rule when platforms is empty, tests match, userInitiated is true', () => {
            const rule = {platforms: [], tests: [speedometer2, jetStream], userInitiated: true};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, jetStream, false, rule));
        });

        it('should not match rule when platforms is empty, tests match, userInitiated is false', () => {
            const rule = {platforms: [], tests: [speedometer2, jetStream], userInitiated: false};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookPro, jetStream, true, rule));
        });

        it('should not match rule when tests is empty, platforms match, userInitiated is true', () => {
            const rule = {tests: [], platforms: [trunkMacBook, trunkMacBookAir], userInitiated: true};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, speedometer, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, false, rule));
        });

        it('should not match rule when tests is empty, platforms match, userInitiated is false', () => {
            const rule = {tests: [], platforms: [trunkMacBook, trunkMacBookAir], userInitiated: false};
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, speedometer2, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, speedometer, false, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBookAir, jetStream, true, rule));
            assert.ok(!AnalysisResultsNotifier._matchesRule(trunkMacBook, jetStream, true, rule));
        });
    });

    describe('_validateRules', () => {
        it('should pass the validation for empty rule', () => {
            assert.doesNotThrow(() => AnalysisResultsNotifier._validateRules([{}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should fail the validation for rule where `tests` is not an array of strings', () => {
            assert.throws(() => AnalysisResultsNotifier._validateRules([{tests: [{}]}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should fail the validation for rule where `platforms` is not an array of strings', () => {
            assert.throws(() => AnalysisResultsNotifier._validateRules([{platforms: [{}]}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should fail the validation for rule where `platforms` and `tests` are not an array of strings', () => {
            assert.throws(() => AnalysisResultsNotifier._validateRules([{platforms: [{}], tests: [9, 1, 1]}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should fail the validation for rule where `platforms` or `tests` is not an array of strings', () => {
            assert.throws(() => AnalysisResultsNotifier._validateRules([{platforms: ['System MacBookAir'], tests: [{}, 0]}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should fail the validation for rule where `platforms` or `tests` is not an array of strings', () => {
            assert.throws(() => AnalysisResultsNotifier._validateRules([{platforms: ['Trunk MacBook', 5], tests: ['speedometer']}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should pass the validation for rule where `tests` is an empty array', () => {
            assert.doesNotThrow(() => AnalysisResultsNotifier._validateRules([{tests: []}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should pass the validation for rule where `platforms` is an empty arrays', () => {
            assert.doesNotThrow(() => AnalysisResultsNotifier._validateRules([{platforms: []}]), /AssertionError \[ERR_ASSERTION\]: Either tests or platforms must be an undefined, empty array, or an array of strings/);
        });

        it('should pass the validation for if a rule only has tests specified', () => {
            AnalysisResultsNotifier._validateRules([{tests: ['speedometer']}]);
        });

        it('should pass the validation for if a rule only has platforms specified', () => {
            AnalysisResultsNotifier._validateRules([{platforms: ['Trunk MacBook']}]);
        });

        it('should pass the validation for if a rule has both tests and platforms specified', () => {
            AnalysisResultsNotifier._validateRules([{tests: ['speedometer'], platforms: ['Trunk MacBook']}]);
        });
    });

    describe('_applyUpdate', () => {
        it('should set value directly if key does not exist in another object', () => {
            const merged = AnalysisResultsNotifier._applyUpdate({a: 1}, {b: 2});
            assert.deepEqual(merged, {a: 1, b: 2});
        });

        it('should concatenate arrays if both are array', () => {
            const merged = AnalysisResultsNotifier._applyUpdate({a: [1]}, {a: [2]});
            assert.deepEqual(merged, {a: [1, 2]});
        });

        it('should add to the array if only one side is array', () => {
            const merged = AnalysisResultsNotifier._applyUpdate({a: [1], b: 1}, {a: 2, b: [2]});
            assert.deepEqual(merged, {a: [1, 2], b: [1, 2]});
        });

        it('should merge recursively', () => {
            const merged = AnalysisResultsNotifier._applyUpdate({a: {b: 1}}, {a: {b: [2]}});
            assert.deepEqual(merged, {a: {b: [1, 2]}});
        });

        it('should merge values into array if one of value with same key is one of follow primitive types: number, string and boolean', () => {
            const merged = AnalysisResultsNotifier._applyUpdate({a: 1, b: "a", c: 1, d: true, e: {}}, {a: 2, b: "b", c: "b", d: 2, e: 1});
            assert.deepEqual(merged, {a: [1, 2], b: ["a", "b"], c: [1, "b"], d: [true, 2], e: [{}, 1]});
        });
    });

    describe('_sendNotification', () => {
        beforeEach(() => {
            MockRemoteAPI.reset('http://send-notification.webkit.org/');
        });

        it('send a notification successfully', async () => {
            const notifier = new AnalysisResultsNotifier(null, null, [{tests: ['a']}], MockRemoteAPI, 'send-notification');
            const promise = notifier._sendNotification('test');
            assert.equal(MockRemoteAPI.requests.length, 1);
            const request = MockRemoteAPI.requests[0];
            assert.equal(request.data, 'test');
            assert.equal(request.url, 'send-notification');
            assert.equal(request.method, 'POST');

            request.resolve('ok');
            const result = await promise;
            assert.equal(result, 'ok');
        });

        it('should fail if notification request gets rejected', async () => {
            const notifier = new AnalysisResultsNotifier(null, null, [{tests: ['a']}], MockRemoteAPI, 'send-notification');
            const promise = notifier._sendNotification('test');
            assert.equal(MockRemoteAPI.requests.length, 1);
            const request = MockRemoteAPI.requests[0];
            assert.equal(request.data, 'test');
            assert.equal(request.url, 'send-notification');
            assert.equal(request.method, 'POST');

            request.reject('unavailable');
            assertThrows('unavailable', () => promise);
        });
    });
});