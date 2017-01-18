'use strict';

// v3 UI still relies on RunsData for associating metrics with units.
// Use declartive syntax once that dependency has been removed.
var TimeSeries = class {
    constructor()
    {
        this._data = [];
    }

    values() { return this._data.map((point) => point.value); }
    length() { return this._data.length; }

    append(item)
    {
        console.assert(item.series === undefined);
        item.series = this;
        item.seriesIndex = this._data.length;
        this._data.push(item);
    }

    extendToFuture()
    {
        if (!this._data.length)
            return;
        var lastPoint = this._data[this._data.length - 1];
        this._data.push({
            series: this,
            seriesIndex: this._data.length,
            time: Date.now() + 365 * 24 * 3600 * 1000,
            value: lastPoint.value,
            interval: lastPoint.interval,
        });
    }

    valuesBetweenRange(startingIndex, endingIndex)
    {
        startingIndex = Math.max(startingIndex, 0);
        endingIndex = Math.min(endingIndex, this._data.length);
        var length = endingIndex - startingIndex;
        var values = new Array(length);
        for (var i = 0; i < length; i++)
            values[i] = this._data[startingIndex + i].value;
        return values;
    }

    firstPoint() { return this._data.length ? this._data[0] : null; }
    lastPoint() { return this._data.length ? this._data[this._data.length - 1] : null; }

    previousPoint(point)
    {
        console.assert(point.series == this);
        if (!point.seriesIndex)
            return null;
        return this._data[point.seriesIndex - 1];
    }

    nextPoint(point)
    {
        console.assert(point.series == this);
        if (point.seriesIndex + 1 >= this._data.length)
            return null;
        return this._data[point.seriesIndex + 1];
    }

    findPointByIndex(index)
    {
        if (!this._data || index < 0 || index >= this._data.length)
            return null;
        return this._data[index];
    }

    findById(id) { return this._data.find(function (point) { return point.id == id }); }

    findPointAfterTime(time) { return this._data.find(function (point) { return point.time >= time; }); }

    dataBetweenPoints(firstPoint, lastPoint)
    {
        console.assert(firstPoint.series == this);
        console.assert(lastPoint.series == this);
        return this._data.slice(firstPoint.seriesIndex, lastPoint.seriesIndex + 1);
    }

};

if (typeof module != 'undefined')
    module.exports.TimeSeries = TimeSeries;
