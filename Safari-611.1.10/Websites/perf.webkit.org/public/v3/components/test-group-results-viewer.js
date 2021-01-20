
class TestGroupResultsViewer extends ComponentBase {
    constructor()
    {
        super('test-group-results-table');
        this._analysisResults = null;
        this._testGroup = null;
        this._startPoint = null;
        this._endPoint = null;
        this._currentMetric = null;
        this._expandedTests = new Set;
        this._barGraphCellMap = new Map;
        this._renderResultsTableLazily = new LazilyEvaluatedFunction(this._renderResultsTable.bind(this));
        this._renderCurrentMetricsLazily = new LazilyEvaluatedFunction(this._renderCurrentMetrics.bind(this));
    }

    setTestGroup(currentTestGroup)
    {
        this._testGroup = currentTestGroup;
        this.enqueueToRender();
    }

    setAnalysisResults(analysisResults, metric)
    {
        this._analysisResults = analysisResults;
        this._currentMetric = metric;
        if (metric) {
            const path = metric.test().path();
            for (let i = path.length - 2; i >= 0; i--)
                this._expandedTests.add(path[i]);
        }

        this.enqueueToRender();
    }

    render()
    {
        if (!this._testGroup || !this._analysisResults)
            return;

        this._renderResultsTableLazily.evaluate(this._testGroup, this._expandedTests,
            ...this._analysisResults.topLevelTestsForTestGroup(this._testGroup));
        this._renderCurrentMetricsLazily.evaluate(this._currentMetric);
    }

    _renderResultsTable(testGroup, expandedTests, ...tests)
    {
        let maxDepth = 0;
        for (const test of expandedTests)
            maxDepth = Math.max(maxDepth, test.path().length);

        const element = ComponentBase.createElement;
        this.renderReplace(this.content('results'), [
            element('thead', [
                element('tr', [
                    element('th', {colspan: maxDepth + 1}, 'Test'),
                    element('th', {class: 'metric-direction'}, ''),
                    element('th', {colspan: 2}, 'Results'),
                    element('th', 'Averages'),
                    element('th', 'Comparison by mean'),
                    element('th', 'Comparison by individual iterations')
                ]),
            ]),
            tests.map((test) => this._buildRowsForTest(testGroup, expandedTests, test, [], maxDepth, 0))]);
    }

    _buildRowsForTest(testGroup, expandedTests, test, sharedPath, maxDepth, depth)
    {
        if (!this._analysisResults.containsTest(test))
            return [];

        const element = ComponentBase.createElement;
        const rows = element('tbody', test.metrics().map((metric) => this._buildRowForMetric(testGroup, metric, sharedPath, maxDepth, depth)));

        if (expandedTests.has(test)) {
            return [rows, test.childTests().map((childTest) => {
                return this._buildRowsForTest(testGroup, expandedTests, childTest, test.path(), maxDepth, depth + 1);
            })];
        }

        if (test.childTests().length) {
            const link = ComponentBase.createLink;
            return [rows, element('tr', {class: 'breakdown'}, [
                element('td', {colspan: maxDepth + 1}, link('(Breakdown)', () => {
                    this._expandedTests = new Set([...expandedTests, test]);
                    this.enqueueToRender();
                })),
                element('td', {colspan: 3}),
            ])];
        }

        return rows;
    }

    _buildRowForMetric(testGroup, metric, sharedPath, maxDepth, depth)
    {
        const commitSets = testGroup.requestedCommitSets();
        const valueMap = this._buildValueMap(testGroup, this._analysisResults.viewForMetric(metric));

        const formatter = metric.makeFormatter(4);
        const deltaFormatter = metric.makeFormatter(2, false);
        const formatValue = (value, interval) => {
            const delta = interval ? (interval[1] - interval[0]) / 2 : null;
            let result = value == null || isNaN(value) ? '-' : formatter(value);
            if (delta != null && !isNaN(delta))
                result += ` \u00b1 ${deltaFormatter(delta)}`;
            return result;
        }

        const barGroup = new BarGraphGroup();
        const barCells = [];
        const createConfigurationRow = (commitSet, previousCommitSet, barColor, meanIndicatorColor) => {
            const entry = valueMap.get(commitSet);
            const previousEntry = valueMap.get(previousCommitSet);

            const comparison = entry && previousEntry ? testGroup.compareTestResults(metric, previousEntry.filteredMeasurements, entry.filteredMeasurements) : null;
            const valueLabels = entry.measurements.map((measurement) => measurement ?  formatValue(measurement.value, measurement.interval) : '-');

            const barCell = element('td', {class: 'plot-bar'},
                element('div', barGroup.addBar(entry.allValues, valueLabels, entry.mean, entry.interval, barColor, meanIndicatorColor)));
            barCell.expandedHeight = +valueLabels.length + 'rem';
            barCells.push(barCell);

            const significanceForMean = comparison && comparison.isStatisticallySignificantForMean ? 'significant' : 'negligible';
            const significanceForIndividual = comparison && comparison.isStatisticallySignificantForIndividual ? 'significant' : 'negligible';
            const changeType = comparison ? comparison.changeType : null;
            return [
                element('th', testGroup.labelForCommitSet(commitSet)),
                barCell,
                element('td', formatValue(entry.mean, entry.interval)),
                element('td', {class: `comparison ${changeType} ${significanceForMean}`}, comparison ? comparison.fullLabelForMean : ''),
                element('td', {class: `comparison ${changeType} ${significanceForIndividual}`}, comparison ? comparison.fullLabelForIndividual : ''),
            ];
        };

        this._barGraphCellMap.set(metric, {barGroup, barCells});

        const rowspan = commitSets.length;
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        const metricName = metric.test().metrics().length == 1 ? metric.test().relativeName(sharedPath) : metric.relativeName(sharedPath);
        const onclick = this.createEventHandler((event) => {
            if (this._currentMetric == metric) {
                if (event.target.localName == 'bar-graph')
                    return;
                this._currentMetric = null;
            } else
                this._currentMetric = metric;
            this.enqueueToRender();
        });
        return [
            element('tr', {onclick}, [
                this._buildEmptyCells(depth, rowspan),
                element('th', {colspan: maxDepth - depth + 1, rowspan}, link(metricName, onclick)),
                element('td', {class: 'metric-direction', rowspan}, metric.isSmallerBetter() ? '\u21A4' : '\u21A6'),
                createConfigurationRow(commitSets[0], null, '#ddd', '#333')
            ]),
            commitSets.slice(1).map((commitSet, setIndex) => {
                return element('tr', {onclick},
                    createConfigurationRow(commitSet, commitSets[setIndex], '#aaa', '#000'));
            })
        ];
    }

