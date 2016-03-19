
// v3 UI still relies on RunsData for associating metrics with units.
// Use declartive syntax once that dependency has been removed.
TimeSeries = class {
    constructor()
    {
        this._data = [];
    }

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
        var data = this._data;
        var filteredData = [];
        for (var i = firstPoint.seriesIndex; i <= lastPoint.seriesIndex; i++) {
            if (!data[i].markedOutlier)
                filteredData.push(data[i]);
        }
        return filteredData;
    }

};
