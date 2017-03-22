'use strict';

class TimeSeries {
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

    viewBetweenPoints(firstPoint, lastPoint)
    {
        console.assert(firstPoint.series == this);
        console.assert(lastPoint.series == this);
        return new TimeSeriesView(this, firstPoint.seriesIndex, lastPoint.seriesIndex + 1);
    }
};

class TimeSeriesView {
    constructor(timeSeries, startingIndex, afterEndingIndex, filteredData = null)
    {
        console.assert(timeSeries instanceof TimeSeries);
        console.assert(startingIndex <= afterEndingIndex);
        console.assert(afterEndingIndex <= timeSeries._data.length);
        this._timeSeries = timeSeries;
        this._data = filteredData || timeSeries._data;
        this._values = null;
        this._length = afterEndingIndex - startingIndex;
        this._startingIndex = startingIndex;
        this._afterEndingIndex = afterEndingIndex;
        this._pointIndexMap = null;

        if (this._data != timeSeries._data) {
            this._findIndexForPoint = (point) => {
                if (this._pointIndexMap == null)
                    this._buildPointIndexMap();
                return this._pointIndexMap.get(point);
            }
        } else
            this._findIndexForPoint = (point) => { return point.seriesIndex; }
    }

    _buildPointIndexMap()
    {
        this._pointIndexMap = new Map;
        const data = this._data;
        const length = data.length;
        for (let i = 0; i < length; i++)
            this._pointIndexMap.set(data[i], i);
    }

    length() { return this._length; }

    firstPoint() { return this._length ? this._data[this._startingIndex] : null; }
    lastPoint() { return this._length ? this._data[this._afterEndingIndex - 1] : null; }

    nextPoint(point)
    {
        let index = this._findIndexForPoint(point);
        index++;
        if (index == this._afterEndingIndex)
            return null;
        return this._data[index];
    }

    previousPoint(point)
    {
        const index = this._findIndexForPoint(point);
        if (index == this._startingIndex)
            return null;
        return this._data[index - 1];
    }

    findPointByIndex(index)
    {
        index += this._startingIndex;
        if (index < 0 || index >= this._afterEndingIndex)
            return null;
        return this._data[index];
    }

    findById(id)
    {
        for (let point of this) {
            if (point.id == id)
                return point;
        }
        return null;
    }

    values()
    {
        if (this._values == null) {
            this._values = new Array(this._length);
            let i = 0;
            for (let point of this)
                this._values[i++] = point.value;
        }
        return this._values;
    }

    filter(callback)
    {
        const filteredData = [];
        let i = 0;
        for (let point of this) {
            if (callback(point, i))
                filteredData.push(point);
            i++;
        }
        return new TimeSeriesView(this._timeSeries, 0, filteredData.length, filteredData);
    }

    viewTimeRange(startTime, endTime)
    {
        const data = this._data;
        let startingIndex = null;
        let endingIndex = null;
        for (let i = this._startingIndex; i < this._afterEndingIndex; i++) {
            if (startingIndex == null && data[i].time >= startTime)
                startingIndex = i;
            if (data[i].time <= endTime)
                endingIndex = i;
        }
        if (startingIndex == null || endingIndex == null)
            return new TimeSeriesView(this._timeSeries, 0, 0, data);
        return new TimeSeriesView(this._timeSeries, startingIndex, endingIndex + 1, data);
    }

    firstPointInTimeRange(startTime, endTime)
    {
        console.assert(startTime <= endTime);
        for (let point of this) {
            if (point.time > endTime)
                return null;
            if (point.time >= startTime)
                return point;
        }
        return null;
    }

    lastPointInTimeRange(startTime, endTime)
    {
        console.assert(startTime <= endTime);
        for (let point of this._reverse()) {
            if (point.time < startTime)
                return null;
            if (point.time <= endTime)
                return point;
        }
        return null;
    }

    [Symbol.iterator]()
    {
        const data = this._data;
        const end = this._afterEndingIndex;
        let i = this._startingIndex;
        return {
            next() {
                return {value: data[i], done: i++ == end};
            }
        };
    }

    _reverse()
    {
        return {
            [Symbol.iterator]: () => {
                const data = this._data;
                const end = this._startingIndex;
                let i = this._afterEndingIndex;
                return {
                    next() {
                        return {done: i-- == end, value: data[i]};
                    }
                };
            }
        }
    }
}

if (typeof module != 'undefined')
    module.exports.TimeSeries = TimeSeries;
