'use strict';

class Metric extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._aggregatorName = object.aggregator;
        object.test.addMetric(this);
        this._test = object.test;
        this._platforms = [];

        const suffix = this.name().match('([A-Z][a-z]+|FrameRate)$')[0];
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
            'Power': 'W',
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

    relativeName(path)
    {
        const relativeTestName = this._test.relativeName(path);
        if (relativeTestName == null)
            return this.label();
        return relativeTestName + ' : ' + this.label();
    }

    aggregatorLabel()
    {
        switch (this._aggregatorName) {
        case 'Arithmetic':
            return 'Arithmetic mean';
        case 'Geometric':
            return 'Geometric mean';
        case 'Harmonic':
            return 'Harmonic mean';
        case 'Total':
            return 'Total';
        }
        return null;
    }

    label()
    {
        const aggregatorLabel = this.aggregatorLabel();
        return this.name() + (aggregatorLabel ? ` : ${aggregatorLabel}` : '');
    }

    unit() { return this._unit; }

    isSmallerBetter()
    {
        const unit = this._unit;
        return unit != 'fps' && unit != '/s' && unit != 'pt';
    }

    labelForDifference(beforeValue, afterValue, progressionTypeName, regressionTypeName)
    {
        const diff = afterValue - beforeValue;
        const changeType = diff < 0 == this.isSmallerBetter() ? progressionTypeName : regressionTypeName;
        const relativeChange = diff / beforeValue * 100;
        const changeLabel = Math.abs(relativeChange).toFixed(2) + '% ' + changeType;
        return {changeType, relativeChange, changeLabel};
    }

    makeFormatter(sigFig, alwaysShowSign) { return Metric.makeFormatter(this.unit(), sigFig, alwaysShowSign); }

    static makeFormatter(unit, sigFig = 2, alwaysShowSign = false)
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

    static formatTime(utcTime)
    {
        // FIXME: This is incorrect when the offset cross day-life-saving change. It's good enough for now.
        const offsetInMinutes = (new Date(utcTime)).getTimezoneOffset();
        const timeInLocalTimeZone = new Date(utcTime - offsetInMinutes * 60 * 1000);
        return timeInLocalTimeZone.toISOString().replace('T', ' ').replace(/\.\d+Z$/, '');
    }
}

if (typeof module != 'undefined')
    module.exports.Metric = Metric;
