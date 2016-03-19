
class AnalysisResultsViewer extends ResultsTable {
    constructor()
    {
        super('analysis-results-viewer');
        this._startPoint = null;
        this._endPoint = null;
        this._testGroups = null;
        this._currentTestGroup = null;
        this._renderedCurrentTestGroup = null;
        this._shouldRenderTable = true;
        this._additionalHeading = null;
        this._testGroupCallback = null;
        this._expandedPoints = new Set;
    }

    setTestGroupCallback(callback) { this._testGroupCallback = callback; }

    setCurrentTestGroup(testGroup)
    {
        this._currentTestGroup = testGroup;
    }

    setPoints(startPoint, endPoint)
    {
        this._startPoint = startPoint;
        this._endPoint = endPoint;
        this._shouldRenderTable = true;
        this._expandedPoints.clear();
        this._expandedPoints.add(startPoint);
        this._expandedPoints.add(endPoint);
    }

    setTestGroups(testGroups)
    {
        this._testGroups = testGroups;
        this._shouldRenderTable = true;
    }

    didUpdateResults()
    {
        this._shouldRenderTable = true;
    }

    render()
    {
        if (!this._valueFormatter || !this._startPoint)
            return;

        Instrumentation.startMeasuringTime('AnalysisResultsViewer', 'render');

        if (this._shouldRenderTable) {
            this._shouldRenderTable = false;
            this._renderedCurrentTestGroup = null;

            Instrumentation.startMeasuringTime('AnalysisResultsViewer', 'renderTable');
            super.render();
            Instrumentation.endMeasuringTime('AnalysisResultsViewer', 'renderTable');
        }

        if (this._currentTestGroup != this._renderedCurrentTestGroup) {
            if (this._renderedCurrentTestGroup) {
                var className = this._classForTestGroup(this._renderedCurrentTestGroup);
                var element = this.content().querySelector('.' + className);
                if (element)
                    element.classList.remove('selected');
            }
            if (this._currentTestGroup) {
                var className = this._classForTestGroup(this._currentTestGroup);
                var element = this.content().querySelector('.' + className);
                if (element)
                    element.classList.add('selected');
            }
            this._renderedCurrentTestGroup = this._currentTestGroup;
        }

        Instrumentation.endMeasuringTime('AnalysisResultsViewer', 'render');
    }

    heading() { return [ComponentBase.createElement('th', 'Point')]; }
    additionalHeading() { return this._additionalHeading; }

    buildRowGroups()
    {
        Instrumentation.startMeasuringTime('AnalysisResultsViewer', 'buildRowGroups');

        var testGroups = this._testGroups || [];
        var rootSetsInTestGroups = this._collectRootSetsInTestGroups(testGroups);

        var rowToMatchingRootSets = new Map;
        var rowList = this._buildRowsForPointsAndTestGroups(rootSetsInTestGroups, rowToMatchingRootSets);

        var testGroupLayoutMap = new Map;
        var self = this;
        rowList.forEach(function (row, rowIndex) {
            var matchingRootSets = rowToMatchingRootSets.get(row);
            if (!matchingRootSets) {
                console.assert(row instanceof AnalysisResultsViewer.ExpandableRow);
                return;
            }

            for (var entry of matchingRootSets) {
                var testGroup = entry.testGroup();

                var block = testGroupLayoutMap.get(testGroup);
                if (!block) {
                    block = new AnalysisResultsViewer.TestGroupStackingBlock(
                        testGroup, self._classForTestGroup(testGroup), self._openStackingBlock.bind(self, testGroup));
                    testGroupLayoutMap.set(testGroup, block);
                }
                block.addRowIndex(entry, rowIndex);
            }
        });

        var grid = new AnalysisResultsViewer.TestGroupStackingGrid(rowList.length);
        for (var testGroup of testGroups) {
            var block = testGroupLayoutMap.get(testGroup);
            if (block)
                grid.insertBlockToColumn(block);
        }

        grid.layout();
        for (var rowIndex = 0; rowIndex < rowList.length; rowIndex++)
            rowList[rowIndex].setAdditionalColumns(grid.createCellsForRow(rowIndex));

        this._additionalHeading = grid._columns ? ComponentBase.createElement('td', {colspan: grid._columns.length + 1, class: 'stacking-block'}) : [];

        Instrumentation.endMeasuringTime('AnalysisResultsViewer', 'buildRowGroups');

        return [{rows: rowList}];
    }

