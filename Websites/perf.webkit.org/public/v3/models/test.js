'use strict';

class Test extends LabeledObject {
    constructor(id, object, isTopLevel)
    {
        super(id, object);
        this._url = object.url; // FIXME: Unused
        this._parent = null;
        this._childTests = [];
        this._metrics = [];

        if (isTopLevel)
            this.ensureNamedStaticMap('topLevelTests').push(this);
    }

    static topLevelTests() { return this.sortByName(this.namedStaticMap('topLevelTests')); }

    parentTest() { return this._parent; }

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

    onlyContainsSingleMetric() { return !this._childTests.length && this._metrics.length == 1; }

    // FIXME: We should store the child test order in the server.
    childTests() { return this._childTests; }
    metrics() { return this._metrics; }

    setParentTest(parent)
    {
        parent.addChildTest(this);
        this._parent = parent;
    }

    addChildTest(test) { this._childTests.push(test); }
    addMetric(metric) { this._metrics.push(metric); }
}

if (typeof module != 'undefined')
    module.exports.Test = Test;
