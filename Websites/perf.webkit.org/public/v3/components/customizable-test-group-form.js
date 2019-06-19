class CustomizableTestGroupForm extends TestGroupForm {

    constructor()
    {
        super('customizable-test-group-form');
        this._commitSetMap = null;
        this._uncustomizedCommitSetMap = null;
        this._name = null;
        this._isCustomized = false;
        this._revisionEditorMap = null;
        this._ownerRevisionMap = null;
        this._checkedLabelByPosition = null;
        this._hasIncompleteOwnedRepository = null;
        this._fetchingCommitPromises = [];
    }

    setCommitSetMap(map)
    {
        this._commitSetMap = map;
        this._uncustomizedCommitSetMap = map;
        this._isCustomized = false;
        this._fetchingCommitPromises = [];
        this._checkedLabelByPosition = new Map;
        this._hasIncompleteOwnedRepository = new Map;
        this.enqueueToRender();
    }

    startTesting()
    {
        this.dispatchAction('startTesting', this._repetitionCount, this._name, this._computeCommitSetMap(), this._notifyOnCompletion);
    }

    didConstructShadowTree()
    {
        super.didConstructShadowTree();

        const nameControl = this.content('name');
        nameControl.oninput = () => {
            this._name = nameControl.value;
            this.enqueueToRender();
        };

        this.content('customize-link').onclick = this.createEventHandler(() => {
            if (!this._isCustomized) {
                const uncustomizedCommitSetMap = this._commitSetMap;
                const fetchingCommitPromises = [];
                this._commitSetMap = new Map;
                for (const label in uncustomizedCommitSetMap) {
                    const intermediateCommitSet = new IntermediateCommitSet(uncustomizedCommitSetMap[label]);
                    fetchingCommitPromises.push(intermediateCommitSet.fetchCommitLogs());
                    this._commitSetMap.set(label, intermediateCommitSet);
                }
                this._fetchingCommitPromises = fetchingCommitPromises;

                return Promise.all(fetchingCommitPromises).then(() => {
                    if (this._fetchingCommitPromises !== fetchingCommitPromises)
                        return;
                    this._isCustomized = true;
                    this._fetchingCommitPromises = [];
                    for (const label in uncustomizedCommitSetMap)
                        this._checkedLabelByPosition.set(label, new Map);
                    this.enqueueToRender();
                });
            }
            this._isCustomized = true;
            this.enqueueToRender();
        });
    }

    _computeCommitSetMap()
    {
        console.assert(this._commitSetMap);
        if (!this._isCustomized)
            return this._commitSetMap;

        const map = {};
        for (const [label, commitSet] of this._commitSetMap) {
            const customCommitSet = new CustomCommitSet;
            for (const repository of commitSet.repositories()) {
                const revisionEditor = this._revisionEditorMap.get(label).get(repository);
                console.assert(revisionEditor);
                const ownerRevision = this._ownerRevisionMap.get(label).get(repository) || null;

                customCommitSet.setRevisionForRepository(repository, revisionEditor.value, null, ownerRevision);
            }
            map[label] = customCommitSet;
        }
        return map;
    }

    render()
    {
        super.render();

        this.content('start-button').disabled = !(this._commitSetMap && this._name);
        this.content('customize-link-container').style.display = !this._commitSetMap ? 'none' : null;

        this._renderCustomRevisionTable(this._commitSetMap, this._isCustomized, this._uncustomizedCommitSetMap);
    }

    _renderCustomRevisionTable(commitSetMap, isCustomized, uncustomizedCommitSetMap)
    {
        if (!commitSetMap || !isCustomized) {
            this.renderReplace(this.content('custom-table'), []);
            return;
        }

        const repositorySet = new Set;
        const ownedRepositoriesByRepository = new Map;
        const commitSetLabels = [];
        this._revisionEditorMap = new Map;
        this._ownerRevisionMap = new Map;
        for (const [label, commitSet] of commitSetMap) {
            for (const repository of commitSet.highestLevelRepositories()) {
                repositorySet.add(repository);
                const ownedRepositories = commitSetMap.get(label).ownedRepositoriesForOwnerRepository(repository);

                if (!ownedRepositories)
                    continue;

                if (!ownedRepositoriesByRepository.has(repository))
                    ownedRepositoriesByRepository.set(repository, new Set);
                const ownedRepositorySet = ownedRepositoriesByRepository.get(repository);
                for (const ownedRepository of ownedRepositories)
                    ownedRepositorySet.add(ownedRepository)
            }
            commitSetLabels.push(label);
            this._revisionEditorMap.set(label, new Map);
            this._ownerRevisionMap.set(label, new Map);
        }

        const repositoryList = Repository.sortByNamePreferringOnesWithURL(Array.from(repositorySet.values()));
        const element = ComponentBase.createElement;
        this.renderReplace(this.content('custom-table'), [
            element('thead',
                element('tr',
                    [element('td', {colspan: 2}, 'Repository'), commitSetLabels.map((label) => element('td', {colspan: commitSetLabels.length + 1}, label)), element('td')])),
            this._constructTableBodyList(repositoryList, commitSetMap, ownedRepositoriesByRepository, this._hasIncompleteOwnedRepository, uncustomizedCommitSetMap),
            this._constructTestabilityRows(commitSetMap)]);
    }

    _constructTestabilityRows(commitSetMap)
    {
        const element = ComponentBase.createElement;

        const commitSets = Array.from(commitSetMap.values());
        const hasCommitWithTestability = commitSets.some((commitSet) =>  !!commitSet.commitsWithTestability().length);
        if (!hasCommitWithTestability)
            return [];

        const testabilityCells = [];
        for (const commitSet of commitSetMap.values()) {
            const entries = commitSet.commitsWithTestability().map((commit) =>
                element('li', `${commit.title()}: ${commit.testability()}`));
            testabilityCells.push(element('td', {colspan: commitSetMap.size + 1, class: 'testability'}, element('ul', entries)));
        }

        return element('tbody', element('tr', [element('td', {colspan: 2}), testabilityCells, element('td')]));
    }

    _constructTableBodyList(repositoryList, commitSetMap, ownedRepositoriesByRepository, hasIncompleteOwnedRepository, uncustomizedCommitSetMap)
    {
        const element = ComponentBase.createElement;
        const tableBodyList = [];
        for(const repository of repositoryList) {
            const rows = [];
            const allCommitSetSpecifiesOwnerCommit = Array.from(commitSetMap.values()).every((commitSet) => commitSet.ownsCommitsForRepository(repository));
            const hasIncompleteRow = hasIncompleteOwnedRepository.get(repository);
            const ownedRepositories = ownedRepositoriesByRepository.get(repository);

            rows.push(this._constructTableRowForCommitsWithoutOwner(commitSetMap, repository, allCommitSetSpecifiesOwnerCommit, hasIncompleteOwnedRepository, uncustomizedCommitSetMap));

            if ((!ownedRepositories || !ownedRepositories.size) && !hasIncompleteRow) {
                tableBodyList.push(element('tbody', rows));
                continue;
            }

            if (ownedRepositories) {
                for (const ownedRepository of ownedRepositories)
                    rows.push(this._constructTableRowForCommitsWithOwner(commitSetMap, ownedRepository, repository, uncustomizedCommitSetMap));
            }

            if (hasIncompleteRow) {
                const commits = Array.from(commitSetMap.values()).map((commitSet) => commitSet.commitForRepository(repository));
                const commitDiff = CommitLog.ownedCommitDifferenceForOwnerCommits(...commits);
                rows.push(this._constructTableRowForIncompleteOwnedCommits(commitSetMap, repository, commitDiff));
            }
            tableBodyList.push(element('tbody', rows));
        }
        return tableBodyList;
    }

    _constructTableRowForCommitsWithoutOwner(commitSetMap, repository, ownsRepositories, hasIncompleteOwnedRepository, uncustomizedCommitSetMap)
    {
        const element = ComponentBase.createElement;
        const cells = [element('th', {colspan: 2}, repository.label())];

        for (const label of commitSetMap.keys())
            cells.push(this._constructRevisionRadioButtons(commitSetMap, repository, label, null, uncustomizedCommitSetMap));

        if (ownsRepositories) {
            const plusButton = new PlusButton();
            plusButton.setDisabled(hasIncompleteOwnedRepository.get(repository));
            plusButton.listenToAction('activate', () => {
                this._hasIncompleteOwnedRepository.set(repository, true);
                this.enqueueToRender();
            });
            cells.push(element('td', plusButton));
        } else
            cells.push(element('td'));

        return element('tr', cells);
    }

    _constructTableRowForCommitsWithOwner(commitSetMap, repository, ownerRepository, uncustomizedCommitSetMap)
    {
        const element = ComponentBase.createElement;
        const cells = [element('td', {class: 'owner-repository-label'}), element('th', repository.label())];
        const minusButton = new MinusButton();

        for (const label of commitSetMap.keys())
            cells.push(this._constructRevisionRadioButtons(commitSetMap, repository, label, ownerRepository, uncustomizedCommitSetMap));

        minusButton.listenToAction('activate', () => {
            for (const commitSet of commitSetMap.values())
                commitSet.removeCommitForRepository(repository)
            this.enqueueToRender();
        });
        cells.push(element('td', minusButton));
        return element('tr', cells);
    }

    _constructTableRowForIncompleteOwnedCommits(commitSetMap, ownerRepository, commitDiff)
    {
        const element = ComponentBase.createElement;
        const configurationCount =  commitSetMap.size;
        const numberOfCellsPerConfiguration = configurationCount + 1;
        const changedRepositories = Array.from(commitDiff.keys());
        const minusButton = new MinusButton();
        const comboBox = new ComboBox(changedRepositories.map((repository) => repository.name()).sort());

        comboBox.listenToAction('update', (repositoryName) => {
            const targetRepository = changedRepositories.find((repository) => repositoryName === repository.name());
            const ownedCommitDifferenceForRepository = Array.from(commitDiff.get(targetRepository).values());
            const commitSetList = Array.from(commitSetMap.values());

            console.assert(ownedCommitDifferenceForRepository.length === commitSetList.length);
            for (let i = 0; i < commitSetList.length; i++)
                commitSetList[i].setCommitForRepository(targetRepository, ownedCommitDifferenceForRepository[i]);
            this._hasIncompleteOwnedRepository.set(ownerRepository, false);
            this.enqueueToRender();
        });

        const cells = [element('td', {class: 'owner-repository-label'}), element('th', comboBox), element('td', {colspan: configurationCount * numberOfCellsPerConfiguration})];

        minusButton.listenToAction('activate', () => {
            this._hasIncompleteOwnedRepository.set(ownerRepository, false);
            this.enqueueToRender();
        });
        cells.push(element('td', minusButton));

        return element('tr', cells);
    }

    _constructRevisionRadioButtons(commitSetMap, repository, columnLabel, ownerRepository, uncustomizedCommitSetMap)
    {
        const element = ComponentBase.createElement;

        const commitForColumn = commitSetMap.get(columnLabel).commitForRepository(repository);
        const revision = commitForColumn ? commitForColumn.revision() : '';
        if (commitForColumn && commitForColumn.ownerCommit())
            this._ownerRevisionMap.get(columnLabel).set(repository, commitForColumn.ownerCommit().revision());

        const revisionEditor = element('input', {disabled: !!ownerRepository, value: revision,
            onchange: () => {
                if (ownerRepository)
                    return;
                commitSetMap.get(columnLabel).updateRevisionForOwnerRepository(repository, revisionEditor.value).then(
                    () => this.enqueueToRender(),
                    () => {
                        alert(`"${revisionEditor.value}" does not exist in "${repository.name()}".`);
                        revisionEditor.value = revision;
                    });
            }});

        this._revisionEditorMap.get(columnLabel).set(repository, revisionEditor);

        const nodes = [];
        for (const [labelToChoose, commitSet] of commitSetMap) {
            const commit = commitSet.commitForRepository(repository);
            const checkedLabel = this._checkedLabelByPosition.get(columnLabel).get(repository) || columnLabel;
            const checked = labelToChoose == checkedLabel;
            const radioButton = element('input', {type: 'radio', name: `${columnLabel}-${repository.id()}-radio`, checked,
                onchange: () => {
                    const uncustomizedCommit = uncustomizedCommitSetMap[labelToChoose].commitForRepository(repository) || commit;
                    commitSetMap.get(columnLabel).setCommitForRepository(repository, uncustomizedCommit);
                    this._checkedLabelByPosition.get(columnLabel).set(repository, labelToChoose);
                    revisionEditor.value = uncustomizedCommit ? uncustomizedCommit.revision() : '';
                    if (uncustomizedCommit && uncustomizedCommit.ownerCommit())
                        this._ownerRevisionMap.get(columnLabel).set(repository, uncustomizedCommit.ownerCommit().revision());
                    this.enqueueToRender();
                }});
            nodes.push(element('td', element('label', [radioButton, labelToChoose])));
        }
        nodes.push(element('td', revisionEditor));

        return nodes;
    }

    static cssTemplate()
    {
        return `
            #customize-link-container,
            #customize-link {
                color: #333;
            }

            #customize-link-container {
                margin-left: 0.4rem;
            }

            #custom-table:not(:empty) {
                margin: 1rem 0;
            }

            #custom-table td.owner-repository-label {
                border-top: solid 2px transparent;
                border-bottom: solid 1px transparent;
                min-width: 2rem;
                text-align: right;
            }

            #custom-table tr:last-child td.owner-repository-label {
                border-bottom: solid 1px #ddd;
            }

            #custom-table th {
                min-width: 12rem;
            }

            #custom-table,
            #custom-table td,
            #custom-table th {
                font-weight: inherit;
                border-collapse: collapse;
                border-top: solid 1px #ddd;
                border-bottom: solid 1px #ddd;
                padding: 0.4rem 0.2rem;
                font-size: 0.9rem;
            }

            #custom-table thead td,
            #custom-table th {
                text-align: center;
            }

            #notify-on-completion-checkbox {
                margin-left: 0.4rem;
            }

            #custom-table td.testability {
                vertical-align: top;
            }

            #custom-table td.testability ul {
                text-align: left;
                color: #c33;
                max-width: 13rem;
                margin: 0 0 0 1rem;
                padding: 0;
            }
            `;
    }

    static formContent()
    {
        return `
            <input id="name" type="text" placeholder="Test group name">
            ${super.formContent()}
            <span id="customize-link-container">(<a id="customize-link" href="#">Customize</a>)</span>
            <table id="custom-table"></table>
        `;
    }
}

ComponentBase.defineElement('customizable-test-group-form', CustomizableTestGroupForm);