    _buildValueMap(testGroup, resultsView)
    {
        const commitSets = testGroup.requestedCommitSets();
        const map = new Map;
        for (const commitSet of commitSets) {
            const requests = testGroup.requestsForCommitSet(commitSet);
            const measurements = requests.map((request) => resultsView.resultForRequest(request));
            const filteredMeasurements = measurements.filter((result) => !!result);
            const filteredValues = filteredMeasurements.map((measurement) => measurement.value);
            const allValues = measurements.map((result) => result != null ? result.value : NaN);
            const interval = Statistics.confidenceInterval(filteredValues);
            map.set(commitSet, {requests, measurements, filteredMeasurements, allValues, mean: Statistics.mean(filteredValues), interval});
        }
        return map;
    }

    _buildEmptyCells(count, rowspan)
    {
        const element = ComponentBase.createElement;
        const emptyCells = [];
        for (let i = 0; i < count; i++)
            emptyCells.push(element('td', {rowspan}, ''));
        return emptyCells;
    }

    _renderCurrentMetrics(currentMetric)
    {
        for (const entry of this._barGraphCellMap.values()) {
            for (const cell of entry.barCells) {
                cell.style.height = null;
                cell.parentNode.className = null;
            }
            entry.barGroup.setShowLabels(false);
        }

        const entry = this._barGraphCellMap.get(currentMetric);
        if (entry) {
            for (const cell of entry.barCells) {
                cell.style.height = cell.expandedHeight;
                cell.parentNode.className = 'selected';
            }
            entry.barGroup.setShowLabels(true);
        }
    }

    static htmlTemplate()
    {
        return `<table id="results"></table>`;
    }

    static cssTemplate()
    {
        return `
            table {
                border-collapse: collapse;
                margin: 0;
                padding: 0;
            }
            td, th {
                border: none;
                padding: 0;
                margin: 0;
                white-space: nowrap;
            }
            td:not(.metric-direction),
            th:not(.metric-direction) {
                padding: 0.1rem 0.5rem;
            }
            td:not(.metric-direction) {
                min-width: 2rem;
            }
            td.metric-direction {
                font-size: large;
            }
            bar-graph {
                width: 7rem;
                height: 1rem;
            }
            th {
                font-weight: inherit;
            }
            thead th {
                font-weight: inherit;
                color: #c93;
            }

            tr.selected > td,
            tr.selected > th {
                background: rgba(204, 153, 51, 0.05);
            }

            tr:first-child > td,
            tr:first-child > th {
                border-top: solid 1px #eee;
            }

            tbody th {
                text-align: left;
            }
            tbody th,
            tbody td {
                cursor: pointer;
            }
            a {
                color: inherit;
                text-decoration: inherit;
            }
            bar-graph {
                width: 100%;
                height: 100%;
            }
            td.plot-bar {
                position: relative;
                min-width: 7rem;
            }
            td.plot-bar > * {
                display: block;
                position: absolute;
                width: 100%;
                height: 100%;
                top: 0;
                left: 0;
            }
            .comparison {
                text-align: left;
            }
            .negligible {
                color: #999;
            }
            .significant.worse {
                color: #c33;
            }
            .significant.better {
                color: #33c;
            }
            tr.breakdown td {
                padding: 0;
                font-size: small;
                text-align: center;
            }
            tr.breakdown a {
                display: inline-block;
                text-decoration: none;
                color: #999;
                margin-bottom: 0.2rem;
            }
        `;
    }
}

ComponentBase.defineElement('test-group-results-viewer', TestGroupResultsViewer);
