
describe('TimeSeriesChart', () => {

    it('should be constructible with an empty sourec list and an empty options', () => {
        return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
            new TimeSeriesChart([], {});
        });
    });

    describe('computeTimeGrid', () => {
        it('should return an empty array when the start and the end times are identical', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const someTime = Date.now();
                const labels = TimeSeriesChart.computeTimeGrid(someTime, someTime, 0);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(0);
            });
        });

        const millisecondsPerHour = 3600 * 1000;
        const millisecondsPerDay = 24 * millisecondsPerHour;

        it('should return an empty array when maxLabels is 0', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = Date.now();
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, endTime, 0);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(0);
            });
        });

        it('should return an empty array when maxLabels is 0 even when the interval spans multiple months', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 2 * millisecondsPerHour, +endTime, 1);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(1);
                checkGridItem(labels[0], '6AM', '2017-01-15T06:00:00Z');
            });
        });

        it('should generate two two-hour labels for four hour interval when maxLabels is 2', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - 4 * millisecondsPerHour, +endTime, 2);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '4AM', '2017-01-15T04:00:00Z');
                checkGridItem(labels[1], '6AM', '2017-01-15T06:00:00Z');
            });
        });

        it('should generate six two-hour labels for twelve hour interval when maxLabels is 6', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, +endTime, 1);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(1);
                checkGridItem(labels[0], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate two one-day labels for one day interval when maxLabels is 2', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T07:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(endTime - millisecondsPerDay, +endTime, 2);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '1/14 12PM', '2017-01-14T12:00:00Z');
                checkGridItem(labels[1], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate four half-day labels for two day interval when maxLabels is 5', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
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
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(+endTime, +endTime + 14 * millisecondsPerDay, 3);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(2);
                checkGridItem(labels[0], '1/22', '2017-01-22T00:00:00Z');
                checkGridItem(labels[1], '1/29', '2017-01-29T00:00:00Z');
            });
        });

        it('should generate seven one-month labels for six and half months interval starting before 15th when maxLabels is 7', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(new Date('2016-07-12T18:53:00Z'), new Date('2017-01-18T08:17:53Z'), 7);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(7);
                checkGridItem(labels[0], '7/15', '2016-07-15T00:00:00Z');
                checkGridItem(labels[1], '8/15', '2016-08-15T00:00:00Z');
                checkGridItem(labels[2], '9/15', '2016-09-15T00:00:00Z');
                checkGridItem(labels[3], '10/15', '2016-10-15T00:00:00Z');
                checkGridItem(labels[4], '11/15', '2016-11-15T00:00:00Z');
                checkGridItem(labels[5], '12/15', '2016-12-15T00:00:00Z');
                checkGridItem(labels[6], '1/15', '2017-01-15T00:00:00Z');
            });
        });

        it('should generate seven one-month labels for six months interval staring after 15th when maxLabels is 7', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(new Date('2016-07-18T18:53:00Z'), new Date('2017-01-18T08:17:53Z'), 7);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(6);
                checkGridItem(labels[0], '8/1', '2016-08-01T00:00:00Z');
                checkGridItem(labels[1], '9/1', '2016-09-01T00:00:00Z');
                checkGridItem(labels[2], '10/1', '2016-10-01T00:00:00Z');
                checkGridItem(labels[3], '11/1', '2016-11-01T00:00:00Z');
                checkGridItem(labels[4], '12/1', '2016-12-01T00:00:00Z');
            });
        });

        it('should generate six two-months labels for one year interval when maxLabels is 7', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(new Date('2016-07-11T18:53:00Z'), new Date('2017-07-27T08:17:53Z'), 7);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(6);
                checkGridItem(labels[0], '9/1', '2016-09-01T00:00:00Z');
                checkGridItem(labels[1], '11/1', '2016-11-01T00:00:00Z');
                checkGridItem(labels[2], '1/1', '2017-01-01T00:00:00Z');
                checkGridItem(labels[3], '3/1', '2017-03-01T00:00:00Z');
                checkGridItem(labels[4], '5/1', '2017-05-01T00:00:00Z');
                checkGridItem(labels[5], '7/1', '2017-07-01T00:00:00Z');
            });
        });

        it('should generate four three-months labels for one year interval when maxLabels is 5', () => {
            return ChartTest.importChartScripts(new BrowsingContext).then((TimeSeriesChart) => {
                const endTime = new Date('2017-01-15T18:53:00Z');
                const labels = TimeSeriesChart.computeTimeGrid(new Date('2016-07-11T18:53:00Z'), new Date('2017-07-27T08:17:53Z'), 5);
                expect(labels).to.be.a('array');
                expect(labels.length).to.be(4);
                checkGridItem(labels[0], '10/1', '2016-10-01T00:00:00Z');
                checkGridItem(labels[1], '1/1', '2017-01-01T00:00:00Z');
                checkGridItem(labels[2], '4/1', '2017-04-01T00:00:00Z');
                checkGridItem(labels[3], '7/1', '2017-07-01T00:00:00Z');
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
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.5, 1.0, 1.5, 2.0]);
                expect(grid.map((item) => item.label)).to.eql(['0.5 pt', '1.0 pt', '1.5 pt', '2.0 pt']);
            });
        });

        it('should generate [0.4, 0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2] for [0.3, 2.3] when maxLabels is 10', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 10, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.4, 0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt', '1.4 pt', '1.6 pt', '1.8 pt', '2.0 pt', '2.2 pt']);
            });
        });

        it('should generate [1, 2] for [0.3, 2.3] when maxLabels is 2', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 2.3, 2, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([1, 2]);
                expect(grid.map((item) => item.label)).to.eql(['1.0 pt', '2.0 pt']);
            });
        });

        it('should generate [0.4, 0.6, 0.8, 1.0, 1.2] for [0.3, 1.3] when maxLabels is 5', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 1.3, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.4, 0.6, 0.8, 1.0, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt']);
            });
        });

        it('should generate [0.2, 0.4, 0.6, 0.8, 1, 1.2] for [0.2, 1.3] when maxLabels is 10', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.2, 1.3, 10, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.2, 0.4, 0.6, 0.8, 1, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['0.2 pt', '0.4 pt', '0.6 pt', '0.8 pt', '1.0 pt', '1.2 pt']);
            });
        });

        it('should generate [0.5, 1.0] for [0.3, 1.3] when maxLabels is 4', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.3, 1.3, 4, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.5, 1.0]);
                expect(grid.map((item) => item.label)).to.eql(['0.5 pt', '1.0 pt']);
            });
        });

        it('should generate [10, 20, 30] for [4, 35] when maxLabels is 4', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(4, 35, 4, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 20, 30]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '20 pt', '30 pt']);
            });
        });

        it('should generate [10, 20, 30] for [4, 35] when maxLabels is 6', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(4, 35, 6, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 20, 30]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '20 pt', '30 pt']);
            });
        });

        it('should generate [10, 15, 20, 25, 30, 35] for [6, 35] when maxLabels is 6', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(6, 35, 6, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => item.value)).to.eql([10, 15, 20, 25, 30, 35]);
                expect(grid.map((item) => item.label)).to.eql(['10 pt', '15 pt', '20 pt', '25 pt', '30 pt', '35 pt']);
            });
        });

        it('should generate [110, 115, 120, 125, 130] for [107, 134] when maxLabels is 6', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(107, 134, 6, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([110, 115, 120, 125, 130]);
                expect(grid.map((item) => item.label)).to.eql(['110 pt', '115 pt', '120 pt', '125 pt', '130 pt']);
            });
        });

        it('should generate [5e7, 10e7] for [1e7, 1e8] when maxLabels is 4', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(1e7, 1e8, 4, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([5e7, 10e7]);
                expect(grid.map((item) => item.label)).to.eql(['50.0 Mpt', '100 Mpt']);
            });
        });

        it('should generate [2e7, 4e7, 6e7, 8e7, 10e7] for [1e7, 1e8] when maxLabels is 5', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(1e7, 1e8, 5, Metric.makeFormatter('pt', 3));
                expect(grid.map((item) => item.value)).to.eql([2e7, 4e7, 6e7, 8e7, 10e7]);
                expect(grid.map((item) => item.label)).to.eql(['20.0 Mpt', '40.0 Mpt', '60.0 Mpt', '80.0 Mpt', '100 Mpt']);
            });
        });

        it('should generate [-1.5, -1.0, -0.5, 0.0, 0.5] for [-1.8, 0.7] when maxLabels is 5', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(-1.8, 0.7, 5, Metric.makeFormatter('pt', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([-1.5, -1.0, -0.5, 0.0, 0.5]);
                expect(grid.map((item) => item.label)).to.eql(['-1.5 pt', '-1.0 pt', '-0.5 pt', '0.0 pt', '0.5 pt']);
            });
        });

        it('should generate [200ms, 400ms, 600ms, 800ms, 1.00s, 1.20s] for [0.2, 1.3] when maxLabels is 10 and unit is seconds', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const grid = TimeSeriesChart.computeValueGrid(0.2, 1.3, 10, Metric.makeFormatter('s', 3));
                expect(grid.map((item) => approximate(item.value))).to.eql([0.2, 0.4, 0.6, 0.8, 1, 1.2]);
                expect(grid.map((item) => item.label)).to.eql(['200 ms', '400 ms', '600 ms', '800 ms', '1.00 s', '1.20 s']);
            });
        });

        it('should generate [2.0GB, 4.0GB, 6.0GB] for [1.2GB, 7.2GB] when maxLabels is 4 and unit is bytes', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const gigabytes = Math.pow(1024, 3);
                const grid = TimeSeriesChart.computeValueGrid(1.2 * gigabytes, 7.2 * gigabytes, 4, Metric.makeFormatter('B', 2));
                expect(grid.map((item) => approximate(item.value))).to.eql([2 * gigabytes, 4 * gigabytes, 6 * gigabytes]);
                expect(grid.map((item) => item.label)).to.eql(['2.0 GB', '4.0 GB', '6.0 GB']);
            });
        });

        it('should generate [0.6GB, 0.8GB, 1.0GB, 1.2GB] for [0.53GB, 1.23GB] when maxLabels is 4 and unit is bytes', () => {
            const context = new BrowsingContext;
            return ChartTest.importChartScripts(context).then((TimeSeriesChart) => {
                const Metric = context.symbols.Metric;
                const gigabytes = Math.pow(1024, 3);
                const grid = TimeSeriesChart.computeValueGrid(0.53 * gigabytes, 1.23 * gigabytes, 4, Metric.makeFormatter('B', 2));
                expect(grid.map((item) => item.label)).to.eql(['0.6 GB', '0.8 GB', '1.0 GB', '1.2 GB']);
            });
        });

    });

    describe('fetchMeasurementSets', () => {

        it('should fetch the measurement set and create a canvas element upon receiving the data', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                expect(chart.content().querySelector('canvas')).to.be(null);
                return waitForComponentsToRender(context).then(() => {
                    expect(chart.content().querySelector('canvas')).to.not.be(null);
                });
            });
        });

        it('should immediately enqueue to render when the measurement set had already been fetched', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context);

                let set = context.symbols.MeasurementSet.findSet(1, 1, 0);
                let promise = set.fetchBetween(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                return promise.then(() => {
                    expect(chart.content().querySelector('canvas')).to.be(null);
                    chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
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
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context);

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

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

    describe('sampledTimeSeriesData', () => {
        it('should not contain an outlier when includeOutliers is false', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{includeOutliers: false}])

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    const view = chart.sampledTimeSeriesData('current');
                    expect(view.length()).to.be(5);
                    for (let point of view)
                        expect(point.markedOutlier).to.be(false);
                });
            });
        });

        it('should contain every outlier when includeOutliers is true', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{includeOutliers: true}])

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    const view = chart.sampledTimeSeriesData('current');
                    expect(view.length()).to.be(7);
                    expect(view.findPointByIndex(1).markedOutlier).to.be(true);
                    expect(view.findPointByIndex(5).markedOutlier).to.be(true);
                });
            });
        });

        it('should only contain data points in the domain and one preceding point when there are no succeeding points', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{includeOutliers: true}])

                chart.setDomain(posixTime('2016-01-06T00:00:00Z'), posixTime('2016-01-07T00:00:00Z'));
                chart.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    const view = chart.sampledTimeSeriesData('current');
                    expect([...view].map((point) => point.id)).to.be.eql([1003, 1004, 1005, 1006]);
                });
            });
        });

        it('should only contain data points in the domain and one succeeding point when there are no preceding points', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{includeOutliers: true}])

                chart.setDomain(posixTime('2016-01-05T00:00:00Z'), posixTime('2016-01-06T00:00:00Z'));
                chart.fetchMeasurementSets();
                chart.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    const view = chart.sampledTimeSeriesData('current');
                    expect([...view].map((point) => point.id)).to.be.eql([1000, 1001, 1002, 1003, 1004]);
                });
            });
        });

        it('should only contain data points in the domain and one preceding point and one succeeding point', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{includeOutliers: true}])

                chart.setDomain(posixTime('2016-01-05T21:00:00Z'), posixTime('2016-01-06T02:00:00Z'));
                chart.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    const view = chart.sampledTimeSeriesData('current');
                    expect([...view].map((point) => point.id)).to.be.eql([1002, 1003, 1004, 1005]);
                });
            });
        });
    });

    describe('render', () => {
        it('should update the canvas size and its content after the window has been resized', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, null, {width: '100%', height: '100%'});

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                expect(dataChangeCount).to.be(0);
                expect(chart.sampledTimeSeriesData('current')).to.be(null);
                expect(chart.content().querySelector('canvas')).to.be(null);
                let canvas;
                let originalWidth;
                let originalHeight;
                return waitForComponentsToRender(context).then(() => {
                    expect(dataChangeCount).to.be(1);
                    expect(chart.sampledTimeSeriesData('current')).to.not.be(null);
                    canvas = chart.content().querySelector('canvas');
                    expect(canvas).to.not.be(null);

                    originalWidth = canvas.offsetWidth;
                    originalHeight = canvas.offsetHeight;
                    expect(originalWidth).to.be(context.document.body.offsetWidth);
                    expect(originalHeight).to.be(context.document.body.offsetHeight);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    context.iframe.style.width = context.iframe.offsetWidth * 2 + 'px';
                    context.global.dispatchEvent(new Event('resize'));

                    expect(canvas.offsetWidth).to.be(originalWidth);
                    expect(canvas.offsetHeight).to.be(originalHeight);

                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(dataChangeCount).to.be(2);
                    expect(canvas.offsetWidth).to.be.greaterThan(originalWidth);
                    expect(canvas.offsetWidth).to.be(context.document.body.offsetWidth);
                    expect(canvas.offsetHeight).to.be(originalHeight);
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                });
            });
        });

        it('should not update update the canvas when the window has been resized but its dimensions stays the same', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createChartWithSampleCluster(context, null, {width: '100px', height: '100px'});

                let dataChangeCount = 0;
                chart.listenToAction('dataChange', () => dataChangeCount++);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);
                expect(dataChangeCount).to.be(0);

                let canvas;
                let data;
                return waitForComponentsToRender(context).then(() => {
                    expect(dataChangeCount).to.be(1);
                    data = chart.sampledTimeSeriesData('current');
                    expect(data).to.not.be(null);
                    canvas = chart.content().querySelector('canvas');
                    expect(canvas).to.not.be(null);

                    expect(canvas.offsetWidth).to.be(100);
                    expect(canvas.offsetHeight).to.be(100);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    context.iframe.style.width = context.iframe.offsetWidth * 2 + 'px';
                    context.global.dispatchEvent(new Event('resize'));

                    expect(canvas.offsetWidth).to.be(100);
                    expect(canvas.offsetHeight).to.be(100);

                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(dataChangeCount).to.be(1);
                    expect(chart.sampledTimeSeriesData('current')).to.be(data);
                    expect(canvas.offsetWidth).to.be(100);
                    expect(canvas.offsetHeight).to.be(100);
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);
                });
            });
        });

        it('should render Y-axis', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chartWithoutYAxis = ChartTest.createChartWithSampleCluster(context, null, {axis:
                    {
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('ms', 3),
                    }
                });
                const chartWithYAxis1 = ChartTest.createChartWithSampleCluster(context, null, {axis:
                    {
                        yAxisWidth: 4,
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('ms', 3),
                    }
                });
                const chartWithYAxis2 = ChartTest.createChartWithSampleCluster(context, null, {axis:
                    {
                        yAxisWidth: 4,
                        gridStyle: '#ccc',
                        fontSize: 1,
                        valueFormatter: context.symbols.Metric.makeFormatter('B', 3),
                    }
                });

                chartWithoutYAxis.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithoutYAxis.fetchMeasurementSets();
                chartWithYAxis1.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithYAxis1.fetchMeasurementSets();
                chartWithYAxis2.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithYAxis2.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                return waitForComponentsToRender(context).then(() => {
                    let canvasWithoutYAxis = chartWithoutYAxis.content().querySelector('canvas');
                    let canvasWithYAxis1 = chartWithYAxis1.content().querySelector('canvas');
                    let canvasWithYAxis2 = chartWithYAxis2.content().querySelector('canvas');
                    CanvasTest.expectCanvasesMismatch(canvasWithoutYAxis, canvasWithYAxis1);
                    CanvasTest.expectCanvasesMismatch(canvasWithoutYAxis, canvasWithYAxis1);
                    CanvasTest.expectCanvasesMismatch(canvasWithYAxis1, canvasWithYAxis2);

                    expect(CanvasTest.canvasContainsColor(canvasWithYAxis1, {r: 204, g: 204, b: 204},
                        {x: canvasWithYAxis1.width - 1, width: 1, y: 0, height: canvasWithYAxis1.height})).to.be(true);
                });
            });
        });

        it('should render the sampled time series', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const lineStyle = 'rgb(0, 128, 255)';
                const lineColor = {r: 0, g: 128, b: 255};
                const chartOptions = {width: '100px', height: '100px'};
                const chartWithoutSampling = ChartTest.createChartWithSampleCluster(context, [{lineStyle, sampleData: false}], chartOptions);
                const chartWithSampling = ChartTest.createChartWithSampleCluster(context, [{lineStyle, sampleData: true}], chartOptions);

                chartWithoutSampling.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithoutSampling.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                chartWithSampling.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithSampling.fetchMeasurementSets();

                let canvasWithSampling;
                let canvasWithoutSampling;
                return waitForComponentsToRender(context).then(() => {
                    canvasWithoutSampling = chartWithoutSampling.content().querySelector('canvas');
                    canvasWithSampling = chartWithSampling.content().querySelector('canvas');

                    CanvasTest.expectCanvasesMatch(canvasWithSampling, canvasWithoutSampling);
                    expect(CanvasTest.canvasContainsColor(canvasWithoutSampling, lineColor)).to.be(true);
                    expect(CanvasTest.canvasContainsColor(canvasWithSampling, lineColor)).to.be(true);

                    const diff = ChartTest.sampleCluster.endTime - ChartTest.sampleCluster.startTime;
                    chartWithoutSampling.setDomain(ChartTest.sampleCluster.startTime - 2 * diff, ChartTest.sampleCluster.endTime);
                    chartWithSampling.setDomain(ChartTest.sampleCluster.startTime - 2 * diff, ChartTest.sampleCluster.endTime);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithoutSampling);
                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithSampling);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithoutSampling)).to.be(true);
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithSampling)).to.be(true);

                    expect(CanvasTest.canvasContainsColor(canvasWithoutSampling, lineColor)).to.be(true);
                    expect(CanvasTest.canvasContainsColor(canvasWithSampling, lineColor)).to.be(true);

                    CanvasTest.expectCanvasesMismatch(canvasWithSampling, canvasWithoutSampling);
                });
            });
        });

        it('should render annotations', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const options = {annotations: { textStyle: '#000', textBackground: '#fff', minWidth: 3, barHeight: 7, barSpacing: 2, fillStyle: '#ccc'}};
                const chartWithoutAnnotations = ChartTest.createChartWithSampleCluster(context, null, options);
                const chartWithAnnotations = ChartTest.createChartWithSampleCluster(context, null, options);

                chartWithoutAnnotations.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithoutAnnotations.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                chartWithAnnotations.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithAnnotations.fetchMeasurementSets();

                let canvasWithAnnotations;
                return waitForComponentsToRender(context).then(() => {
                    const diff = ChartTest.sampleCluster.endTime - ChartTest.sampleCluster.startTime;
                    chartWithAnnotations.setAnnotations([{
                        startTime: ChartTest.sampleCluster.startTime + diff / 4,
                        endTime: ChartTest.sampleCluster.startTime + diff / 2,
                        label: 'hello, world',
                    }]);

                    canvasWithAnnotations = chartWithAnnotations.content().querySelector('canvas');
                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithAnnotations);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithAnnotations)).to.be(true);
                
                    const canvasWithoutAnnotations = chartWithoutAnnotations.content().querySelector('canvas');
                    CanvasTest.expectCanvasesMismatch(canvasWithAnnotations, canvasWithoutAnnotations);
                });
            });
        });
    });

});
