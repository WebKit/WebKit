
class AnalysisResultsViewer extends ResultsTable {
    constructor()
    {
        super('analysis-results-viewer');
        this._startPoint = null;
        this._endPoint = null;
        this._metric = null;
        this._testGroups = null;
        this._currentTestGroup = null;
        this._rangeSelectorLabels = [];
        this._selectedRange = {};
        this._expandedPoints = new Set;
        this._groupToCellMap = new Map;
        this._selectorRadioButtonList = {};

        this._renderTestGroupsLazily = new LazilyEvaluatedFunction(this.renderTestGroups.bind(this));
    }

    setRangeSelectorLabels(labels) { this._rangeSelectorLabels = labels; }
    selectedRange() { return this._selectedRange; }

    setPoints(startPoint, endPoint, metric)
    {
        this._metric = metric;
        this._startPoint = startPoint;
        this._endPoint = endPoint;
        this._expandedPoints = new Set;
        this._expandedPoints.add(startPoint);
        this._expandedPoints.add(endPoint);
        this.enqueueToRender();
    }

    setTestGroups(testGroups, currentTestGroup)
    {
        this._testGroups = testGroups;
        this._currentTestGroup = currentTestGroup;
        if (currentTestGroup && this._rangeSelectorLabels.length) {
            const commitSets = currentTestGroup.requestedCommitSets();
            this._selectedRange = {
                [this._rangeSelectorLabels[0]]: commitSets[0],
                [this._rangeSelectorLabels[1]]: commitSets[1]
            };
        }
        this.enqueueToRender();
    }

    setAnalysisResultsView(analysisResultsView)
    {
        console.assert(analysisResultsView instanceof AnalysisResultsView);
        this._analysisResultsView = analysisResultsView;
        this.enqueueToRender();
    }

    render()
    {
        super.render();
        Instrumentation.startMeasuringTime('AnalysisResultsViewer', 'render');

        this._renderTestGroupsLazily.evaluate(this._testGroups,
            this._startPoint, this._endPoint, this._metric, this._analysisResultsView, this._expandedPoints);

        for (const label of this._rangeSelectorLabels) {
            const commitSet = this._selectedRange[label];
            if (!commitSet)
                continue;
            const list = this._selectorRadioButtonList[label] || [];
            for (const item of list) {
                if (item.commitSet.equals(commitSet))
                    item.radio.checked = true;
            }
        }

        const selectedCell = this.content().querySelector('td.selected');
        if (selectedCell)
            selectedCell.classList.remove('selected');
        if (this._groupToCellMap && this._currentTestGroup) {
            const cell = this._groupToCellMap.get(this._currentTestGroup);
            if (cell)
                cell.classList.add('selected');
        }

        Instrumentation.endMeasuringTime('AnalysisResultsViewer', 'render');
    }

