
class Platform extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._metrics = object.metrics;
        this._lastModifiedByMetric = object.lastModifiedByMetric;
        this._containingTests = null;

        for (var metric of this._metrics)
            metric.addPlatform(this);
    }

    hasTest(test)
    {
        if (!this._containingTests) {
            this._containingTests = {};
            for (var metric of this._metrics) {
                for (var test = metric.test(); test; test = test.parentTest()) {
                    if (test.id() in this._containingTests)
                        break;
                    this._containingTests[test.id()] = true;
                }
            }
        }
        return this._containingTests[test.id()];
    }

    hasMetric(metric) { return !!this.lastModified(metric); }

    lastModified(metric)
    {
        console.assert(metric instanceof Metric);
        return this._lastModifiedByMetric[metric.id()];
    }
}