    _collectRootSetsInTestGroups(testGroups)
    {
        if (!this._testGroups)
            return [];

        var rootSetsInTestGroups = [];
        for (var group of this._testGroups) {
            var sortedSets = group.requestedRootSets();
            for (var i = 0; i < sortedSets.length; i++)
                rootSetsInTestGroups.push(new AnalysisResultsViewer.RootSetInTestGroup(group, sortedSets[i], sortedSets[i + 1]));
        }

        return rootSetsInTestGroups;
    }

    _buildRowsForPointsAndTestGroups(rootSetsInTestGroups, rowToMatchingRootSets)
    {
        console.assert(this._startPoint.series == this._endPoint.series);
        var rowList = [];
        var pointAfterEnd = this._endPoint.series.nextPoint(this._endPoint);
        var rootSetsWithPoints = new Set;
        var pointIndex = 0;
        var previousPoint;
        for (var point = this._startPoint; point && point != pointAfterEnd; point = point.series.nextPoint(point), pointIndex++) {
            var rootSetInPoint = point.rootSet();
            var matchingRootSets = [];
            for (var entry of rootSetsInTestGroups) {
                if (rootSetInPoint.equals(entry.rootSet()) && !rootSetsWithPoints.has(entry)) {
                    matchingRootSets.push(entry);
                    rootSetsWithPoints.add(entry);
                }
            }

            var hasMatchingTestGroup = !!matchingRootSets.length;
            if (!hasMatchingTestGroup && !this._expandedPoints.has(point))
                continue;

            var row = new ResultsTableRow(pointIndex.toString(), rootSetInPoint);
            row.setResult(point);

            if (previousPoint && previousPoint.series.nextPoint(previousPoint) != point)
                rowList.push(new AnalysisResultsViewer.ExpandableRow(this._expandBetween.bind(this, previousPoint, point)));
            previousPoint = point;

            rowToMatchingRootSets.set(row, matchingRootSets);
            rowList.push(row);
        }

        rootSetsInTestGroups.forEach(function (entry) {
            if (rootSetsWithPoints.has(entry))
                return;

            for (var i = 0; i < rowList.length; i++) {
                var row = rowList[i];
                if (!(row instanceof AnalysisResultsViewer.ExpandableRow) && row.rootSet().equals(entry.rootSet())) {
                    rowToMatchingRootSets.get(row).push(entry);
                    return;
                }
            }

            var groupTime = entry.rootSet().latestCommitTime();
            var newRow = new ResultsTableRow(null, entry.rootSet());
            rowToMatchingRootSets.set(newRow, [entry]);

            for (var i = 0; i < rowList.length; i++) {
                if (rowList[i] instanceof AnalysisResultsViewer.ExpandableRow)
                    continue;

                if (rowList[i].rootSet().equals(entry.succeedingRootSet())) {
                    rowList.splice(i, 0, newRow);
                    return;
                }

                var rowTime = rowList[i].rootSet().latestCommitTime();
                if (rowTime > groupTime) {
                    rowList.splice(i, 0, newRow);
                    return;
                }

                if (rowTime == groupTime) {
                    // Missing some commits. Do as best as we can to avoid going backwards in time.
                    var repositoriesInNewRow = entry.rootSet().repositories();
                    for (var j = i; j < rowList.length; j++) {
                        if (rowList[j] instanceof AnalysisResultsViewer.ExpandableRow)
                            continue;
                        for (var repository of repositoriesInNewRow) {
                            var newCommit = entry.rootSet().commitForRepository(repository);
                            var rowCommit = rowList[j].rootSet().commitForRepository(repository);
                            if (!rowCommit || newCommit.time() < rowCommit.time()) {
                                rowList.splice(j, 0, newRow);
                                return;
                            }
                        }
                    }
                }
            }

            var newRow = new ResultsTableRow(null, entry.rootSet());
            rowToMatchingRootSets.set(newRow, [entry]);
            rowList.push(newRow);
        });

        return rowList;
    }

    _classForTestGroup(testGroup)
    {
        return 'stacked-test-group-' + testGroup.id();
    }

    _openStackingBlock(testGroup)
    {
        if (this._testGroupCallback)
            this._testGroupCallback(testGroup);
    }
    
