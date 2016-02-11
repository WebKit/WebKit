
class TestGroupResultsTable extends ResultsTable {
    constructor()
    {
        super('test-group-results-table');
        this._testGroup = null;
        this._renderedTestGroup = null;
    }

    didUpdateResults() { this._renderedTestGroup = null; }
    setTestGroup(testGroup) { this._testGroup = testGroup; }

    heading()
    {
        return ComponentBase.createElement('th', {colspan: 2}, 'Configuration');
    }

    render()
    {
        if (this._renderedTestGroup == this._testGroup)
            return;
        this._renderedTestGroup = this._testGroup;
        super.render();
    }

    buildRowGroups()
    {
        var testGroup = this._testGroup;
        if (!testGroup)
            return [];

        var rootSets = this._testGroup.requestedRootSets();
        var groups = rootSets.map(function (rootSet) {
            var rows = [new ResultsTableRow('Mean', rootSet)];
            var results = [];

            for (var request of testGroup.requestsForRootSet(rootSet)) {
                var result = request.result();
                // Call result.rootSet() for each result since the set of revisions used in testing maybe different from requested ones.
                var row = new ResultsTableRow(1 + +request.order(), result ? result.rootSet() : null);
                rows.push(row);
                if (result) {
                    row.setLink(result.build().url(), result.build().label());
                    row.setResult(result);
                    results.push(result);
                } else
                    row.setLink(request.statusUrl(), request.statusLabel());
            }

            var aggregatedResult = MeasurementAdaptor.aggregateAnalysisResults(results);
            if (!isNaN(aggregatedResult.value))
                rows[0].setResult(aggregatedResult);

            return {heading: testGroup.labelForRootSet(rootSet), rows:rows};
        });

        var comparisonRows = [];
        for (var i = 0; i < rootSets.length; i++) {
            for (var j = i + 1; j < rootSets.length; j++) {
                var startConfig = testGroup.labelForRootSet(rootSets[i]);
                var endConfig = testGroup.labelForRootSet(rootSets[j]);

                var result = this._testGroup.compareTestResults(rootSets[i], rootSets[j]);
                if (result.status == 'pending' || result.status == 'running' || result.status == 'failed')
                    continue;

                var row = new ResultsTableRow(`${startConfig} to ${endConfig}`, null);
                row.setLabelForWholeRow(result.fullLabel);
                comparisonRows.push(row);
            }
        }

        groups.unshift({heading: '', rows: comparisonRows});

        return groups;
    }
}

ComponentBase.defineElement('test-group-results-table', TestGroupResultsTable);