    renderTestGroups(testGroups, startPoint, endPoint, metric, analysisResults, expandedPoints)
    {
        if (!testGroups || !startPoint || !endPoint || !metric || !analysisResults)
            return false;

        Instrumentation.startMeasuringTime('AnalysisResultsViewer', 'renderTestGroups');

        const commitSetsInTestGroups = this._collectCommitSetsInTestGroups(testGroups);
        const rowToMatchingCommitSets = new Map;
        const rows = this._buildRowsForPointsAndTestGroups(commitSetsInTestGroups, rowToMatchingCommitSets);

        const testGroupLayoutMap = new Map;
        rows.forEach((row, rowIndex) => {
            const matchingCommitSets = rowToMatchingCommitSets.get(row);
            if (!matchingCommitSets) {
                console.assert(row instanceof AnalysisResultsViewer.ExpandableRow);
                return;
            }

            for (let entry of matchingCommitSets) {
                const testGroup = entry.testGroup();

                let block = testGroupLayoutMap.get(testGroup);
                if (!block) {
                    block = new AnalysisResultsViewer.TestGroupStackingBlock(testGroup, this._analysisResultsView,
                        this._groupToCellMap, () => this.dispatchAction('testGroupClick', testGroup));
                    testGroupLayoutMap.set(testGroup, block);
                }
                block.addRowIndex(entry, rowIndex);
            }
        });

        const [additionalColumnsByRow, columnCount] = AnalysisResultsViewer._layoutBlocks(rows.length, testGroups.map((group) => testGroupLayoutMap.get(group)));

        for (const label of this._rangeSelectorLabels)
            this._selectorRadioButtonList[label] = [];

        const element = ComponentBase.createElement;
        const buildHeaders = (headers) => {
            return [
                this._rangeSelectorLabels.map((label) => element('th', label)),
                headers,
                columnCount ? element('td', {colspan: columnCount + 1, class: 'stacking-block'}) : [],
            ]
        };
        const buildColumns = (columns, row, rowIndex) => {
            return [
                this._rangeSelectorLabels.map((label) => {
                    if (!row.commitSet())
                        return element('td', '');
                    const radio = element('input', {type: 'radio', name: label, onchange: () => {
                        this._selectedRange[label] = row.commitSet();
                        this.dispatchAction('rangeSelectorClick', label, row);
                    }});
                    this._selectorRadioButtonList[label].push({radio, commitSet: row.commitSet()});
                    return element('td', radio);
                }),
                columns,
                additionalColumnsByRow[rowIndex],
            ];
        }
        this.renderTable(metric.makeFormatter(4), [{rows}], 'Point', buildHeaders, buildColumns);

        Instrumentation.endMeasuringTime('AnalysisResultsViewer', 'renderTestGroups');

        return true;
    }

    _collectCommitSetsInTestGroups(testGroups)
    {
        if (!this._testGroups)
            return [];

        var commitSetsInTestGroups = [];
        for (var group of this._testGroups) {
            var sortedSets = group.requestedCommitSets();
            for (var i = 0; i < sortedSets.length; i++)
                commitSetsInTestGroups.push(new AnalysisResultsViewer.CommitSetInTestGroup(group, sortedSets[i], sortedSets[i + 1]));
        }

        return commitSetsInTestGroups;
    }