    _expandBetween(pointBeforeExpansion, pointAfterExpansion)
    {
        console.assert(pointBeforeExpansion.series == pointAfterExpansion.series);
        var indexBeforeStart = pointBeforeExpansion.seriesIndex;
        var indexAfterEnd = pointAfterExpansion.seriesIndex;
        console.assert(indexBeforeStart + 1 < indexAfterEnd);

        var series = pointAfterExpansion.series;
        var increment = Math.ceil((indexAfterEnd - indexBeforeStart) / 5);
        if (increment < 3)
            increment = 1;
        for (var i = indexBeforeStart + 1; i < indexAfterEnd; i += increment)
            this._expandedPoints.add(series.findPointByIndex(i));
        this._shouldRenderTable = true;
        this.render();
    }

    static htmlTemplate()
    {
        return `<section class="analysis-view">${ResultsTable.htmlTemplate()}</section>`;
    }

    static cssTemplate()
    {
        return ResultsTable.cssTemplate() + `
            .analysis-view .stacking-block {
                position: relative;
                border: solid 1px #fff;
                cursor: pointer;
            }

            .analysis-view .stacking-block a {
                display: block;
                text-decoration: none;
                color: inherit;
                font-size: 0.8rem;
                padding: 0 0.1rem;
                max-width: 3rem;
            }

            .analysis-view .stacking-block:not(.failed) {
                color: black;
                opacity: 1;
            }

            .analysis-view .stacking-block.selected,
            .analysis-view .stacking-block:hover {
                text-decoration: underline;
            }

            .analysis-view .stacking-block.selected:before {
                content: '';
                position: absolute;
                left: 0px;
                top: 0px;
                width: calc(100% - 2px);
                height: calc(100% - 2px);
                border: solid 1px #333;
            }

            .analysis-view .stacking-block.failed {
                background: rgba(128, 51, 128, 0.5);
            }
            .analysis-view .stacking-block.unchanged {
                background: rgba(128, 128, 128, 0.5);
            }
            .analysis-view .stacking-block.pending {
                background: rgba(204, 204, 51, 0.2);
            }
            .analysis-view .stacking-block.running {
                background: rgba(204, 204, 51, 0.5);
            }
            .analysis-view .stacking-block.worse {
                background: rgba(255, 102, 102, 0.5);
            }
            .analysis-view .stacking-block.better {
                background: rgba(102, 102, 255, 0.5);
            }

            .analysis-view .point-label-with-expansion-link {
                font-size: 0.7rem;
            }
            .analysis-view .point-label-with-expansion-link a {
                color: #999;
                text-decoration: none;
            }
        `;
    }
}

ComponentBase.defineElement('analysis-results-viewer', AnalysisResultsViewer);

AnalysisResultsViewer.ExpandableRow = class extends ResultsTableRow {
    constructor(callback)
    {
        super(null, null);
        this._callback = callback;
    }

    resultContent() { return ''; }

    heading()
    {
        return ComponentBase.createElement('span', {class: 'point-label-with-expansion-link'}, [
            ComponentBase.createLink('(Expand)', 'Expand', this._callback),
        ]);
    }
}

AnalysisResultsViewer.RootSetInTestGroup = class {
    constructor(testGroup, rootSet, succeedingRootSet)
    {
        console.assert(testGroup instanceof TestGroup);
        console.assert(rootSet instanceof RootSet);
        this._testGroup = testGroup;
        this._rootSet = rootSet;
        this._succeedingRootSet = succeedingRootSet;
    }

    testGroup() { return this._testGroup; }
    rootSet() { return this._rootSet; }
    succeedingRootSet() { return this._succeedingRootSet; }
}

