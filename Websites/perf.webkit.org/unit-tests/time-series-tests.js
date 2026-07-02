'use strict';

const assert = require('assert');
if (!assert.almostEqual)
    assert.almostEqual = require('./resources/almost-equal.js');

const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
require('../tools/js/v3-models.js');

let threePoints;
let fivePoints;
beforeEach(() => {
    threePoints = [
        {id: 910, time: 101, value: 110},
        {id: 902, time: 220, value: 102},
        {id: 930, time: 303, value: 130},
    ];
    fivePoints = [...threePoints,
        {id: 904, time: 400, value: 114},
        {id: 950, time: 505, value: 105},
        {id: 960, time: 600, value: 116},
    ];
});

function addPointsToSeries(timeSeries, list = threePoints)
{
    for (let point of list)
        timeSeries.append(point);
}

describe('TimeSeries', () => {

    describe('length', () => {
        it('should return the length', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.length(), 3);
        });

        it('should return 0 when there are no points', () => {
            const timeSeries = new TimeSeries();
            assert.equal(timeSeries.length(), 0);
        });
    });

    describe('firstPoint', () => {
        it('should return the first point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.firstPoint(), threePoints[0]);
        });

        it('should return null when there are no points', () => {
            const timeSeries = new TimeSeries();
            assert.equal(timeSeries.firstPoint(), null);
        });
    });

    describe('lastPoint', () => {
        it('should return the first point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.lastPoint(), threePoints[2]);
        });

        it('should return null when there are no points', () => {
            const timeSeries = new TimeSeries();
            assert.equal(timeSeries.lastPoint(), null);
        });
    });

    describe('nextPoint', () => {
        it('should return the next point when called on the first point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.nextPoint(threePoints[0]), threePoints[1]);
        });

        it('should return the next point when called on a mid-point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.nextPoint(threePoints[1]), threePoints[2]);
        });

        it('should return null when called on the last point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.nextPoint(threePoints[2]), null);
        });
    });

    describe('previousPoint', () => {
        it('should return null when called on the first point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.previousPoint(threePoints[0]), null);
        });

        it('should return the previous point when called on a mid-point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.previousPoint(threePoints[1]), threePoints[0]);
        });

        it('should return the previous point when called on the last point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.previousPoint(threePoints[2]), threePoints[1]);
        });
    });

    describe('findPointByIndex', () => {
        it('should return null the index is less than 0', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointByIndex(-10), null);
            assert.equal(timeSeries.findPointByIndex(-1), null);
        });

        it('should return null when the index is greater than or equal to the length', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointByIndex(10), null);
            assert.equal(timeSeries.findPointByIndex(3), null);
        });

        it('should return null when the index is not a number', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointByIndex(undefined), null);
            assert.equal(timeSeries.findPointByIndex(NaN), null);
            assert.equal(timeSeries.findPointByIndex('a'), null);
        });

        it('should return the point at the specified index when it is in the valid range', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointByIndex(0), threePoints[0]);
            assert.equal(timeSeries.findPointByIndex(1), threePoints[1]);
            assert.equal(timeSeries.findPointByIndex(2), threePoints[2]);
        });
    });

    describe('findById', () => {
        it('should return the point with the specified ID', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findById(threePoints[0].id), threePoints[0]);
            assert.equal(timeSeries.findById(threePoints[1].id), threePoints[1]);
            assert.equal(timeSeries.findById(threePoints[2].id), threePoints[2]);
        });

        it('should return null for a non-existent ID', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findById(null), null);
            assert.equal(timeSeries.findById(undefined), null);
            assert.equal(timeSeries.findById(NaN), null);
            assert.equal(timeSeries.findById('a'), null);
            assert.equal(timeSeries.findById(4231563246), null);
        });
    });

    describe('findPointAfterTime', () => {
        it('should return the point at the specified time', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointAfterTime(threePoints[0].time), threePoints[0]);
            assert.equal(timeSeries.findPointAfterTime(threePoints[1].time), threePoints[1]);
            assert.equal(timeSeries.findPointAfterTime(threePoints[2].time), threePoints[2]);
        });

        it('should return the point after the specified time', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointAfterTime(threePoints[0].time - 0.1), threePoints[0]);
            assert.equal(timeSeries.findPointAfterTime(threePoints[1].time - 0.1), threePoints[1]);
            assert.equal(timeSeries.findPointAfterTime(threePoints[2].time - 0.1), threePoints[2]);
        });

        it('should return null when there are no points after the specified time', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries);
            assert.equal(timeSeries.findPointAfterTime(threePoints[2].time + 0.1), null);
        });

        it('should return the first point when there are multiple points at the specified time', () => {
            const timeSeries = new TimeSeries();
            const points = [
                {id: 909, time:  99, value: 105},
                {id: 910, time: 100, value: 110},
                {id: 902, time: 100, value: 102},
                {id: 930, time: 101, value: 130},
            ];
            addPointsToSeries(timeSeries, points);
            assert.equal(timeSeries.findPointAfterTime(points[1].time), points[1]);
        });
    });

    describe('viewBetweenPoints', () => {

        it('should return a view between two points', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const view = timeSeries.viewBetweenPoints(fivePoints[1], fivePoints[3]);
            assert.equal(view.length(), 3);
            assert.equal(view.firstPoint(), fivePoints[1]);
            assert.equal(view.lastPoint(), fivePoints[3]);

            assert.equal(view.nextPoint(fivePoints[1]), fivePoints[2]);
            assert.equal(view.nextPoint(fivePoints[2]), fivePoints[3]);
            assert.equal(view.nextPoint(fivePoints[3]), null);

            assert.equal(view.previousPoint(fivePoints[1]), null);
            assert.equal(view.previousPoint(fivePoints[2]), fivePoints[1]);
            assert.equal(view.previousPoint(fivePoints[3]), fivePoints[2]);

            assert.equal(view.findPointByIndex(0), fivePoints[1]);
            assert.equal(view.findPointByIndex(1), fivePoints[2]);
            assert.equal(view.findPointByIndex(2), fivePoints[3]);
            assert.equal(view.findPointByIndex(3), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), fivePoints[1]);
            assert.equal(view.findById(fivePoints[2].id), fivePoints[2]);
            assert.equal(view.findById(fivePoints[3].id), fivePoints[3]);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[1].value, fivePoints[2].value, fivePoints[3].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), fivePoints[2]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], fivePoints.slice(1, 4));
        });

        it('should return a view with exactly one point for when the starting point is identical to the ending point', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const view = timeSeries.viewBetweenPoints(fivePoints[2], fivePoints[2]);
            assert.equal(view.length(), 1);
            assert.equal(view.firstPoint(), fivePoints[2]);
            assert.equal(view.lastPoint(), fivePoints[2]);

            assert.equal(view.findPointByIndex(0), fivePoints[2]);
            assert.equal(view.findPointByIndex(1), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), null);
            assert.equal(view.findById(fivePoints[2].id), fivePoints[2]);
            assert.equal(view.findById(fivePoints[3].id), null);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[2].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[2]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[2]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), fivePoints[2]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], [fivePoints[2]]);
        });

    });

});

