'use strict';

class Test extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._url = object.url; // FIXME: Unused
        this._parentId = object.parentId;
        this._childTests = [];
        this._metrics = [];
        this._hidden = object.hidden;
        this._computePathLazily = new LazilyEvaluatedFunction(this._computePath.bind(this));

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

    path() { return this._computePathLazily.evaluate(); }

    _computePath()
    {
        const path = [];
        let currentTest = this;
        while (currentTest) {
            path.unshift(currentTest);
            currentTest = currentTest.parentTest();
        }
        return path;
    }

    fullName() { return this.path().map((test) => test.label()).join(' \u220B '); }

    relativeName(sharedPath)
    {
        const path = this.path();
        const partialName = (index) => path.slice(index).map((test) => test.label()).join(' \u220B ');
        if (!sharedPath || !sharedPath.length)
            return partialName(0);
        let i = 0;
        for (; i < path.length && i < sharedPath.length; i++) {
            if (sharedPath[i] != path[i])
                return partialName(i);
        }
        if (i < path.length)
            return partialName(i);
        return null;
    }

    onlyContainsSingleMetric() { return !this.childTests().length && this._metrics.length == 1; }

    childTests()
    {
        var childMap = this.namedStaticMap('childTestMap');
        return childMap && this.id() in childMap ? childMap[this.id()] : [];
    }

    metrics() { return this._metrics; }

    addChildTest(test) { this._childTests.push(test); }
    addMetric(metric) { this._metrics.push(metric); }
    isHidden() { return this._hidden; }
}

if (typeof module != 'undefined')
    module.exports.Test = Test;
