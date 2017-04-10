'use strict';

class Metric extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._aggregatorName = object.aggregator;
        object.test.addMetric(this);
        this._test = object.test;
        this._platforms = [];

        const suffix = this.name().match('([A-z][a-z]+|FrameRate)$')[0];
        this._unit = {
            'FrameRate': 'fps',
            'Runs': '/s',
            'Time': 'ms',
            'Duration': 'ms',
            'Malloc': 'B',
            'Heap': 'B',
            'Allocations': 'B',
            'Size': 'B',
            'Score': 'pt',
        }[suffix];
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

    unit() { return this._unit; }

    isSmallerBetter()
    {
        const unit = this._unit;
        return unit != 'fps' && unit != '/s' && unit != 'pt';
    }

    makeFormatter(sigFig, alwaysShowSign) { return Metric.makeFormatter(this.unit(), sigFig, alwaysShowSign); }

    static makeFormatter(unit, sigFig = 2, alwaysShowSign)
    {
        let isMiliseconds = false;
        if (unit == 'ms') {
            isMiliseconds = true;
            unit = 's';
        }

        if (!unit)
            return function (value) { return value.toFixed(2) + ' ' + (unit || ''); }

        var divisor = unit == 'B' ? 1024 : 1000;
        var suffix = ['\u03BC', 'm', '', 'K', 'M', 'G', 'T', 'P', 'E'];
        var threshold = sigFig >= 3 ? divisor : (divisor / 10);
        let formatter = function (value, maxAbsValue = 0) {
            var i;
            var sign = value >= 0 ? (alwaysShowSign ? '+' : '') : '-';
            value = Math.abs(value);
            let sigFigForValue = sigFig;

            // The number of sig-figs to reduce in order to match that of maxAbsValue
            // e.g. 0.5 instead of 0.50 when maxAbsValue is 2.
            let adjustment = 0;
            if (maxAbsValue && value)
                adjustment = Math.max(0, Math.floor(Math.log(maxAbsValue) / Math.log(10)) - Math.floor(Math.log(value) / Math.log(10)));

            for (i = isMiliseconds ? 1 : 2; value && value < 1 && i > 0; i--)
                value *= divisor;
            for (; value >= threshold; i++)
                value /= divisor;

            if (adjustment) // Make the adjustment only for decimal positions below 1.
                adjustment = Math.min(adjustment, Math.max(0, -Math.floor(Math.log(value) / Math.log(10))));

            return sign + value.toPrecision(sigFig - adjustment) + ' ' + suffix[i] + (unit || '');
        }
        formatter.divisor = divisor;
        return formatter;
    };
}

if (typeof module != 'undefined')
    module.exports.Metric = Metric;