describe('TimeSeriesView', () => {

    describe('filter', () => {
        it('should call callback with an element in the view and its index', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const originalView = timeSeries.viewBetweenPoints(fivePoints[1], fivePoints[3]);
            const points = [];
            const indices = [];
            const view = originalView.filter((point, index) => {
                points.push(point);
                indices.push(index);
            });
            assert.deepEqual(points, [fivePoints[1], fivePoints[2], fivePoints[3]]);
            assert.deepEqual(indices, [0, 1, 2]);
        });

        it('should create a filtered view', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const originalView = timeSeries.viewBetweenPoints(fivePoints[0], fivePoints[4]);
            const view = originalView.filter((point) => { return point == fivePoints[1] || point == fivePoints[3]; });

            assert.equal(view.length(), 2);
            assert.equal(view.firstPoint(), fivePoints[1]);
            assert.equal(view.lastPoint(), fivePoints[3]);

            assert.equal(view.nextPoint(fivePoints[1]), fivePoints[3]);
            assert.equal(view.nextPoint(fivePoints[3]), null);

            assert.equal(view.previousPoint(fivePoints[1]), null);
            assert.equal(view.previousPoint(fivePoints[3]), fivePoints[1]);

            assert.equal(view.findPointByIndex(0), fivePoints[1]);
            assert.equal(view.findPointByIndex(1), fivePoints[3]);
            assert.equal(view.findPointByIndex(2), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), fivePoints[1]);
            assert.equal(view.findById(fivePoints[2].id), null);
            assert.equal(view.findById(fivePoints[3].id), fivePoints[3]);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[1].value, fivePoints[3].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[1].time), fivePoints[1]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], [fivePoints[1], fivePoints[3]]);
        });
    });

    describe('viewTimeRange', () => {
        it('should create a view filtered by the specified time range', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const originalView = timeSeries.viewBetweenPoints(fivePoints[0], fivePoints[4]);
            const view = originalView.viewTimeRange(fivePoints[1].time - 0.1, fivePoints[4].time - 0.1);

            assert.equal(view.length(), 3);
            assert.equal(view.firstPoint(), fivePoints[1]);
            assert.equal(view.lastPoint(), fivePoints[3]);

            assert.equal(view.nextPoint(fivePoints[1]), fivePoints[2]);
            assert.equal(view.nextPoint(fivePoints[2]), fivePoints[3]);
            assert.equal(view.nextPoint(fivePoints[3]), null);

            assert.equal(view.previousPoint(fivePoints[1]), null);
            assert.equal(view.previousPoint(fivePoints[2]), fivePoints[1]);
            assert.equal(view.previousPoint(fivePoints[3]), fivePoints[2]);

            assert.equal(view.findPointByIndex(0), fivePoints[1]);
            assert.equal(view.findPointByIndex(1), fivePoints[2]);
            assert.equal(view.findPointByIndex(2), fivePoints[3]);
            assert.equal(view.findPointByIndex(3), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), fivePoints[1]);
            assert.equal(view.findById(fivePoints[2].id), fivePoints[2]);
            assert.equal(view.findById(fivePoints[3].id), fivePoints[3]);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[1].value, fivePoints[2].value, fivePoints[3].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), fivePoints[1]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), fivePoints[2]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[1].time), fivePoints[1]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), fivePoints[2]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], fivePoints.slice(1, 4));
        });

        it('should create a view filtered by the specified time range on a view already filtered by a time range', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const originalView = timeSeries.viewBetweenPoints(fivePoints[0], fivePoints[4]);
            const prefilteredView = originalView.viewTimeRange(fivePoints[1].time - 0.1, fivePoints[4].time - 0.1);
            const view = prefilteredView.viewTimeRange(fivePoints[3].time - 0.1, fivePoints[3].time + 0.1);

            assert.equal(view.length(), 1);
            assert.equal(view.firstPoint(), fivePoints[3]);
            assert.equal(view.lastPoint(), fivePoints[3]);

            assert.equal(view.nextPoint(fivePoints[3]), null);
            assert.equal(view.previousPoint(fivePoints[3]), null);

            assert.equal(view.findPointByIndex(0), fivePoints[3]);
            assert.equal(view.findPointByIndex(1), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), null);
            assert.equal(view.findById(fivePoints[2].id), null);
            assert.equal(view.findById(fivePoints[3].id), fivePoints[3]);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[3].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[1].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], [fivePoints[3]]);
        });

        it('should create a view filtered by the specified time range on a view already filtered', () => {
            const timeSeries = new TimeSeries();
            addPointsToSeries(timeSeries, fivePoints);
            const originalView = timeSeries.viewBetweenPoints(fivePoints[0], fivePoints[4]);
            const prefilteredView = originalView.filter((point) => { return point == fivePoints[1] || point == fivePoints[3]; });
            const view = prefilteredView.viewTimeRange(fivePoints[3].time - 0.1, fivePoints[3].time + 0.1);

            assert.equal(view.length(), 1);
            assert.equal(view.firstPoint(), fivePoints[3]);
            assert.equal(view.lastPoint(), fivePoints[3]);

            assert.equal(view.nextPoint(fivePoints[3]), null);
            assert.equal(view.previousPoint(fivePoints[3]), null);

            assert.equal(view.findPointByIndex(0), fivePoints[3]);
            assert.equal(view.findPointByIndex(1), null);

            assert.equal(view.findById(fivePoints[0].id), null);
            assert.equal(view.findById(fivePoints[1].id), null);
            assert.equal(view.findById(fivePoints[2].id), null);
            assert.equal(view.findById(fivePoints[3].id), fivePoints[3]);
            assert.equal(view.findById(fivePoints[4].id), null);

            assert.deepEqual(view.values(), [fivePoints[3].value]);

            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time, fivePoints[1].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.firstPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time, fivePoints[1].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[0].time, fivePoints[0].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[1].time + 0.1, fivePoints[2].time), null);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[3].time - 0.1, fivePoints[4].time), fivePoints[3]);
            assert.deepEqual(view.lastPointInTimeRange(fivePoints[4].time, fivePoints[4].time), null);

            assert.deepEqual([...view], [fivePoints[3]]);
        });
    });

});
