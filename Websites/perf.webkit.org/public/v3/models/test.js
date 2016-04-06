'use strict';

class Test extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._url = object.url; // FIXME: Unused
        this._parentId = object.parentId;
        this._childTests = [];
        this._metrics = [];

        if (!this._parentId)
            this.ensureNamedStaticMap('topLevelTests')[id] = this;
        else {
            var childMap = this.ensureNamedStaticMap('childTestMap');
            if (!childMap[this._parentId])
                childMap[this._parentId] = [this];
            else
                childMap[this._parentId].push(this);
        }
    }

    static topLevelTests() { return this.sortByName(this.listForStaticMap('topLevelTests')); }

    static findByPath(path)
    {
        var matchingTest = null;
        var testList = this.topLevelTests();
        for (var part of path) {
            matchingTest = null;
            for (var test of testList) {
                if (part == test.name()) {
                    matchingTest = test;
                    break;
                }
            }
            if (!matchingTest)
                return null;
            testList = matchingTest.childTests();
        }
        return matchingTest;
    }

    parentTest() { return Test.findById(this._parentId); }

    path()
    {
        var path = [];
        var currentTest = this;
        while (currentTest) {
            path.unshift(currentTest);
            currentTest = currentTest.parentTest();
        }
        return path;
    }

    fullName() { return this.path().map(function (test) { return test.label(); }).join(' \u220B '); }

    onlyContainsSingleMetric() { return !this.childTests().length && this._metrics.length == 1; }

    childTests()
    {
        var childMap = this.namedStaticMap('childTestMap');
        return childMap && this.id() in childMap ? childMap[this.id()] : [];
    }

    metrics() { return this._metrics; }

    addChildTest(test) { this._childTests.push(test); }
    addMetric(metric) { this._metrics.push(metric); }
}

if (typeof module != 'undefined')
    module.exports.Test = Test;
