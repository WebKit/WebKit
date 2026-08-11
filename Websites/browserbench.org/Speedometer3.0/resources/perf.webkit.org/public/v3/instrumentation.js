'use strict';

class Instrumentation {

    static startMeasuringTime(domain, label)
    {
        label = domain + ':' + label;
        if (!Instrumentation._statistics) {
            Instrumentation._statistics = {};
            Instrumentation._currentMeasurement = {};
        }
        Instrumentation._currentMeasurement[label] = Date.now();
    }

    static endMeasuringTime(domain, label)
    {
        var time = Date.now() - Instrumentation._currentMeasurement[domain + ':' + label];
        this.reportMeasurement(domain, label, 'ms', time);
    }

    static reportMeasurement(domain, label, unit, value)
    {
        label = domain + ':' + label;
        var statistics = Instrumentation._statistics;
        if (label in statistics) {
            statistics[label].value += value;
            statistics[label].count++;
            statistics[label].min = Math.min(statistics[label].min, value);
            statistics[label].max = Math.max(statistics[label].max, value);
            statistics[label].mean = statistics[label].value / statistics[label].count;
        } else
            statistics[label] = {value: value, unit: unit, count: 1, mean: value, min:value, max: value};
    }

    static dumpStatistics()
    {
        if (!this._statistics)
            return;
        var maxKeyLength = 0;
        for (let key in this._statistics)
            maxKeyLength = Math.max(key.length, maxKeyLength);

        for (let key of Object.keys(this._statistics).sort()) {
            const item = this._statistics[key];
            const keySuffix = ' '.repeat(maxKeyLength - key.length);
            let log = `${key}${keySuffix}: `;
            log += ` mean = ${item.mean.toFixed(2)} ${item.unit}`;
            if (item.unit == 'ms')
                log += ` total = ${item.value.toFixed(2)} ${item.unit}`;
            log += ` min = ${item.min.toFixed(2)} ${item.unit}`;
            log += ` max = ${item.max.toFixed(2)} ${item.unit}`;
            log += ` (${item.count} calls)`;
            console.log(log);
        }
    }

}

if (typeof module != 'undefined')
    module.exports.Instrumentation = Instrumentation;
