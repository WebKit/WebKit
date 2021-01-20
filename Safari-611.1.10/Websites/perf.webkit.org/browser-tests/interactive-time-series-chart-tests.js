
describe('InteractiveTimeSeriesChart', () => {

    it('should change the unlocked indicator to the point closest to the last mouse move position', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            let selectionChangeCount = 0;
            chart.listenToAction('selectionChange', () => selectionChangeCount++);

            let canvas;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([]);

                canvas = chart.content().querySelector('canvas');
                const rect = canvas.getBoundingClientRect();
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right - 1, clientY: rect.top + rect.height / 2, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator).to.not.be(null);
                const currentView = chart.sampledTimeSeriesData('current');
                const lastPoint = currentView.lastPoint();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(false);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, false]]);

                expect(selectionChangeCount).to.be(0);
            });
        });
    });

    it('should lock the indicator to the point closest to the clicked position', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            let selectionChangeCount = 0;
            chart.listenToAction('selectionChange', () => selectionChangeCount++);

            let canvas;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([]);
                canvas = chart.content().querySelector('canvas');
                const rect = canvas.getBoundingClientRect();

                const x = rect.right - 1;
                const y = rect.top + rect.height / 2;
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: x, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousedown', {target: canvas, clientX: x, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: x - 0.5, clientY: y + 0.5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mouseup', {target: canvas, clientX: x - 0.5, clientY: y + 0.5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: x - 0.5, clientY: y + 0.5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                const currentView = chart.sampledTimeSeriesData('current');
                const lastPoint = currentView.lastPoint();
                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(true);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, false], [lastPoint.id, true]]);

                expect(selectionChangeCount).to.be(0);
            });
        });
    });

    it('should clear the unlocked indicator when the mouse cursor exits the chart', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            let selectionChangeCount = 0;
            chart.listenToAction('selectionChange', () => selectionChangeCount++);

            let canvas;
            let rect;
            let lastPoint;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([]);

                canvas = chart.content().querySelector('canvas');
                rect = canvas.getBoundingClientRect();
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right - 1, clientY: rect.top + rect.height / 2, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                const currentView = chart.sampledTimeSeriesData('current');
                lastPoint = currentView.lastPoint();
                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(false);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, false]]);

                canvas.parentNode.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right + 50, clientY: rect.bottom + 50, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mouseleave', {target: canvas, clientX: rect.right + 50, clientY: rect.bottom + 50, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, false], [null, false]]);

                expect(selectionChangeCount).to.be(0);
            });
        });
    });

    it('should not clear the locked indicator when the mouse cursor exits the chart', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            let selectionChangeCount = 0;
            chart.listenToAction('selectionChange', () => selectionChangeCount++);

            let canvas;
            let rect;
            let currentView;
            let lastPoint;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([]);

                canvas = chart.content().querySelector('canvas');
                rect = canvas.getBoundingClientRect();
                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.right - 1, clientY: rect.top + rect.height / 2, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                currentView = chart.sampledTimeSeriesData('current');
                lastPoint = currentView.lastPoint();
                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(true);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, true]]);

                canvas.parentNode.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right + 50, clientY: rect.bottom + 50, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mouseleave', {target: canvas, clientX: rect.right + 50, clientY: rect.bottom + 50, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);

                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(true);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, true]]);

                expect(selectionChangeCount).to.be(0);
            })
        });
    });

    it('should clear the locked indicator when clicked', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            let selectionChangeCount = 0;
            chart.listenToAction('selectionChange', () => selectionChangeCount++);

            let canvas;
            let rect;
            let y;
            let currentView;
            let lastPoint;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(indicatorChangeCalls).to.be.eql([]);

                canvas = chart.content().querySelector('canvas');
                rect = canvas.getBoundingClientRect();
                y = rect.top + rect.height / 2;
                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.right - 1, clientY: y, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                currentView = chart.sampledTimeSeriesData('current');
                lastPoint = currentView.lastPoint();
                expect(chart.currentSelection()).to.be(null);
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(lastPoint);
                expect(indicator.isLocked).to.be(true);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, true]]);

                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.left + 1, clientY: y, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.be(null);
                const firstPoint = currentView.firstPoint();
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(firstPoint);
                expect(indicator.isLocked).to.be(false);
                expect(indicatorChangeCalls).to.be.eql([[lastPoint.id, true], [firstPoint.id, false]]);

                expect(selectionChangeCount).to.be(0);
            })
        });
    });

    it('should change the selection when the mouse cursor is dragged', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context, null, {selection: {lineStyle: '#f93', lineWidth: 2, fillStyle: '#ccc'}});

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const indicatorChangeCalls = [];
            chart.listenToAction('indicatorChange', (...args) => indicatorChangeCalls.push(args));

            const selectionChangeCalls = [];
            chart.listenToAction('selectionChange', (...args) => selectionChangeCalls.push(args));

            const zoomButton = chart.content('zoom-button');

            let canvas;
            let rect;
            let y;
            let currentView;
            let firstPoint;
            let oldRange;
            let newRange;
            return waitForComponentsToRender(context).then(() => {
                expect(chart.currentSelection()).to.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(selectionChangeCalls).to.be.eql([]);

                canvas = chart.content().querySelector('canvas');
                rect = canvas.getBoundingClientRect();
                y = rect.top + rect.height / 2;
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                currentView = chart.sampledTimeSeriesData('current');
                firstPoint = currentView.firstPoint();
                expect(chart.currentSelection()).to.be(null);
                let indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(firstPoint);
                expect(indicator.isLocked).to.be(false);
                expect(indicatorChangeCalls).to.be.eql([[firstPoint.id, false]]);
                expect(zoomButton.offsetHeight).to.be(0);

                canvas.dispatchEvent(new MouseEvent('mousedown', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);

                expect(chart.currentSelection()).to.be(null);
                let indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(firstPoint);
                expect(indicator.isLocked).to.be(false);
                expect(selectionChangeCalls).to.be.eql([]);
                expect(indicatorChangeCalls).to.be.eql([[firstPoint.id, false]]);
                expect(zoomButton.offsetHeight).to.be(0);

                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.left + 15, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.not.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(selectionChangeCalls.length).to.be(1);
                oldRange = selectionChangeCalls[0][0];
                expect(oldRange).to.be.eql(chart.currentSelection());
                expect(selectionChangeCalls[0][1]).to.be(false);
                expect(indicatorChangeCalls).to.be.eql([[firstPoint.id, false], [null, false]]);
                expect(zoomButton.offsetHeight).to.be(0);

                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right - 5, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.not.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(selectionChangeCalls.length).to.be(2);
                newRange = selectionChangeCalls[1][0];
                expect(newRange).to.be.eql(chart.currentSelection());
                expect(newRange[0]).to.be(oldRange[0]);
                expect(newRange[1]).to.be.greaterThan(oldRange[1]);
                expect(selectionChangeCalls[1][1]).to.be(false);
                expect(zoomButton.offsetHeight).to.be(0);

                canvas.dispatchEvent(new MouseEvent('mouseup', {target: canvas, clientX: rect.right - 5, clientY: y + 5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.right - 5, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.be.eql(newRange);
                expect(chart.currentIndicator()).to.be(null);
                expect(selectionChangeCalls.length).to.be(3);
                expect(selectionChangeCalls[2][0]).to.be.eql(newRange);
                expect(selectionChangeCalls[2][1]).to.be(true);
                expect(zoomButton.offsetHeight).to.be(0);
            });
        });
    });

    it('should dispatch the "zoom" action when the zoom button is clicked', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context, null, {selection: {lineStyle: '#f93', lineWidth: 2, fillStyle: '#ccc'}, zoomButton: true});

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const zoomCalls = [];
            chart.listenToAction('zoom', (...args) => zoomCalls.push(args));
            const zoomButton = chart.content('zoom-button');

            let selection;
            let canvas;
            return waitForComponentsToRender(context).then(() => {
                expect(zoomButton.offsetHeight).to.be(0);
                canvas = chart.content().querySelector('canvas');
                const rect = canvas.getBoundingClientRect();
                const y = rect.top + rect.height / 2;
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousedown', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right - 10, clientY: y + 5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mouseup', {target: canvas, clientX: rect.right - 10, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                selection = chart.currentSelection();
                expect(selection).to.not.be(null);
                expect(chart.currentIndicator()).to.be(null);
                expect(zoomButton.offsetHeight).to.not.be(0);
                expect(zoomCalls).to.be.eql([]);
                zoomButton.click();
            }).then(() => {
                expect(zoomCalls).to.be.eql([[selection]]);

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);
            });
        });
    });

    it('should clear the selection when clicked', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context, null, {selection: {lineStyle: '#f93', lineWidth: 2, fillStyle: '#ccc'}});

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            let canvas;
            let rect;
            let y;
            return waitForComponentsToRender(context).then(() => {
                canvas = chart.content().querySelector('canvas');
                rect = canvas.getBoundingClientRect();
                y = rect.top + rect.height / 2;
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousedown', {target: canvas, clientX: rect.left + 5, clientY: y, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mousemove', {target: canvas, clientX: rect.right - 10, clientY: y + 5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('mouseup', {target: canvas, clientX: rect.right - 10, clientY: y + 5, composed: true, bubbles: true}));
                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.right - 10, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.not.be(null);
                expect(chart.currentIndicator()).to.be(null);

                canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: rect.left + 1, clientY: y + 5, composed: true, bubbles: true}));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.currentSelection()).to.be(null);
                const currentView = chart.sampledTimeSeriesData('current');
                const indicator = chart.currentIndicator();
                expect(indicator.view).to.be(currentView);
                expect(indicator.point).to.be(currentView.firstPoint());
                expect(indicator.isLocked).to.be(false);
            });
        });
    });

    it('should dispatch "annotationClick" action when an annotation is clicked', () => {
        const context = new BrowsingContext();
        return ChartTest.importChartScripts(context).then(() => {
            const chart = ChartTest.createInteractiveChartWithSampleCluster(context, null,
                {annotations: { textStyle: '#000', textBackground: '#fff', minWidth: 3, barHeight: 10, barSpacing: 1, fillStyle: '#ccc'}});

            chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
            chart.fetchMeasurementSets();
            ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

            const diff = ChartTest.sampleCluster.endTime - ChartTest.sampleCluster.startTime;
            const annotations = [{
                startTime: ChartTest.sampleCluster.startTime + diff / 2,
                endTime: ChartTest.sampleCluster.endTime - diff / 4,
                label: 'hello, world',
            }];
            chart.setAnnotations(annotations);

            const annotationClickCalls = [];
            chart.listenToAction('annotationClick', (...args) => annotationClickCalls.push(args));

            let canvas;
            let init;
            return waitForComponentsToRender(context).then(() => {
                expect(annotationClickCalls).to.be.eql([]);
                expect(chart.content('annotation-label').textContent).to.not.contain('hello, world');

                canvas = chart.content().querySelector('canvas');
                const rect = canvas.getBoundingClientRect();
                init = {target: canvas, clientX: rect.right - rect.width / 4, clientY: rect.bottom - 5, composed: true, bubbles: true};
                canvas.dispatchEvent(new MouseEvent('mousemove', init));

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                expect(chart.content('annotation-label').textContent).to.contain('hello, world');
                expect(annotationClickCalls).to.be.eql([]);
                canvas.dispatchEvent(new MouseEvent('mousedown', init));
                canvas.dispatchEvent(new MouseEvent('mouseup', init));
                canvas.dispatchEvent(new MouseEvent('click', init));

                expect(annotationClickCalls).to.be.eql([[annotations[0]]]);

                CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);
            });
        });
    });

    describe('render', () => {
        it('should render the unlocked indicator when options.indicator is specified', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chartWithoutIndicator = ChartTest.createInteractiveChartWithSampleCluster(context);
                const chartWithIndicator = ChartTest.createInteractiveChartWithSampleCluster(context, null,
                    {indicator: {lineStyle: 'rgb(51, 204, 255)', lineWidth: 2, pointRadius: 2}, interactiveChart: true});
                const indicatorColor = {r: 51, g: 204, b: 255};

                chartWithoutIndicator.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithoutIndicator.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                chartWithIndicator.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithIndicator.fetchMeasurementSets();

                let canvasWithoutIndicator;
                let canvasWithIndicator;
                return waitForComponentsToRender(context).then(() => {
                    canvasWithoutIndicator = chartWithoutIndicator.content().querySelector('canvas');
                    canvasWithIndicator = chartWithIndicator.content().querySelector('canvas');

                    const rect = canvasWithIndicator.getBoundingClientRect();
                    const x = rect.right - 1;
                    const y = rect.top + rect.height / 2;
                    canvasWithIndicator.dispatchEvent(new MouseEvent('mousemove', {target: canvasWithIndicator, clientX: x, clientY: y, composed: true, bubbles: true}));

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithIndicator);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithIndicator)).to.be(true);

                    const indicator = chartWithIndicator.currentIndicator();
                    const currentView = chartWithIndicator.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.be(currentView.lastPoint());
                    expect(indicator.isLocked).to.be(false);

                    CanvasTest.expectCanvasesMismatch(canvasWithoutIndicator, canvasWithIndicator);
                    expect(CanvasTest.canvasContainsColor(canvasWithoutIndicator, indicatorColor)).to.be(false);
                    expect(CanvasTest.canvasContainsColor(canvasWithIndicator, indicatorColor)).to.be(true);
                });
            });
        });

        it('should render the locked indicator differently from the unlocked indicator when options.lockedIndicator is specified', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chartOptions = {
                    indicator: {lineStyle: 'rgb(51, 204, 255)', lineWidth: 2, pointRadius: 3},
                    lockedIndicator: {lineStyle: 'rgb(51, 102, 204)', fillStyle: 'rgb(250, 250, 250)', lineWidth: 2, pointRadius: 3}
                };
                const unlockedColor = {r: 51, g: 204, b: 255};
                const lockedColor = {r: 51, g: 102, b: 204};
                const lockedFillColor = {r: 250, g: 250, b: 250};
                const chartWithUnlockedIndicator = ChartTest.createInteractiveChartWithSampleCluster(context, null, chartOptions);
                const chartWithLockedIndicator = ChartTest.createInteractiveChartWithSampleCluster(context, null, chartOptions);

                chartWithUnlockedIndicator.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithUnlockedIndicator.fetchMeasurementSets();
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                chartWithLockedIndicator.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chartWithLockedIndicator.fetchMeasurementSets();

                let canvasWithUnlockedIndicator;
                let canvasWithLockedIndicator;
                return waitForComponentsToRender(context).then(() => {
                    canvasWithUnlockedIndicator = chartWithUnlockedIndicator.content().querySelector('canvas');
                    canvasWithLockedIndicator = chartWithLockedIndicator.content().querySelector('canvas');

                    const rect = canvasWithUnlockedIndicator.getBoundingClientRect();
                    const x = rect.right - 1;
                    const y = rect.top + rect.height / 2;
                    canvasWithUnlockedIndicator.dispatchEvent(new MouseEvent('mousemove', {target: canvasWithUnlockedIndicator, clientX: x, clientY: y, composed: true, bubbles: true}));
                    canvasWithLockedIndicator.dispatchEvent(new MouseEvent('click', {target: canvasWithLockedIndicator, clientX: x, clientY: y, composed: true, bubbles: true}));

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithUnlockedIndicator);
                    CanvasTest.fillCanvasBeforeRedrawCheck(canvasWithLockedIndicator);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithUnlockedIndicator)).to.be(true);
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvasWithLockedIndicator)).to.be(true);

                    let indicator = chartWithUnlockedIndicator.currentIndicator();
                    let currentView = chartWithUnlockedIndicator.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.be(currentView.lastPoint());
                    expect(indicator.isLocked).to.be(false);

                    indicator = chartWithLockedIndicator.currentIndicator();
                    currentView = chartWithLockedIndicator.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.be(currentView.lastPoint());
                    expect(indicator.isLocked).to.be(true);

                    CanvasTest.expectCanvasesMismatch(canvasWithUnlockedIndicator, canvasWithLockedIndicator);
                    expect(CanvasTest.canvasContainsColor(canvasWithUnlockedIndicator, unlockedColor)).to.be(true);
                    expect(CanvasTest.canvasContainsColor(canvasWithUnlockedIndicator, lockedFillColor)).to.be(false);
                    expect(CanvasTest.canvasContainsColor(canvasWithLockedIndicator, lockedColor)).to.be(true);
                    expect(CanvasTest.canvasContainsColor(canvasWithLockedIndicator, lockedFillColor)).to.be(true);
                });
            });
        });
    });

    describe('moveLockedIndicatorWithNotification', () => {
        it('should move the locked indicator to the right when forward boolean is true', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();
                let indicatorChangeCount = 0;
                chart.listenToAction('indicatorChange', () => indicatorChangeCount++);
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                let canvas;
                return waitForComponentsToRender(context).then(() => {
                    expect(indicatorChangeCount).to.be(0);

                    canvas = chart.content().querySelector('canvas');

                    const rect = canvas.getBoundingClientRect();
                    const x = rect.left + 1;
                    const y = rect.top + rect.height / 2;
                    canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: x, clientY: y, composed: true, bubbles: true}));

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                    expect(indicatorChangeCount).to.be(1);

                    let indicator = chart.currentIndicator();
                    let currentView = chart.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.be(currentView.firstPoint());
                    expect(indicator.isLocked).to.be(true);

                    chart.moveLockedIndicatorWithNotification(true);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                    expect(indicatorChangeCount).to.be(2);

                    let indicator = chart.currentIndicator();
                    let currentView = chart.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.not.be(currentView.firstPoint());
                    expect(indicator.point).to.be(currentView.nextPoint(currentView.firstPoint()));
                    expect(currentView.previousPoint(indicator.point)).to.be(currentView.firstPoint());
                    expect(indicator.isLocked).to.be(true);
                    expect(indicatorChangeCount).to.be(2);
                });
            });
        });

        it('should move the locked indicator to the left when forward boolean is false', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

                chart.setDomain(ChartTest.sampleCluster.startTime, ChartTest.sampleCluster.endTime);
                chart.fetchMeasurementSets();
                let indicatorChangeCount = 0;
                chart.listenToAction('indicatorChange', () => indicatorChangeCount++);
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                let canvas;
                return waitForComponentsToRender(context).then(() => {
                    expect(indicatorChangeCount).to.be(0);

                    canvas = chart.content().querySelector('canvas');

                    const rect = canvas.getBoundingClientRect();
                    const x = rect.right - 1;
                    const y = rect.top + rect.height / 2;
                    canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: x, clientY: y, composed: true, bubbles: true}));

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                    expect(indicatorChangeCount).to.be(1);

                    let indicator = chart.currentIndicator();
                    let currentView = chart.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.be(currentView.lastPoint());
                    expect(indicator.isLocked).to.be(true);

                    chart.moveLockedIndicatorWithNotification(false);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                    expect(indicatorChangeCount).to.be(2);

                    let indicator = chart.currentIndicator();
                    let currentView = chart.sampledTimeSeriesData('current');
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point).to.not.be(currentView.firstPoint());
                    expect(indicator.point).to.be(currentView.previousPoint(currentView.lastPoint()));
                    expect(currentView.nextPoint(indicator.point)).to.be(currentView.lastPoint());
                    expect(indicator.isLocked).to.be(true);
                    expect(indicatorChangeCount).to.be(2);
                });
            });
        });

        it('should not move the locked indicator when there are no points within the domain', () => {
            const context = new BrowsingContext();
            return ChartTest.importChartScripts(context).then(() => {
                const chart = ChartTest.createInteractiveChartWithSampleCluster(context);

                // The domain inclues points 2, 3
                chart.setDomain(posixTime('2016-01-05T20:00:00Z'), posixTime('2016-01-06T00:00:00Z'));
                chart.fetchMeasurementSets();
                let indicatorChangeCount = 0;
                chart.listenToAction('indicatorChange', () => indicatorChangeCount++);
                ChartTest.respondWithSampleCluster(context.symbols.MockRemoteAPI.requests[0]);

                let canvas;
                let currentView;
                return waitForComponentsToRender(context).then(() => {
                    expect(indicatorChangeCount).to.be(0);

                    canvas = chart.content().querySelector('canvas');

                    const rect = canvas.getBoundingClientRect();
                    const x = rect.right - 1;
                    const y = rect.top + rect.height / 2;
                    canvas.dispatchEvent(new MouseEvent('click', {target: canvas, clientX: x, clientY: y, composed: true, bubbles: true}));

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);
                    expect(indicatorChangeCount).to.be(1);

                    currentView = chart.sampledTimeSeriesData('current');
                    expect(currentView.length()).to.be(4); // points 0 and 4 are added to draw lines extending beyond the domain.
                    expect([...currentView].map((point) => point.id)).to.be.eql([1000, 1002, 1003, 1004]);

                    const indicator = chart.currentIndicator();
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point.id).to.be(1003);
                    expect(indicator.isLocked).to.be(true);

                    chart.moveLockedIndicatorWithNotification(true);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);

                    expect(indicatorChangeCount).to.be(1);
                    const indicator = chart.currentIndicator();
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point.id).to.be(1003);
                    expect(indicator.isLocked).to.be(true);

                    chart.moveLockedIndicatorWithNotification(false);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(true);

                    expect(indicatorChangeCount).to.be(2);
                    const indicator = chart.currentIndicator();
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point.id).to.be(1002);
                    expect(indicator.isLocked).to.be(true);

                    chart.moveLockedIndicatorWithNotification(false);

                    CanvasTest.fillCanvasBeforeRedrawCheck(canvas);
                    return waitForComponentsToRender(context);
                }).then(() => {
                    expect(CanvasTest.hasCanvasBeenRedrawn(canvas)).to.be(false);

                    expect(indicatorChangeCount).to.be(2);
                    const indicator = chart.currentIndicator();
                    expect(indicator.view).to.be(currentView);
                    expect(indicator.point.id).to.be(1002);
                    expect(indicator.isLocked).to.be(true);
                });
            });
        });

    });
});
