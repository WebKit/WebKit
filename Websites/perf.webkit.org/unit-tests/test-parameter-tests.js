'use strict';

const assert = require('assert');
const MockModels = require('./resources/mock-v3-models.js').MockModels;
require('../tools/js/v3-models.js');


function mockSimpleTestParameterSet(id=1, sdk='13.4', diagnose= true)
{
    return TestParameterSet.ensureSingleton(id, {
        testParameterItems: [
            {parameter: MockModels.macOSSDKParameter, value: sdk},
            {parameter: MockModels.diagnoseParameter, value: diagnose}
        ]
    });
}

function mockEmptyTestParameterSet(id=1)
{
    return TestParameterSet.ensureSingleton(id, {
        testParameterItems: []
    });
}

describe('TestParameterSet', function () {
    MockModels.inject();
    beforeEach(() => {
        TestParameter.clearStaticMap();
        TestParameterSet.clearStaticMap();
    });

    describe('equals', () => {
        it('return true when two parameter sets are identical', () => {
            const a = mockSimpleTestParameterSet(1);
            const b = mockSimpleTestParameterSet(2);
            assert.ok(a.equals(b));
            assert.ok(TestParameterSet.equals(a, b));
        });

        it('should return false when two parameter sets are different', () => {
            const a = mockSimpleTestParameterSet(1);
            const b = mockSimpleTestParameterSet(2, '13.5');
            assert.ok(!a.equals(b));
            assert.ok(!TestParameterSet.equals(a, b));
        })

        it('should return true when one parameter set has no parameter and the other is null or undefined', () => {
            const a = mockEmptyTestParameterSet();
            assert.ok(a.equals(null));
            assert.ok(a.equals(undefined));
            assert.ok(TestParameterSet.equals(a, null));
            assert.ok(TestParameterSet.equals(a, undefined));
            assert.ok(TestParameterSet.equals(null, a));
            assert.ok(TestParameterSet.equals(undefined, a));
        })
    });

    describe('isValueEqual', () => {
        it('should be able to compare nested values', () => {
            assert.ok(TestParameterSet.isValueEqual(
                {value: {key1: [1, '2', 'three'], key2: {string: '1', value: null}}},
                {value: {key1: [1, '2', 'three'], key2: {string: '1', value: null}}}));

            assert.ok(!TestParameterSet.isValueEqual(
                {value: {key1: [1, '2', 'three'], key2: {string: '1'}}},
                {value: {key1: [1, '2', 'three'], key2: {string: '1', value: null}}}));

            assert.ok(!TestParameterSet.isValueEqual(
                {value: {key1: [1, '2', 'three'], key2: {string: '1', value: null}}},
                {value: {key1: [1, '2', 'three'], key2: {string: '1'}}}));
        });
    });

    describe('hasSameBuildParameters', () => {
        it('return true when build parameters from two parameter sets are identical', () => {
            const a = mockSimpleTestParameterSet(1, '13.4', true);
            const b = mockSimpleTestParameterSet(2, '13.4', false);
            assert.ok(!a.equals(b));
            assert.ok(!TestParameterSet.equals(a, b));
            assert.ok(TestParameterSet.hasSameBuildParameters(a, b));
        });

        it('return false when build parameters from two parameter sets are different', () => {
            const a = mockSimpleTestParameterSet(1, '13.4', true);
            const b = mockSimpleTestParameterSet(2, '13.5', true);
            assert.ok(!a.equals(b));
            assert.ok(!TestParameterSet.equals(a, b));
            assert.ok(!TestParameterSet.hasSameBuildParameters(a, b));
        });

        it('should return true if one does not have build type parameter, the other side is null / undefined', () => {
            const parameterSet = mockEmptyTestParameterSet();
            assert.ok(TestParameterSet.hasSameBuildParameters(parameterSet, null));
            assert.ok(TestParameterSet.hasSameBuildParameters(parameterSet, undefined));
            assert.ok(TestParameterSet.hasSameBuildParameters(null, parameterSet));
            assert.ok(TestParameterSet.hasSameBuildParameters(undefined, parameterSet));
        });

        it('should return true if both side are null / undefined', () => {
            assert.ok(TestParameterSet.hasSameBuildParameters(undefined, undefined));
            assert.ok(TestParameterSet.hasSameBuildParameters(undefined, null));
            assert.ok(TestParameterSet.hasSameBuildParameters(null, undefined));
            assert.ok(TestParameterSet.hasSameBuildParameters(null, null));
        });
    });
})

describe('CustomTestParameterSet', function() {
    MockModels.inject();
    beforeEach(() => {
        TestParameter.clearStaticMap();
    });

    describe('set', () => {
        it('should omit field that is not in parameter definition', () => {
            const parameterSet = new CustomTestParameterSet;
            parameterSet.set(MockModels.macOSSDKParameter, {value: '13.4', file: 'something'});
            const entry = parameterSet.get(MockModels.macOSSDKParameter);
            assert.deepEqual(entry, {value: '13.4'});
        });
    });

    describe('delete', () => {
        it('should delete parameter', () => {
            const parameterSet = new CustomTestParameterSet;
            parameterSet.set(MockModels.macOSSDKParameter, {value: '13.4'});
            parameterSet.set(MockModels.diagnoseParameter, {value: true});
            assert.deepEqual(parameterSet.testTypeParameters, [MockModels.diagnoseParameter]);
            assert.deepEqual(parameterSet.buildTypeParameters, [MockModels.macOSSDKParameter]);

            parameterSet.delete(MockModels.macOSSDKParameter);
            assert.deepEqual(parameterSet.testTypeParameters, [MockModels.diagnoseParameter]);
            assert.deepEqual(parameterSet.buildTypeParameters, []);

            parameterSet.delete(MockModels.diagnoseParameter);
            assert.deepEqual(parameterSet.testTypeParameters, []);
            assert.deepEqual(parameterSet.buildTypeParameters, []);

        });

        it('should be able to handle deleting parameter that does not exist', () => {
            const parameterSet = new CustomTestParameterSet;
            assert.deepEqual(parameterSet.testTypeParameters, []);
            assert.deepEqual(parameterSet.buildTypeParameters, []);

            parameterSet.delete(MockModels.diagnoseParameter);
            assert.deepEqual(parameterSet.testTypeParameters, []);
            assert.deepEqual(parameterSet.buildTypeParameters, []);
        });
    });
});