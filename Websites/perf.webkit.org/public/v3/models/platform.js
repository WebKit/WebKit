'use strict';

class Platform extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._label = object.label;
        this._metrics = object.metrics;
        this._lastModifiedByMetric = object.lastModifiedByMetric;
        this._isHidden = object.hidden;
        this._containingTests = null;

        this.ensureNamedStaticMap('name')[object.name] = this;

        for (var metric of this._metrics)
            metric.addPlatform(this);

        this._group = object.group;
        if (this._group)
            this._group.addPlatform(this);
    }

    static findByName(name)
    {
        var map = this.namedStaticMap('name');
        return map ? map[name] : null;
    }

    label()
    {
        return this._label || this.name();
    }

    isInSameGroupAs(other)
    {
        if (!this.group() && !other.group())
            return this == other;
        return this.group() == other.group();
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

    isHidden()
    {
        return this._isHidden;
    }

    group() { return this._group; }
}

if (typeof module != 'undefined')
    module.exports.Platform = Platform;
