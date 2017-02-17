
describe('TimeSeriesChart', () => {
    const scripts = [
        '../shared/statistics.js',
        'instrumentation.js',
        'models/data-model.js',
        'models/metric.js',
        'models/time-series.js',
        'models/measurement-set.js',
        'models/measurement-cluster.js',
        'models/measurement-adaptor.js',
        'components/base.js',
        'components/time-series-chart.js'];

    it('should be constructible with an empty sourec list and an empty options', () => {
        return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
            new TimeSeriesChart([], {});
        });
    });

    describe('computeTimeGrid', () => {
        it('should return an empty array when the start and the end times are identical', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const someTime = Date.now();
                const labels = TimeSeriesChart.computeTimeGrid(someTime, someTime, 0);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(0);
            });
        });

        const millisecondsPerHour = 3600 * 1000;
        const millisecondsPerDay = 24 * millisecondsPerHour;

        it('should return an empty array when maxLabels is 0', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = Date.now();
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, endTime, 0);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(0);
            });
        });

        it('should return an empty array when maxLabels is 0 even when the interval spans multiple months', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = Date.now();
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 120 * millisecondsPerDay, endTime, 0);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(0);
            });
        });

        function checkGridItem(item, label, expectedDate)
        {
            expect(item.label).to.be(label);
            expect(item.time.__proto__.constructor.name).to.be('Date');
            expect(+item.time).to.be(+new Date(expectedDate));
        }

        it('should generate one hour label with just day for two hour interval when maxLabels is 1', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 2 * millisecondsPerHour, +endTime, 1);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(1);
                checkGridItem(labels[0], '6AM', '2017-01-15T06:00:00Z');
            });
        });

        it('should generate two two-hour labels for four hour interval when maxLabels is 2', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 4 * millisecondsPerHour, +endTime, 2);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '4AM', '2017-01-15T04:00:00Z');
                checkGridItem(labels[1], '6AM', '2017-01-15T06:00:00Z');
            });
        });

        it('should generate six two-hour labels for twelve hour interval when maxLabels is 6', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 12 * millisecondsPerHour, 6);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(6);
                checkGridItem(labels[0], '8AM', '2017-01-15T08:00:00Z');
                checkGridItem(labels[1], '10AM', '2017-01-15T10:00:00Z');
                checkGridItem(labels[2], '12PM', '2017-01-15T12:00:00Z');
                checkGridItem(labels[3], '2PM', '2017-01-15T14:00:00Z');
                checkGridItem(labels[4], '4PM', '2017-01-15T16:00:00Z');
                checkGridItem(labels[5], '6PM', '2017-01-15T18:00:00Z');
            });
        });

        it('should generate six two-hour labels with one date label for twelve hour interval that cross a day when maxLabels is 6', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T16:12:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 12 * millisecondsPerHour, 6);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(6);
                checkGridItem(labels[0], '6PM', '2017-01-15T18:00:00Z');
                checkGridItem(labels[1], '8PM', '2017-01-15T20:00:00Z');
                checkGridItem(labels[2], '10PM', '2017-01-15T22:00:00Z');
                checkGridItem(labels[3], '1/16', '2017-01-16T00:00:00Z');
                checkGridItem(labels[4], '2AM', '2017-01-16T02:00:00Z');
                checkGridItem(labels[5], '4AM', '2017-01-16T04:00:00Z');
            });
        });

        it('should generate three two-hour labels for six hour interval that cross a year when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2016-12-31T21:37:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 6 * millisecondsPerHour, 5);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(3);
                checkGridItem(labels[0], '10PM', '2016-12-31T22:00:00Z');
                checkGridItem(labels[1], '1/1', '2017-01-01T00:00:00Z');
                checkGridItem(labels[2], '2AM', '2017-01-01T02:00:00Z');
            });
        });

        it('should generate one one-day label for one day interval when maxLabels is 1', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, +endTime, 1);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(1);
                checkGridItem(labels[0], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate two one-day labels for one day interval when maxLabels is 2', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, +endTime, 2);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '1/14 12PM', '2017-01-14T12:00:00Z');
                checkGridItem(labels[1], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate four half-day labels for two day interval when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T16:12:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 2 * millisecondsPerDay, 5);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(4);
                checkGridItem(labels[0], '1/16', '2017-01-16T00:00:00Z');
                checkGridItem(labels[1], '12PM', '2017-01-16T12:00:00Z');
                checkGridItem(labels[2], '1/17', '2017-01-17T00:00:00Z');
                checkGridItem(labels[3], '12PM', '2017-01-17T12:00:00Z');
            });
        });

        it('should generate four half-day labels for two day interval that cross a year when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2016-12-31T09:12:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 2 * millisecondsPerDay, 5);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(4);
                checkGridItem(labels[0], '12/31 12PM', '2016-12-31T12:00:00Z');
                checkGridItem(labels[1], '1/1', '2017-01-01T00:00:00Z');
                checkGridItem(labels[2], '12PM', '2017-01-01T12:00:00Z');
                checkGridItem(labels[3], '1/2', '2017-01-02T00:00:00Z');
            });
        });

        it('should generate seven per-day labels for one week interval when maxLabels is 10', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 7 * millisecondsPerDay, endTime, 10);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(7);
                checkGridItem(labels[0], '1/9', '2017-01-09T00:00:00Z');
                checkGridItem(labels[1], '1/10', '2017-01-10T00:00:00Z');
                checkGridItem(labels[2], '1/11', '2017-01-11T00:00:00Z');
                checkGridItem(labels[3], '1/12', '2017-01-12T00:00:00Z');
                checkGridItem(labels[4], '1/13', '2017-01-13T00:00:00Z');
                checkGridItem(labels[5], '1/14', '2017-01-14T00:00:00Z');
                checkGridItem(labels[6], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate three two-day labels for one week interval when maxLabels is 4', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 7 * millisecondsPerDay, endTime, 4);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(3);
                checkGridItem(labels[0], '1/10', '2017-01-10T00:00:00Z');
                checkGridItem(labels[1], '1/12', '2017-01-12T00:00:00Z');
                checkGridItem(labels[2], '1/14', '2017-01-14T00:00:00Z');
            });
        });

        it('should generate seven one-day labels for two week interval when maxLabels is 8', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 14 * millisecondsPerDay, 8);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(7);
                checkGridItem(labels[0], '1/17', '2017-01-17T00:00:00Z');
                checkGridItem(labels[1], '1/19', '2017-01-19T00:00:00Z');
                checkGridItem(labels[2], '1/21', '2017-01-21T00:00:00Z');
                checkGridItem(labels[3], '1/23', '2017-01-23T00:00:00Z');
                checkGridItem(labels[4], '1/25', '2017-01-25T00:00:00Z');
                checkGridItem(labels[5], '1/27', '2017-01-27T00:00:00Z');
                checkGridItem(labels[6], '1/29', '2017-01-29T00:00:00Z');
            });
        });

        it('should generate two one-week labels for two week interval when maxLabels is 3', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart').then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 14 * millisecondsPerDay, 3);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '1/22', '2017-01-22T00:00:00Z');
                checkGridItem(labels[1], '1/29', '2017-01-29T00:00:00Z');
            });
        });
    });

    describe('computeValueGrid', () => {

        function checkValueGrid(actual, expected) {
            expect(actual).to.be.a('array');
            expect(JSON.stringify(actual)).to.be(JSON.stringify(expected));
        }

        function approximate(number)
        {
            return Math.round(number * 100000000) / 100000000;
        }

        it('should generate [0.5, 1.0, 1.5, 2.0] for [0.3, 2.3] when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.5, 1.0, 1.5, 2.0]);
                expect(grid.map((item) => item.label)).to.eql(['0.5 pt', '1.0 pt', '1.5 pt', '2.0 pt']);
            });
        });

        it('should generate [0.4, 0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2] for [0.3, 2.3] when maxLabels is 10', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 10, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.4, 0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt', '1.4 pt', '1.6 pt', '1.8 pt', '2.0 pt', '2.2 pt']);
            });
        });

        it('should generate [1, 2] for [0.3, 2.3] when maxLabels is 2', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 2, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([1, 2]);
                expect(grid.map((item) => item.label)).to.eql(['1.0 pt', '2.0 pt']);
            });
        });

        it('should generate [0.4, 0.6, 0.8, 1.0, 1.2] for [0.3, 1.3] when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 1.3, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.4, 0.6, 0.8, 1.0, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt']);
            });
        });

        it('should generate [0.2, 0.4, 0.6, 0.8, 1, 1.2] for [0.2, 1.3] when maxLabels is 10', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.2, 1.3, 10, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.2, 0.4, 0.6, 0.8, 1, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.2 pt', '0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt']);
            });
        });

        it('should generate [0.5, 1.0] for [0.3, 1.3] when maxLabels is 4', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 1.3, 4, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.5, 1.0]);
                expect(grid.map((item) => item.label)).to.eql(['0.5 pt', '1.0 pt']);
            });
        });

        it('should generate [10, 20, 30] for [4, 35] when maxLabels is 4', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(4, 35, 4, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 20, 30]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '20 pt', '30 pt']);
            });
        });

        it('should generate [10, 20, 30] for [4, 35] when maxLabels is 6', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(4, 35, 6, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 20, 30]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '20 pt', '30 pt']);
            });
        });

        it('should generate [10, 15, 20, 25, 30, 35] for [6, 35] when maxLabels is 6', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(6, 35, 6, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 15, 20, 25, 30, 35]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '15 pt', '20 pt', '25 pt', '30 pt', '35 pt']);
            });
        });

        it('should generate [110, 115, 120, 125, 130] for [107, 134] when maxLabels is 6', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(107, 134, 6, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([110, 115, 120, 125, 130]);
                expect(grid.map((item) => item.label)).to.eql(['110 pt', '115 pt', '120 pt', '125 pt', '130 pt']);
            });
        });

        it('should generate [5e7, 10e7] for [1e7, 1e8] when maxLabels is 4', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(1e7, 1e8, 4, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([5e7, 10e7]);
                expect(grid.map((item) => item.label)).to.eql(['50.0 Mpt', '100 Mpt']);
            });
        });

        it('should generate [2e7, 4e7, 6e7, 8e7, 10e7] for [1e7, 1e8] when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(1e7, 1e8, 5, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([2e7, 4e7, 6e7, 8e7, 10e7]);
                expect(grid.map((item) => item.label)).to.eql(['20.0 Mpt', '40.0 Mpt', '60.0 Mpt', '80.0 Mpt', '100 Mpt']);
            });
        });

        it('should generate [-1.5, -1.0, -0.5, 0.0, 0.5] for [-1.8, 0.7] when maxLabels is 5', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(-1.8, 0.7, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([-1.5, -1.0, -0.5, 0.0, 0.5]);
                expect(grid.map((item) => item.label)).to.eql(['-1.5 pt', '-1.0 pt', '-0.5 pt', '0.0 pt', '0.5 pt']);
            });
        });

        it('should generate [200ms, 400ms, 600ms, 800ms, 1.00s, 1.20s] for [0.2, 1.3] when maxLabels is 10 and unit is seconds', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const grid = TimeSeriesChart.computeValueGrid(0.2, 1.3, 10, Metric.makeFormatter('s', 3));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.2, 0.4, 0.6, 0.8, 1, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['200 ms', '400 ms', '600 ms', '800 ms', '1.00 s', '1.20 s']);
            });
        });

        it('should generate [2.0GB, 4.0GB, 6.0GB] for [1.2GB, 7.2GB] when maxLabels is 4 and unit is bytes', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const gigabytes = Math.pow(1024, 3);
                const grid = TimeSeriesChart.computeValueGrid(1.2 * gigabytes, 7.2 * gigabytes, 4, Metric.makeFormatter('B', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([2 * gigabytes, 4 * gigabytes, 6 * gigabytes]);
                expect(grid.map((item) => item.label)).to.eql(['2.0 GB', '4.0 GB', '6.0 GB']);
            });
        });

        it('should generate [0.6GB, 0.8GB, 1.0GB, 1.2GB] for [0.53GB, 1.23GB] when maxLabels is 4 and unit is bytes', () => {
            return new BrowsingContext().importScripts(scripts, 'TimeSeriesChart', 'Metric').then((symbols) => {
                const [TimeSeriesChart, Metric] = symbols;
                const gigabytes = Math.pow(1024, 3);
                const grid = TimeSeriesChart.computeValueGrid(0.53 * gigabytes, 1.23 * gigabytes, 4, Metric.makeFormatter('B', 2));
                expect(grid.map((item) => item.label)).to.eql(['0.6 GB', '0.8 GB', '1.0 GB', '1.2 GB']);
            });
        });

    });

    // Data from https://perf.webkit.org/v3/#/charts?paneList=((15-769))&since=1476426488465
    const sampleCluster = {
        "clusterStart": 946684800000,
        "clusterSize": 5184000000,
        "configurations": {
            "current": [
                [
                    26530031, 135.26375, 80, 10821.1, 1481628.13, false,
                    [ [27173, 1, "210096", 1482398562950] ],
                    1482398562950, 52999, 1482413222311, "10877", 7
                ],
                [
                    26530779, 153.2675, 80, 12261.4, 1991987.4, true, // changed to true.
                    [ [27174,1,"210097",1482424870729] ],
                    1482424870729, 53000, 1482424992735, "10878", 7
                ],
                [
                    26532275, 134.2725, 80, 10741.8, 1458311.88, false,
                    [ [ 27176, 1, "210102", 1482431464371 ] ],
                    1482431464371, 53002, 1482436041865, "10879", 7
                ],
                [
                    26547226, 150.9625, 80, 12077, 1908614.94, false,
                    [ [ 27195, 1, "210168", 1482852412735 ] ],
                    1482852412735, 53022, 1482852452143, "10902", 7
                ],
                [
                    26559915, 141.72, 80, 11337.6, 1633126.8, false,
                    [ [ 27211, 1, "210222", 1483347732051 ] ],
                    1483347732051, 53039, 1483347926429, "10924", 7
                ],
                [
                    26564388, 138.13125, 80, 11050.5, 1551157.93, false,
                    [ [ 27217, 1, "210231", 1483412171531 ] ],
                    1483412171531, 53045, 1483415426049, "10930", 7
                ],
                [
                    26568867, 144.16, 80, 11532.8, 1694941.1, false,
                    [ [ 27222, 1, "210240", 1483469584347 ] ],
                    1483469584347, 53051, 1483469642993, "10935", 7
                ]
            ]
        },
        "formatMap": [
            "id", "mean", "iterationCount", "sum", "squareSum", "markedOutlier",
            "revisions",
            "commitTime", "build", "buildTime", "buildNumber", "builder"
        ],
        "startTime": 1480636800000,
        "endTime": 1485820800000,
        "lastModified": 1484105738736,
        "clusterCount": 1,
        "elapsedTime": 56.421995162964,
        "status": "OK"
    };

    function createChartWithSampleCluster(context, chartOptions = {}, options = {width: '500px', height: '150px'})
    {
        const TimeSeriesChart = context.symbols.TimeSeriesChart;
        const MeasurementSet = context.symbols.MeasurementSet;

        const chart = new TimeSeriesChart([{type: 'current', measurementSet: MeasurementSet.findSet(1, 1, 0)}], chartOptions);
        const element = chart.element();
        element.style.width = options.width;
        element.style.height = options.height;
        context.document.body.appendChild(element);

        return chart;
    }

    function respondWithSampleCluster(request)
    {
        expect(request.url).to.be('../data/measurement-set-1-1.json');
        expect(request.method).to.be('GET');
        request.resolve(sampleCluster);
    }

    describe('fetchMeasurementSets', () => {

        it('should fetch the measurement set and create a canvas element upon receiving the data', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI').then(() => {
                const chart = createChartWithSampleCluster(context);

                chart.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);

                expect(chart.content().querySelector('canvas')).to.be(null);
                return waitForComponentsToRender(context).then(() => {
                    expect(chart.content().querySelector('canvas')).to.not.be(null);
                });
            });
        });

        it('should immediately enqueue to render when the measurement set had already been fetched', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI').then(() => {
                const chart = createChartWithSampleCluster(context);

                let set = context.symbols.MeasurementSet.findSet(1, 1, 0);
                let promise = set.fetchBetween(sampleCluster.startTime, sampleCluster.endTime);

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);

                return promise.then(() => {
                    expect(chart.content().querySelector('canvas')).to.be(null);
                    chart.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                    chart.fetchMeasurementSets();
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(requests.length).to.be(1);
                    expect(chart.content().querySelector('canvas')).to.not.be(null);
                });
            });
        });

        it('should dispatch "dataChange" action once the fetched data becomes available', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI').then(() => {
                const chart = createChartWithSampleCluster(context);

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);

                expect(dataChangeCount).to.be(0);
                expect(chart.sampledTimeSeriesData('current')).to.be(null);
                expect(chart.content().querySelector('canvas')).to.be(null);
                return waitForComponentsToRender(context).then(() => {
                    expect(dataChangeCount).to.be(1);
                    expect(chart.sampledTimeSeriesData('current')).to.not.be(null);
                    expect(chart.content().querySelector('canvas')).to.not.be(null);
                });
            });
        });
    });

    function fillCanvasBeforeRedrawCheck(canvas)
    {
        const canvasContext = canvas.getContext('2d');
        canvasContext.fillStyle = 'white';
        canvasContext.fillRect(0, 0, canvas.offsetWidth, canvas.offsetHeight);
    }

    function hasCanvasBeenRedrawn(canvas)
    {
        return canvasImageData(canvas).data.some((value) => value != 255);
    }

    function canvasImageData(canvas)
    {
        return canvas.getContext('2d').getImageData(0, 0, canvas.offsetWidth, canvas.offsetHeight);
    }

    function canvasRefTest(canvas1, canvas2, shouldMatch)
    {
        expect(canvas1.offsetWidth).to.be(canvas2.offsetWidth);
        expect(canvas2.offsetHeight).to.be(canvas2.offsetHeight);
        const data1 = canvasImageData(canvas1).data;
        const data2 = canvasImageData(canvas2).data;
        expect(data1.length).to.be.a('number');
        expect(data1.length).to.be(data2.length);

        let match = true;
        for (let i = 0; i < data1.length; i++) {
            if (data1[i] != data2[i]) {
                match = false;
                break;
            }
        }

        if (match == shouldMatch)
            return;

        [canvas1, canvas2].forEach((canvas) => {
            let image = document.createElement('img');
            image.src = canvas.toDataURL();
            image.style.display = 'block';
            document.body.appendChild(image);
        });

        throw new Error(shouldMatch ? 'Canvas contents were different' : 'Canvas contents were identical');
    }
    function expectCanvasesMatch(canvas1, canvas2) { return canvasRefTest(canvas1, canvas2, true); }
    function expectCanvasesMismatch(canvas1, canvas2) { return canvasRefTest(canvas1, canvas2, false); }

    describe('render', () => {

        it('should update the canvas size and its content after the window has been resized', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI').then(() => {
                const chart = createChartWithSampleCluster(context, {}, {width: '100%', height: '100%'});

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);

                expect(dataChangeCount).to.be(0);
                expect(chart.sampledTimeSeriesData('current')).to.be(null);
                expect(chart.content().querySelector('canvas')).to.be(null);
                let canvas;
                let originalWidth;
                let originalHeight;
                const dpr = window.devicePixelRatio || 1;
                return waitForComponentsToRender(context).then(() => {
                    expect(dataChangeCount).to.be(1);
                    expect(chart.sampledTimeSeriesData('current')).to.not.be(null);
                    canvas = chart.content().querySelector('canvas');
                    expect(canvas).to.not.be(null);

                    originalWidth = canvas.offsetWidth;
                    originalHeight = canvas.offsetHeight;
                    expect(originalWidth).to.be(dpr * context.document.body.offsetWidth);
                    expect(originalHeight).to.be(dpr * context.document.body.offsetHeight);

                    fillCanvasBeforeRedrawCheck(canvas);
                    context.iframe.style.width = context.iframe.offsetWidth * 2 + 'px';
                    context.global.dispatchEvent(new Event('resize'));

                    expect(canvas.offsetWidth).to.be(originalWidth);
                    expect(canvas.offsetHeight).to.be(originalHeight);

                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(dataChangeCount).to.be(2);
                    expect(canvas.offsetWidth).to.be.greaterThan(originalWidth);
                    expect(canvas.offsetWidth).to.be(dpr * context.document.body.offsetWidth);
                    expect(canvas.offsetHeight).to.be(originalHeight);
                    expect(hasCanvasBeenRedrawn(canvas)).to.be(true);
                });
            });
        });

        it('should not update update the canvas when the window has been resized but its dimensions stays the same', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI').then(() => {
                const chart = createChartWithSampleCluster(context, {}, {width: '100px', height: '100px'});

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);
                expect(dataChangeCount).to.be(0);

                let canvas;
                let data;
                const dpr = window.devicePixelRatio || 1;
                return waitForComponentsToRender(context).then(() => {
                    expect(dataChangeCount).to.be(1);
                    data = chart.sampledTimeSeriesData('current');
                    expect(data).to.not.be(null);
                    canvas = chart.content().querySelector('canvas');
                    expect(canvas).to.not.be(null);

                    expect(canvas.offsetWidth).to.be(dpr * 100);
                    expect(canvas.offsetHeight).to.be(dpr * 100);

                    fillCanvasBeforeRedrawCheck(canvas);
                    context.iframe.style.width = context.iframe.offsetWidth * 2 + 'px';
                    context.global.dispatchEvent(new Event('resize'));

                    expect(canvas.offsetWidth).to.be(dpr * 100);
                    expect(canvas.offsetHeight).to.be(dpr * 100);

                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(dataChangeCount).to.be(1);
                    expect(chart.sampledTimeSeriesData('current')).to.be(data);
                    expect(canvas.offsetWidth).to.be(dpr * 100);
                    expect(canvas.offsetHeight).to.be(dpr * 100);
                    expect(hasCanvasBeenRedrawn(canvas)).to.be(false);
                });
            });
        });

        it('should render Y-axis', () => {
            const context = new BrowsingContext();
            return context.importScripts(scripts, 'ComponentBase', 'TimeSeriesChart', 'MeasurementSet', 'MockRemoteAPI', 'Metric').then(() => {
                const chartWithoutYAxis = createChartWithSampleCluster(context, {axis:
                    {
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('ms', 3),
                    }
                });
                const chartWithYAxis1 = createChartWithSampleCluster(context, {axis:
                    {
                        yAxisWidth: 4,
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('ms', 3),
                    }
                });
                const chartWithYAxis2 = createChartWithSampleCluster(context, {axis:
                    {
                        yAxisWidth: 4,
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('B', 3),
                    }
                });

                chartWithoutYAxis.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chartWithoutYAxis.fetchMeasurementSets();
                chartWithYAxis1.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chartWithYAxis1.fetchMeasurementSets();
                chartWithYAxis2.setDomain(sampleCluster.startTime, sampleCluster.endTime);
                chartWithYAxis2.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                respondWithSampleCluster(requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    let canvasWithoutYAxis = chartWithoutYAxis.content().querySelector('canvas');
                    let canvasWithYAxis1 = chartWithYAxis1.content().querySelector('canvas');
                    let canvasWithYAxis2 = chartWithYAxis2.content().querySelector('canvas');
                    expectCanvasesMismatch(canvasWithoutYAxis, canvasWithYAxis1);
                    expectCanvasesMismatch(canvasWithoutYAxis, canvasWithYAxis1);
                    expectCanvasesMismatch(canvasWithYAxis1, canvasWithYAxis2);

                    let content1 = canvasImageData(canvasWithYAxis1);
                    let foundGridLine = false;
                    for (let y = 0; y < content1.height; y++) {
                        let endOfY = content1.width * 4 * y;
                        let r = content1.data[endOfY - 4];
                        let g = content1.data[endOfY - 3];
                        let b = content1.data[endOfY - 2];
                        if (r == 204 && g == 204 && b == 204) {
                            foundGridLine = true;
                            break;
                        }
                    }
                    expect(foundGridLine).to.be(true);
                });
            });
        });
    });

});

