'use strict';

class Metric extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._aggregatorName = object.aggregator;
        object.test.addMetric(this);
        this._test = object.test;
        this._platforms = [];
    }

    aggregatorName() { return this._aggregatorName; }

    test() { return this._test; }
    platforms() { return this._platforms; }

    addPlatform(platform)
    {
        console.assert(platform instanceof Platform);
        this._platforms.push(platform);
    }

    childMetrics()
    {
        var metrics = [];
        for (var childTest of this._test.childTests()) {
            for (var childMetric of childTest.metrics())
                metrics.push(childMetric);
        }
        return metrics;
    }

    path() { return this._test.path().concat([this]); }

    fullName() { return this._test.fullName() + ' : ' + this.label(); }

    label()
    {
        var suffix = '';
        switch (this._aggregatorName) {
        case null:
            break;
        case 'Arithmetic':
            suffix = ' : Arithmetic mean';
            break;
        case 'Geometric':
            suffix = ' : Geometric mean';
            break;
        case 'Harmonic':
            suffix = ' : Harmonic mean';
            break;
        case 'Total':
        default:
            suffix = ' : ' + this._aggregatorName;
        }
        return this.name() + suffix;
    }

    unit() { return RunsData.unitFromMetricName(this.name()); }
    isSmallerBetter() { return RunsData.isSmallerBetter(this.unit()); }

    makeFormatter(sigFig, alwaysShowSign)
    {
        var unit = this.unit();
        var isMiliseconds = false;
        if (unit == 'ms') {
            isMiliseconds = true;
            unit = 's';
        }
        var divisor = unit == 'B' ? 1024 : 1000;

        var suffix = ['\u03BC', 'm', '', 'K', 'M', 'G', 'T', 'P', 'E'];
        var threshold = sigFig >= 3 ? divisor : (divisor / 10);
        return function (value) {
            var i;
            var sign = value >= 0 ? (alwaysShowSign ? '+' : '') : '-';
            value = Math.abs(value);
            for (i = isMiliseconds ? 1 : 2; value < 1 && i > 0; i--)
                value *= divisor;
            for (; value >= threshold; i++)
                value /= divisor;
            return sign + value.toPrecision(Math.max(2, sigFig)) + ' ' + suffix[i] + (unit || '');
        }
    };
}

if (typeof module != 'undefined')
    module.exports.Metric = Metric;