    _buildRowsForPointsAndTestGroups(commitSetsInTestGroups, rowToMatchingCommitSets)
    {
        console.assert(this._startPoint.series == this._endPoint.series);
        var rowList = [];
        var pointAfterEnd = this._endPoint.series.nextPoint(this._endPoint);
        var commitSetsWithPoints = new Set;
        var pointIndex = 0;
        var previousPoint;
        for (var point = this._startPoint; point && point != pointAfterEnd; point = point.series.nextPoint(point), pointIndex++) {
            const commitSetInPoint = point.commitSet();
            const matchingCommitSets = [];
            for (var entry of commitSetsInTestGroups) {
                if (commitSetInPoint.equals(entry.commitSet()) && !commitSetsWithPoints.has(entry)) {
                    matchingCommitSets.push(entry);
                    commitSetsWithPoints.add(entry);
                }
            }

            const hasMatchingTestGroup = !!matchingCommitSets.length;
            if (!hasMatchingTestGroup && !this._expandedPoints.has(point))
                continue;

            const row = new ResultsTableRow(pointIndex.toString(), commitSetInPoint);
            row.setResult(point);

            if (previousPoint && previousPoint.series.nextPoint(previousPoint) != point)
                rowList.push(new AnalysisResultsViewer.ExpandableRow(this._expandBetween.bind(this, previousPoint, point)));
            previousPoint = point;

            rowToMatchingCommitSets.set(row, matchingCommitSets);
            rowList.push(row);
        }

        commitSetsInTestGroups.forEach(function (entry) {
            if (commitSetsWithPoints.has(entry))
                return;

            for (var i = 0; i < rowList.length; i++) {
                var row = rowList[i];
                if (!(row instanceof AnalysisResultsViewer.ExpandableRow) && row.commitSet().equals(entry.commitSet())) {
                    rowToMatchingCommitSets.get(row).push(entry);
                    return;
                }
            }

            var groupTime = entry.commitSet().latestCommitTime();
            var newRow = new ResultsTableRow(null, entry.commitSet());
            rowToMatchingCommitSets.set(newRow, [entry]);

            for (var i = 0; i < rowList.length; i++) {
                if (rowList[i] instanceof AnalysisResultsViewer.ExpandableRow)
                    continue;

                if (entry.succeedingCommitSet() && rowList[i].commitSet().equals(entry.succeedingCommitSet())) {
                    rowList.splice(i, 0, newRow);
                    return;
                }

                var rowTime = rowList[i].commitSet().latestCommitTime();
                if (rowTime > groupTime) {
                    rowList.splice(i, 0, newRow);
                    return;
                }

                if (rowTime == groupTime) {
                    // Missing some commits. Do as best as we can to avoid going backwards in time.
                    var repositoriesInNewRow = entry.commitSet().repositories();
                    for (var j = i; j < rowList.length; j++) {
                        if (rowList[j] instanceof AnalysisResultsViewer.ExpandableRow)
                            continue;
                        for (var repository of repositoriesInNewRow) {
                            var newCommit = entry.commitSet().commitForRepository(repository);
                            var rowCommit = rowList[j].commitSet().commitForRepository(repository);
                            if (!rowCommit || newCommit.time() < rowCommit.time()) {
                                rowList.splice(j, 0, newRow);
                                return;
                            }
                        }
                    }
                }
            }

            var newRow = new ResultsTableRow(null, entry.commitSet());
            rowToMatchingCommitSets.set(newRow, [entry]);
            rowList.push(newRow);
        });

        return rowList;
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

        const expandedPoints = new Set([...this._expandedPoints]);
        for (var i = indexBeforeStart + 1; i < indexAfterEnd; i += increment)
            expandedPoints.add(series.findPointByIndex(i));
        this._expandedPoints = expandedPoints;

        this.enqueueToRender();
    }

    static _layoutBlocks(rowCount, blocks)
    {
        const sortedBlocks = this._sortBlocksByRow(blocks);

        const columns = [];
        for (const block of sortedBlocks)
            this._insertBlockInFirstAvailableColumn(columns, block);

        const rows = new Array(rowCount);
        for (let i = 0; i < rowCount; i++)
            rows[i] = this._createCellsForRow(columns, i);

        return [rows, columns.length];
    }

    static _sortBlocksByRow(blocks)
    {
        for (let i = 0; i < blocks.length; i++)
            blocks[i].index = i;

        return blocks.slice(0).sort((block1, block2) => {
            const startRowDiff = block1.startRowIndex() - block2.startRowIndex();
            if (startRowDiff)
                return startRowDiff;

            // Order backwards for end rows in order to place test groups with a larger range at the beginning.
            const endRowDiff = block2.endRowIndex() - block1.endRowIndex();
            if (endRowDiff)
                return endRowDiff;

            return block1.index - block2.index;
        });
    }

    static _insertBlockInFirstAvailableColumn(columns, newBlock)
    {
        for (const existingColumn of columns) {
            for (let i = 0; i < existingColumn.length; i++) {
                const currentBlock = existingColumn[i];
                if ((!i || existingColumn[i - 1].endRowIndex() < newBlock.startRowIndex())
                    && newBlock.endRowIndex() < currentBlock.startRowIndex()) {
                    existingColumn.splice(i, 0, newBlock);
                    return;
                }
            }
            const lastBlock = existingColumn[existingColumn.length - 1];
            console.assert(lastBlock);
            if (lastBlock.endRowIndex() < newBlock.startRowIndex()) {
                existingColumn.push(newBlock);
                return;
            }
        }
        columns.push([newBlock]);
    }

    static _createCellsForRow(columns, rowIndex)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;

