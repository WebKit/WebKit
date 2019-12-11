'use strict';

class Platform extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._metrics = object.metrics;
        this._lastModifiedByMetric = object.lastModifiedByMetric;
        this._containingTests = null;

        this.ensureNamedStaticMap('name')[object.name] = this;

        for (var metric of this._metrics)
            metric.addPlatform(this);
    }

    static findByName(name)
    {
        var map = this.namedStaticMap('name');
        return map ? map[name] : null;
    }

    hasTest(test)
    {
        if (!this._containingTests) {
            this._containingTests = {};
            for (var metric of this._metrics) {
                for (var currentTest = metric.test(); currentTest; currentTest = currentTest.parentTest()) {
                    if (currentTest.id() in this._containingTests)
                        break;
                    this._containingTests[currentTest.id()] = true;
                }
            }
        }
        return test.id() in this._containingTests;
    }

    hasMetric(metric) { return !!this.lastModified(metric); }

    lastModified(metric)
    {
        console.assert(metric instanceof Metric);
        return this._lastModifiedByMetric[metric.id()];
    }
}

if (typeof module != 'undefined')
    module.exports.Platform = Platform;
