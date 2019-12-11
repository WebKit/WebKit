'use strict';

const assert = require('assert');
require('../tools/js/v3-models.js');

describe('Test', function () {
    beforeEach(() => {
        Test.clearStaticMap();
    });

    describe('topLevelTests', () => {
        it('should contain the tests without a parent test', () => {
            assert.deepEqual(Test.topLevelTests(), []);
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            assert.deepEqual(Test.topLevelTests(), [someTest]);
        });

        it('should not contain the tests with a parent test', () => {
            assert.deepEqual(Test.topLevelTests(), []);
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            assert.equal(childTest.parentTest(), someTest);
            assert.deepEqual(Test.topLevelTests(), [someTest]);
        });
    });

    describe('childTests', () => {
        it('must return the list of the child tests', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            const otherChildTest = new Test(3, {id: 3, name: 'other child test', parentId: 1});
            assert.deepEqual(someTest.childTests(), [childTest, otherChildTest]);
        });

        it('must not return a list that contains a grand child test', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            const grandChildTest = new Test(3, {id: 3, name: 'grand child test', parentId: 2});
            assert.deepEqual(someTest.childTests(), [childTest]);
            assert.deepEqual(childTest.childTests(), [grandChildTest]);
        });
    });

    describe('parentTest', () => {
        it('must return null for a test without a parent test', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            assert.equal(someTest.parentTest(), null);
        });

        it('must return the parent test', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            const grandChildTest = new Test(3, {id: 3, name: 'grand child test', parentId: 2});
            assert.equal(childTest.parentTest(), someTest);
            assert.equal(grandChildTest.parentTest(), childTest);
        });
    });

    describe('path', () => {
        it('must return an array containing itself for a test without a parent', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            assert.deepEqual(someTest.path(), [someTest]);
        });

        it('must return an array containing every ancestor and itself for a test with a parent', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            const grandChildTest = new Test(3, {id: 3, name: 'grand child test', parentId: 2});
            assert.deepEqual(childTest.path(), [someTest, childTest]);
            assert.deepEqual(grandChildTest.path(), [someTest, childTest, grandChildTest]);
        });
    });

    describe('findByPath', () => {
        it('must return null when there are no tests', () => {
            assert.equal(Test.findByPath(['some test']), null);
        });

        it('must be able to find top-level tests', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const otherTest = new Test(2, {id: 2, name: 'other test', parentId: null});
            assert.equal(Test.findByPath(['some test']), someTest);
        });

        it('must be able to find second-level tests', () => {
            const parent = new Test(1, {id: 1, name: 'parent', parentId: null});
            const someChild = new Test(2, {id: 2, name: 'some', parentId: 1});
            const otherChild = new Test(3, {id: 3, name: 'other', parentId: 1});
            assert.equal(Test.findByPath(['some']), null);
            assert.equal(Test.findByPath(['other']), null);
            assert.equal(Test.findByPath(['parent', 'some']), someChild);
            assert.equal(Test.findByPath(['parent', 'other']), otherChild);
        });

        it('must be able to find third-level tests', () => {
            const parent = new Test(1, {id: 1, name: 'parent', parentId: null});
            const child = new Test(2, {id: 2, name: 'child', parentId: 1});
            const grandChild = new Test(3, {id: 3, name: 'grandChild', parentId: 2});
            assert.equal(Test.findByPath(['child']), null);
            assert.equal(Test.findByPath(['grandChild']), null);
            assert.equal(Test.findByPath(['child', 'grandChild']), null);
            assert.equal(Test.findByPath(['parent', 'grandChild']), null);
            assert.equal(Test.findByPath(['parent', 'child']), child);
            assert.equal(Test.findByPath(['parent', 'child', 'grandChild']), grandChild);
        });
    });

    describe('fullName', () => {
        it('must return the name of a top-level test', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            assert.equal(someTest.fullName(), 'some test');
        });

        it('must return the name of a second-level test and the name of its parent concatenated with \u220B', () => {
            const parent = new Test(1, {id: 1, name: 'parent', parentId: null});
            const child = new Test(2, {id: 2, name: 'child', parentId: 1});
            assert.equal(child.fullName(), 'parent \u220B child');
        });

        it('must return the name of a third-level test concatenated with the names of its ancestor tests with \u220B', () => {
            const parent = new Test(1, {id: 1, name: 'parent', parentId: null});
            const child = new Test(2, {id: 2, name: 'child', parentId: 1});
            const grandChild = new Test(3, {id: 3, name: 'grandChild', parentId: 2});
            assert.equal(grandChild.fullName(), 'parent \u220B child \u220B grandChild');
        });
    });

    describe('relativeName', () => {
        it('must return the full name of a test when the shared path is null', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            assert.equal(someTest.relativeName(null), someTest.fullName());
            assert.equal(childTest.relativeName(null), childTest.fullName());
        });

        it('must return the full name of a test when the shared path is empty', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            assert.equal(someTest.relativeName([]), someTest.fullName());
            assert.equal(childTest.relativeName([]), childTest.fullName());
        });

        it('must return null when the shared path is identical to the path of the test', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(2, {id: 2, name: 'child test', parentId: 1});
            assert.equal(someTest.relativeName(someTest.path()), null);
            assert.equal(childTest.relativeName(childTest.path()), null);
        });

        it('must return the full name of a test when the first part in the path differs', () => {
            const someTest = new Test(1, {id: 1, name: 'some test', parentId: null});
            const otherTest = new Test(2, {id: 1, name: 'some test', parentId: null});
            const childTest = new Test(3, {id: 3, name: 'child test', parentId: 1});
            assert.equal(someTest.relativeName([otherTest]), someTest.fullName());
            assert.equal(childTest.relativeName([otherTest]), childTest.fullName());
        });

        it('must return the name relative to its parent when the shared path is of the parent', () => {
            const parent = new Test(1, {id: 1, name: 'parent', parentId: null});
            const child = new Test(2, {id: 2, name: 'child', parentId: 1});
            assert.equal(child.relativeName([parent]), 'child');
        });

        it('must return the name relative to its grand parent when the shared path is of the grand parent', () => {
            const grandParent = new Test(1, {id: 1, name: 'grandParent', parentId: null});
            const parent = new Test(2, {id: 2, name: 'parent', parentId: 1});
            const self = new Test(3, {id: 3, name: 'self', parentId: 2});
            assert.equal(self.relativeName([grandParent]), 'parent \u220B self');
        });

        it('must return the name relative to its parent when the shared path is of the parent even if it had a grandparent', () => {
            const grandParent = new Test(1, {id: 1, name: 'grandParent', parentId: null});
            const parent = new Test(2, {id: 2, name: 'parent', parentId: 1});
            const self = new Test(3, {id: 3, name: 'self', parentId: 2});
            assert.equal(self.relativeName([grandParent, parent]), 'self');
        });
    });

});