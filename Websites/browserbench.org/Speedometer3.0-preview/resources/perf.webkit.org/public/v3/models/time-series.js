'use strict';

class TimeSeries {
    _data = [];

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
        const lastPoint = this._data[this._data.length - 1];
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
        const length = endingIndex - startingIndex;
        const values = new Array(length);
        for (let i = 0; i < length; ++i)
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

    findById(id) { return this._data.find((point) => point.id == id); }

    findPointAfterTime(time) { return this._data.find((point) => point.time >= time); }
    viewBetweenTime(startTime, endTime)
    {
        const startPoint = this.findPointAfterTime(startTime);
        if (!startPoint)
            return null;
        let endPoint = this.findPointAfterTime(endTime);
        if (!endPoint)
            endPoint = this.lastPoint();
        else if (endPoint.time > endTime)
            endPoint = this.previousPoint(endPoint);

        return this.viewBetweenPoints(startPoint, endPoint);
    }

    viewBetweenPoints(firstPoint, lastPoint)
    {
        console.assert(firstPoint.series == this);
        console.assert(lastPoint.series == this);
        return new TimeSeriesView(this, firstPoint.seriesIndex, lastPoint.seriesIndex + 1);
    }
};

class TimeSeriesView {
    _timeSeries;
    _values = null;
    _length;
    _startingIndex;
    _afterEndingIndex;

    constructor(timeSeries, startingIndex, afterEndingIndex)
    {
        console.assert(timeSeries instanceof TimeSeries);
        console.assert(startingIndex <= afterEndingIndex);
        console.assert(afterEndingIndex <= timeSeries._data.length);
        this._timeSeries = timeSeries;
        this._length = afterEndingIndex - startingIndex;
        this._startingIndex = startingIndex;
        this._afterEndingIndex = afterEndingIndex;
    }

    get _data() { return this._timeSeries._data; }

    length() { return this._length; }

    firstPoint() { return this._length ? this._data[this._startingIndex] : null; }
    lastPoint() { return this._length ? this._data[this._afterEndingIndex - 1] : null; }

    _findIndexForPoint(point)
    {
        return point.seriesIndex;
    }

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
        for (let i = this._startingIndex; i < this._afterEndingIndex; ++i) {
            let point = this._data[i];
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
            for (let index = this._startingIndex; index < this._afterEndingIndex; ++index) {
                let point = this._data[index];
                this._values[i++] = point.value;
            }
        }
        return this._values;
    }

    filter(callback)
    {
        const filteredData = [];
        let i = 0;
        for (let index = this._startingIndex; index < this._afterEndingIndex; ++index) {
            let point = this._data[index];
            if (callback(point, i))
                filteredData.push(point);
            i++;
        }
        return new FilteredTimeSeriesView(this._timeSeries, 0, filteredData.length, filteredData);
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
            return this._subRange(0, 0);
        return this._subRange(startingIndex, endingIndex + 1);
    }

    _subRange(startingIndex, afterEndingIndex)
    {
        return new TimeSeriesView(this._timeSeries, startingIndex, afterEndingIndex);
    }

    firstPointInTimeRange(startTime, endTime)
    {
        console.assert(startTime <= endTime);
        for (let index = this._startingIndex; index < this._afterEndingIndex; ++index) {
            let point = this._data[index];
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
        for (let index = this._afterEndingIndex - 1; index >= this._startingIndex; --index) {
            let point = this._data[index];
            if (point.time < startTime)
                return null;
            if (point.time <= endTime)
                return point;
        }
        return null;
    }
}

class FilteredTimeSeriesView extends TimeSeriesView {
    _filteredData;
    _pointIndexMap;

    constructor(timeSeries, startingIndex, afterEndingIndex, filteredData)
    {
        console.assert(afterEndingIndex <= filteredData.length);
        super(timeSeries, startingIndex, afterEndingIndex);
        this._filteredData = filteredData;
    }

    get _data() { return this._filteredData; }

    _subRange(startingIndex, afterEndingIndex)
    {
        return new FilteredTimeSeriesView(this._timeSeries, startingIndex, afterEndingIndex, this._filteredData);
    }

    _findIndexForPoint(point)
    {
        if (!this._pointIndexMap)
            this._buildPointIndexMap();
        return this._pointIndexMap.get(point);
    }

    _buildPointIndexMap()
    {
        this._pointIndexMap = new Map;
        const data = this._data;
        const length = data.length;
        for (let i = 0; i < length; i++)
            this._pointIndexMap.set(data[i], i);
    }
}

if (typeof module != 'undefined')
    module.exports.TimeSeries = TimeSeries;
