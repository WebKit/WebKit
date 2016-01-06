
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

        return this._testGroup.requestedRootSets().map(function (rootSet, setIndex) {
            var rows = [new ResultsTableRow('Mean', rootSet)];
            var results = [];

            for (var request of testGroup.requestsForRootSet(rootSet)) {
                var result = request.result();
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

            return {heading: String.fromCharCode('A'.charCodeAt(0) + setIndex), rows:rows};
        })
    }
}

ComponentBase.defineElement('test-group-results-table', TestGroupResultsTable);
