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
