
describe('ChartStatusEvaluator', () => {

    function importEvaluator(context)
    {
        const scripts = ['components/chart-status-evaluator.js'];

        return ChartTest.importChartScripts(context).then(() => {
            return context.importScripts(scripts, 'Test', 'Metric', 'ChartStatusEvaluator');
        }).then(() => {
            return context.symbols.ChartStatusEvaluator;
        });
    }

    function makeMetric(context, name) {
        const Test = context.symbols.Test;
        const Metric = context.symbols.Metric;

        const test = new Test(10, {name: 'SomeTest'});
        const metric = new Metric(1, {name: name, test: test});

        return metric;
    }

    describe('status on a non-interactive chart', () => {

        it('should report the current value of the latest data point', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            })
        });

        it('should compare the latest current data point to the baseline when for a smaller-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('better');
                expect(status.label).to.be('11.5% better than baseline (131 ms)');
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            })
        });

        it('should compare the latest current data point to the baseline when for a bigger-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Score');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('worse');
                expect(status.label).to.be('11.5% worse than baseline (131 pt)');
                expect(status.currentValue).to.be('116 pt');
                expect(status.relativeDelta).to.be(null);
            })
        });

        it('should compare the latest current data point to the target when for a smaller-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('worse');
                expect(status.label).to.be('27.5% until target (91.0 ms)');
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            })
        });

        it('should compare the latest current data point to the target when for a bigger-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Score');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('better');
                expect(status.label).to.be('27.5% until target (91.0 pt)');
                expect(status.currentValue).to.be('116 pt');
                expect(status.relativeDelta).to.be(null);
            })
        });

        it('should compare the latest current data point to the target when it is smaller than the baseline for a smaller-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be('27.5% until target (91.0 ms)');
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            });
        });

        it('should compare the latest current data point to the baseline when it is smaller than the baseline for a bigger-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Score');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('worse');
                expect(status.label).to.be('11.5% worse than baseline (131 pt)');
                expect(status.currentValue).to.be('116 pt');
                expect(status.relativeDelta).to.be(null);
            });
        });

        it('should compare the latest current data point to the baseline when it is bigger than the baseline and the target for a smaller-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0], {baselineIsSmaller: true});

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('worse');
                expect(status.label).to.be('274.2% worse than baseline (31.0 ms)');
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            });
        });

        it('should compare the latest current data point to the target when it is bigger than the baseline but smaller than the target for a bigger-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0], {baselineIsSmaller: true, targetIsBigger: true});

                const metric = makeMetric(context, 'Score');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be('4.1% until target (121 pt)');
                expect(status.currentValue).to.be('116 pt');
                expect(status.relativeDelta).to.be(null);
            });
        });

        it('should compare the latest current data point to the target when it is smaller than the target for a smaller-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0], {targetIsBigger: true});

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be('better');
                expect(status.label).to.be('4.1% better than target (121 ms)');
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be(null);
            });
        });

        it('should compare the latest current data point to the baseline when it is smaller than the target but bigger than the baseline for a bigger-is-better unit', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createChartWithSampleCluster(context, [{type: 'current'}, {type: 'target'}, {type: 'baseline'}]);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0], {baselineIsSmaller: true, targetIsBigger: true});

                const metric = makeMetric(context, 'Score');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                const status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be('4.1% until target (121 pt)');
                expect(status.currentValue).to.be('116 pt');
                expect(status.relativeDelta).to.be(null);
            });
        });
    });

    describe('status on an interactive chart', () => {

        it('should not report the current value of the latest data point', () => {
            const context = new BrowsingContext();
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                const chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.status()).to.be(null);
            })
        });

        it('should report the current value and the relative delta when there is a locked indicator', () => {
            const context = new BrowsingContext();
            let chart;
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.status()).to.be(null);

                const currentView = chart.sampledTimeSeriesData('current');
                chart.setIndicator(currentView.lastPoint().id, true);

                let status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('116 ms');
                expect(status.relativeDelta).to.be('-6%');

                chart.setIndicator(currentView.previousPoint(currentView.lastPoint()).id, true);
                status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('124 ms');
                expect(status.relativeDelta).to.be('10%');

                chart.setIndicator(currentView.firstPoint().id, true);
                status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('100 ms');
                expect(status.relativeDelta).to.be(null);

                return waitForComponentsToRender(context);
            });
        });

        it('should report the current value and the relative delta when there is a selection with at least two points', () => {
            const context = new BrowsingContext();
            let chart;
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.status()).to.be(null);

                const currentView = chart.sampledTimeSeriesData('current');
                const firstPoint = currentView.firstPoint();
                const lastPoint = currentView.lastPoint();
                chart.setSelection([firstPoint.time + 1, lastPoint.time - 1]);

                let status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('124 ms');
                expect(status.relativeDelta).to.be('2%');

                return waitForComponentsToRender(context);
            });
        });

        it('should report the current value but not the relative delta when there is a selection with exaclyt one point', () => {
            const context = new BrowsingContext();
            let chart;
            let evaluator;
            return importEvaluator(context).then((ChartStatusEvaluator) => {
                chart = ChartTest.createInteractiveChartWithSampleCluster(context);
                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();

                const requests = context.symbols.MockRemoteAPI.requests;
                expect(requests.length).to.be(1);
                ChartTest.respondWithSampleCluster(requests[0]);

                const metric = makeMetric(context, 'Time');
                evaluator = new ChartStatusEvaluator(chart, metric);
                expect(evaluator.status()).to.be(null);

                return waitForComponentsToRender(context);
            }).then(() => {
                expect(evaluator.status()).to.be(null);

                const currentView = chart.sampledTimeSeriesData('current');
                const firstPoint = currentView.firstPoint();
                chart.setSelection([firstPoint.time + 1, currentView.nextPoint(firstPoint).time + 1]);

                let status = evaluator.status();
                expect(status).to.not.be(null);
                expect(status.comparison).to.be(null);
                expect(status.label).to.be(null);
                expect(status.currentValue).to.be('122 ms');
                expect(status.relativeDelta).to.be(null);

                return waitForComponentsToRender(context);
            });
        });

    });

});