        const crateEmptyCell = (rowspan) => element('td', {rowspan: rowspan, class: 'stacking-block'}, '');

        const cells = [element('td', {class: 'stacking-block'}, '')];
        for (const blocksInColumn of columns) {
            if (!rowIndex && blocksInColumn[0].startRowIndex()) {
                cells.push(crateEmptyCell(blocksInColumn[0].startRowIndex()));
                continue;
            }
            for (let i = 0; i < blocksInColumn.length; i++) {
                const block = blocksInColumn[i];
                if (block.startRowIndex() == rowIndex) {
                    cells.push(block.createStackingCell());
                    break;
                }
                const rowCount = i + 1 < blocksInColumn.length ? blocksInColumn[i + 1].startRowIndex() : this._rowCount;
                const remainingRows = rowCount - block.endRowIndex() - 1;
                if (rowIndex == block.endRowIndex() + 1 && rowIndex < rowCount)
                    cells.push(crateEmptyCell(remainingRows));
            }
        }

        return cells;
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

AnalysisResultsViewer.CommitSetInTestGroup = class {
    constructor(testGroup, commitSet, succeedingCommitSet)
    {
        console.assert(testGroup instanceof TestGroup);
        console.assert(commitSet instanceof CommitSet);
        this._testGroup = testGroup;
        this._commitSet = commitSet;
        this._succeedingCommitSet = succeedingCommitSet;
    }

    testGroup() { return this._testGroup; }
    commitSet() { return this._commitSet; }
    succeedingCommitSet() { return this._succeedingCommitSet; }
}

AnalysisResultsViewer.TestGroupStackingBlock = class {
    constructor(testGroup, analysisResultsView, groupToCellMap, callback)
    {
        this._testGroup = testGroup;
        this._analysisResultsView = analysisResultsView;
        this._commitSetIndexRowIndexMap = [];
        this._groupToCellMap = groupToCellMap;
        this._callback = callback;
    }

    addRowIndex(commitSetInTestGroup, rowIndex)
    {
        console.assert(commitSetInTestGroup instanceof AnalysisResultsViewer.CommitSetInTestGroup);
        this._commitSetIndexRowIndexMap.push({commitSet: commitSetInTestGroup.commitSet(), rowIndex});
    }

    testGroup() { return this._testGroup; }

    createStackingCell()
    {
        const {label, title, status} = this._computeTestGroupStatus();

        const cell = ComponentBase.createElement('td', {
            rowspan: this.endRowIndex() - this.startRowIndex() + 1,
            title,
            class: 'stacking-block ' + status,
            onclick: this._callback,
        }, ComponentBase.createLink(label, title, this._callback));

        this._groupToCellMap.set(this._testGroup, cell);

        return cell;
    }

    isComplete() { return this._commitSetIndexRowIndexMap.length >= 2; }

    startRowIndex() { return this._commitSetIndexRowIndexMap[0].rowIndex; }
    endRowIndex() { return this._commitSetIndexRowIndexMap[this._commitSetIndexRowIndexMap.length - 1].rowIndex; }

    _measurementsForCommitSet(testGroup, commitSet)
    {
        return testGroup.requestsForCommitSet(commitSet).map((request) => {
            return this._analysisResultsView.resultForRequest(request);
        }).filter((result) => !!result);
    }

    _computeTestGroupStatus()
    {
        if (!this.isComplete())
            return {label: null, title: null, status: null};
        console.assert(this._commitSetIndexRowIndexMap.length <= 2); // FIXME: Support having more root sets.
        const startValues = this._measurementsForCommitSet(this._testGroup, this._commitSetIndexRowIndexMap[0].commitSet);
        const endValues = this._measurementsForCommitSet(this._testGroup, this._commitSetIndexRowIndexMap[1].commitSet);
        const result = this._testGroup.compareTestResults(this._analysisResultsView.metric(), startValues, endValues);
        return {label: result.label, title: result.fullLabelForMean, status: result.status};
    }
}
