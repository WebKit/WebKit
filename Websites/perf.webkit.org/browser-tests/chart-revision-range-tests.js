
describe('ChartRevisionRange', () => {

    function importRevisionList(context)
    {
        return ChartTest.importChartScripts(context).then(() => {
            ChartTest.makeModelObjectsForSampleCluster(context);
            return context.importScripts(['lazily-evaluated-function.js', 'components/chart-revision-range.js'], 'ChartRevisionRange');
        });
    }

    describe('revisionList on a non-interactive chart', () => {
        it('should report the list of revision for the latest point', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importRevisionList(context).then((ChartRevisionRange) => {
                const chart = ChartTest.createChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                evaluator = new ChartRevisionRange(chart);
                expect(evaluator.revisionList()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const revisionList = evaluator.revisionList();
                expect(revisionList).to.not.be(null);
                expect(revisionList.length).to.be(2);

                expect(revisionList[0].repository.label()).to.be('SomeApp');
                expect(revisionList[0].label).to.be('r4006');
                expect(revisionList[0].from).to.be(null);
                expect(revisionList[0].to).to.be('4006');

                expect(revisionList[1].repository.label()).to.be('macOS');
                expect(revisionList[1].label).to.be('15C50');
                expect(revisionList[1].from).to.be(null);
                expect(revisionList[1].to).to.be('15C50');
            })
        });
    });


    describe('revisionList on an interactive chart', () => {

        it('should not report the list of revision for the latest point when there is no selection or indicator', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importRevisionList(context).then((ChartRevisionRange) => {
                const chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                evaluator = new ChartRevisionRange(chart);
                expect(evaluator.revisionList()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.revisionList()).to.be(null);
            })
        });

        it('should report the list of revision for the locked indicator with differences to the previous point', () => {
            const context = new BrowsingContext();
            let chart;
            let evaluator;
            return importRevisionList(context).then((ChartRevisionRange) => {
                chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                evaluator = new ChartRevisionRange(chart);
                expect(evaluator.revisionList()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.revisionList()).to.be(null);

                const currentView = chart.sampledTimeSeriesData('current');
                chart.setIndicator(currentView.lastPoint().id, true);

                let revisionList = evaluator.revisionList();
                expect(revisionList).to.not.be(null);
                expect(revisionList.length).to.be(2);

                expect(revisionList[0].repository.label()).to.be('SomeApp');
                expect(revisionList[0].label).to.be('r4005-r4006');
                expect(revisionList[0].from).to.be('4005');
                expect(revisionList[0].to).to.be('4006');

                expect(revisionList[1].repository.label()).to.be('macOS');
                expect(revisionList[1].label).to.be('15C50');
                expect(revisionList[1].from).to.be(null);
                expect(revisionList[1].to).to.be('15C50');

                chart.setIndicator(1004, true); // Across macOS change.

                revisionList = evaluator.revisionList();
                expect(revisionList.length).to.be(2);

                expect(revisionList[0].repository.label()).to.be('SomeApp');
                expect(revisionList[0].label).to.be('r4004-r4004');
                expect(revisionList[0].from).to.be('4004');
                expect(revisionList[0].to).to.be('4004');

                expect(revisionList[1].repository.label()).to.be('macOS');
                expect(revisionList[1].label).to.be('15B42 - 15C50');
                expect(revisionList[1].from).to.be('15B42');
                expect(revisionList[1].to).to.be('15C50');
            });
        });

        it('should report the list of revision for the selected range', () => {
            const context = new BrowsingContext();
            let chart;
            let evaluator;
            return importRevisionList(context).then((ChartRevisionRange) => {
                chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                evaluator = new ChartRevisionRange(chart);
                expect(evaluator.revisionList()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.revisionList()).to.be(null);

                const currentView = chart.sampledTimeSeriesData('current');
                chart.setSelection([currentView.firstPoint().time + 1, currentView.lastPoint().time - 1]);

                let revisionList = evaluator.revisionList();
                expect(revisionList).to.not.be(null);
                expect(revisionList.length).to.be(2);

                expect(revisionList[0].repository.label()).to.be('SomeApp');
                expect(revisionList[0].label).to.be('r4003-r4004'); // 4002 and 4005 are outliers and skipped.
                expect(revisionList[0].from).to.be('4003');
                expect(revisionList[0].to).to.be('4004');

                expect(revisionList[1].repository.label()).to.be('macOS');
                expect(revisionList[1].label).to.be('15B42 - 15C50');
                expect(revisionList[1].from).to.be('15B42');
                expect(revisionList[1].to).to.be('15C50');
            });
        });
    });

});