AnalysisResultsViewer.TestGroupStackingBlock = class {
    constructor(testGroup, className, callback)
    {
        this._testGroup = testGroup;
        this._rootSetIndexRowIndexMap = [];
        this._className = className;
        this._label = null;
        this._title = null;
        this._status = null;
        this._callback = callback;
    }

    addRowIndex(rootSetInTestGroup, rowIndex)
    {
        console.assert(rootSetInTestGroup instanceof AnalysisResultsViewer.RootSetInTestGroup);
        this._rootSetIndexRowIndexMap.push({rootSet: rootSetInTestGroup.rootSet(), rowIndex: rowIndex});
    }

    testGroup() { return this._testGroup; }

    createStackingCell()
    {
        this._computeTestGroupStatus();

        return ComponentBase.createElement('td', {
            rowspan: this.endRowIndex() - this.startRowIndex() + 1,
            title: this._title,
            class: 'stacking-block ' + this._className + ' ' + this._status,
            onclick: this._callback,
        }, ComponentBase.createLink(this._label, this._title, this._callback));
    }

    isComplete() { return this._rootSetIndexRowIndexMap.length >= 2; }

    startRowIndex() { return this._rootSetIndexRowIndexMap[0].rowIndex; }
    endRowIndex() { return this._rootSetIndexRowIndexMap[this._rootSetIndexRowIndexMap.length - 1].rowIndex; }
    isThin()
    {
        this._computeTestGroupStatus();
        return this._status == 'failed';
    }

    _computeTestGroupStatus()
    {
        if (this._status || !this.isComplete())
            return;

        console.assert(this._rootSetIndexRowIndexMap.length <= 2); // FIXME: Support having more root sets.

        var result = this._testGroup.compareTestResults(
            this._rootSetIndexRowIndexMap[0].rootSet, this._rootSetIndexRowIndexMap[1].rootSet);

        this._label = result.label;
        this._title = result.fullLabel;
        this._status = result.status;
    }
}

AnalysisResultsViewer.TestGroupStackingGrid = class {
    constructor(rowCount)
    {
        this._blocks = [];
        this._columns = null;
        this._rowCount = rowCount;
    }

    insertBlockToColumn(newBlock)
    {
        console.assert(newBlock instanceof AnalysisResultsViewer.TestGroupStackingBlock);
        for (var i = this._blocks.length - 1; i >= 0; i--) {
            var currentBlock = this._blocks[i];
            if (currentBlock.startRowIndex() == newBlock.startRowIndex()
                && currentBlock.endRowIndex() == newBlock.endRowIndex()) {
                this._blocks.splice(i + 1, 0, newBlock);
                return;
            }
        }
        this._blocks.push(newBlock);
    }

    layout()
    {
        this._columns = [];
        for (var block of this._blocks)
            this._layoutBlock(block);
    }

    _layoutBlock(newBlock)
    {
        for (var columnIndex = 0; columnIndex < this._columns.length; columnIndex++) {
            var existingColumn = this._columns[columnIndex];
            if (newBlock.isThin() != existingColumn[0].isThin())
                continue;

            for (var i = 0; i < existingColumn.length; i++) {
                var currentBlock = existingColumn[i];
                if ((!i || existingColumn[i - 1].endRowIndex() < newBlock.startRowIndex())
                    && newBlock.endRowIndex() < currentBlock.startRowIndex()) {
                    existingColumn.splice(i, 0, newBlock);
                    return;
                }
            }

            var lastBlock = existingColumn[existingColumn.length - 1];
            if (lastBlock.endRowIndex() < newBlock.startRowIndex()) {
                existingColumn.push(newBlock);
                return;
            }
        }
        this._columns.push([newBlock]);
    }

    createCellsForRow(rowIndex)
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        var cells = [element('td', {class: 'stacking-block'}, '')];
        for (var columnIndex = 0; columnIndex < this._columns.length; columnIndex++) {
            var blocksInColumn = this._columns[columnIndex];
            if (!rowIndex && blocksInColumn[0].startRowIndex()) {
                cells.push(this._createEmptyStackingCell(blocksInColumn[0].startRowIndex()));
                continue;
            }
            for (var i = 0; i < blocksInColumn.length; i++) {
                var block = blocksInColumn[i];
                if (block.startRowIndex() == rowIndex) {
                    cells.push(block.createStackingCell());
                    break;
                }
                var rowCount = i + 1 < blocksInColumn.length ? blocksInColumn[i + 1].startRowIndex() : this._rowCount;
                var remainingRows = rowCount - block.endRowIndex() - 1;
                if (rowIndex == block.endRowIndex() + 1 && rowIndex < rowCount)
                    cells.push(this._createEmptyStackingCell(remainingRows));
            }
        }

        return cells;
    }

    _createEmptyStackingCell(rowspan, content)
    {
        return ComponentBase.createElement('td', {rowspan: rowspan, class: 'stacking-block'}, '');
    }

}
