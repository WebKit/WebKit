class CustomAnalysisTaskConfigurator extends ComponentBase {

    constructor()
    {
        super('custom-analysis-task-configurator');

        this._selectedTests = [];
        this._triggerablePlatforms = [];
        this._selectedPlatform = null;
        this._configurationNames = ['Baseline', 'Comparison'];
        this._showComparison = false;
        this._commitSetMap = {};
        this._specifiedRevisions = {'Baseline': new Map, 'Comparison': new Map};
        this._fetchedRevisions = {'Baseline': new Map, 'Comparison': new Map};
        this._repositoryGroupByConfiguration = {'Baseline': null, 'Comparison': null};
        this._updateTriggerableLazily = new LazilyEvaluatedFunction(this._updateTriggerable.bind(this));

        this._renderTriggerableTestsLazily = new LazilyEvaluatedFunction(this._renderTriggerableTests.bind(this));
        this._renderTriggerablePlatformsLazily = new LazilyEvaluatedFunction(this._renderTriggerablePlatforms.bind(this));
        this._renderRepositoryPanesLazily = new LazilyEvaluatedFunction(this._renderRepositoryPanes.bind(this));

        this._fileUploaders = {};
    }

    tests() { return this._selectedTests; }
    platform() { return this._selectedPlatform; }
    commitSets()
    {
        const map = this._commitSetMap;
        if (!map['Baseline'] || !map['Comparison'])
            return null;
        return [map['Baseline'], map['Comparison']];
    }

    didConstructShadowTree()
    {
        this.content('specify-comparison-button').onclick = this.createEventHandler(() => this._configureComparison());

        const baselineRootsUploader = new InstantFileUploader;
        baselineRootsUploader.listenToAction('uploadedFile', (uploadedFile) => {
            comparisonRootsUploader.addUploadedFile(uploadedFile);
            this._updateCommitSetMap();
        });
        baselineRootsUploader.listenToAction('removedFile', () => this._updateCommitSetMap());
        this._fileUploaders['Baseline'] = baselineRootsUploader;

        const comparisonRootsUploader = new InstantFileUploader;
        comparisonRootsUploader.listenToAction('uploadedFile', () => this._updateCommitSetMap());
        comparisonRootsUploader.listenToAction('removedFile', () => this._updateCommitSetMap());
        this._fileUploaders['Comparison'] = comparisonRootsUploader;
    }

    _configureComparison()
    {
        this._showComparison = true;
        this._repositoryGroupByConfiguration['Comparison'] = this._repositoryGroupByConfiguration['Baseline'];

        const specifiedBaselineRevisions = this._specifiedRevisions['Baseline'];
        const specifiedComparisonRevisions = new Map;
        for (let key of specifiedBaselineRevisions.keys())
            specifiedComparisonRevisions.set(key, specifiedBaselineRevisions.get(key));
        this._specifiedRevisions['Comparison'] = specifiedComparisonRevisions;

        this.enqueueToRender();
    }

    render()
    {
        super.render();

        this._renderTriggerableTestsLazily.evaluate();
        this._renderTriggerablePlatformsLazily.evaluate(this._selectedTests, this._triggerablePlatforms);

        const [triggerable, error] = this._updateTriggerableLazily.evaluate(this._selectedTests, this._selectedPlatform);

        this._renderRepositoryPanesLazily.evaluate(triggerable, error, this._selectedPlatform, this._repositoryGroupByConfiguration, this._showComparison);
    }

    _renderTriggerableTests()
    {
        const enabledTriggerables = Triggerable.all().filter((triggerable) => !triggerable.isDisabled());

        let tests = Test.topLevelTests().filter((test) => test.metrics().length && enabledTriggerables.some((triggerable) => triggerable.acceptsTest(test)));
        this.renderReplace(this.content('test-list'), this._buildCheckboxList('test', tests, (selectedTests) => {
            this._selectedTests = selectedTests;

            this._triggerablePlatforms = Triggerable.triggerablePlatformsForTests(this._selectedTests);
            if (this._selectedTests.length && !this._triggerablePlatforms.includes(this._selectedPlatform))
                this._selectedPlatform = null;
        }));
    }

    _renderTriggerablePlatforms(selectedTests, triggerablePlatforms)
    {
        if (!selectedTests.length) {
            this.content('platform-pane').style.display = 'none';
            return;
        }
        this.content('platform-pane').style.display = null;

        this.renderReplace(this.content('platform-list'), this._buildCheckboxList('platform', triggerablePlatforms, (selectedPlatforms) => {
            this._selectedPlatform = selectedPlatforms.length ? selectedPlatforms[0] : null;

            const [triggerable, error] = this._updateTriggerableLazily.evaluate(this._selectedTests, this._selectedPlatform);
            this._updateRepositoryGroups(triggerable);
            this._updateCommitSetMap();

            this.enqueueToRender();
        }));
    }

    _buildCheckboxList(name, objects, callback)
    {
        const listItems = [];
        let selectedListItems = [];
        const element = ComponentBase.createElement;
        return objects.map((object) => {
            const checkbox = element('input', {type: 'radio', name: name, onchange: () => {
                selectedListItems.forEach((item) => item.label.classList.remove('selected'));
                selectedListItems = listItems.filter((item) => item.checkbox.checked);
                selectedListItems.forEach((item) => item.label.classList.add('selected'));

                callback(selectedListItems.map((item) => item.object));
                this.enqueueToRender();
            }});
            const label = element('label', [checkbox, object.label()]);
            listItems.push({checkbox, label, object});
            return element('li', label);
        })
    }

    _updateTriggerable(tests, platform)
    {
        let triggerable = null;
        let error = null;
        if (tests.length && platform) {
            triggerable = Triggerable.findByTestConfiguration(tests[0], platform);
            let matchingTests = new Set;
            let mismatchingTests = new Set;
            for (let test of tests) {
                if (Triggerable.findByTestConfiguration(test, platform) == triggerable)
                    matchingTests.add(test);
                else
                    mismatchingTests.add(test);
            }
            if (matchingTests.size < tests.length) {
                const matchingTestNames = [...matchingTests].map((test) => test.fullName()).sort().join('", "');
                const mismathingTestNames = [...mismatchingTests].map((test) => test.fullName()).sort().join('", "');
                error = `Tests "${matchingTestNames}" and "${mismathingTestNames}" cannot be scheduled
                    simultenosuly on "${platform.name()}". Please select one of them at a time.`;
            }
        }

        return [triggerable, error];
    }

    _updateRepositoryGroups(triggerable)
    {
        const repositoryGroups = triggerable ? TriggerableRepositoryGroup.sortByNamePreferringSmallerRepositories(triggerable.repositoryGroups()) : [];
        for (let name in this._repositoryGroupByConfiguration) {
            const currentGroup = this._repositoryGroupByConfiguration[name];
            let matchingGroup = null;
            if (currentGroup) {
                if (repositoryGroups.includes(currentGroup))
                    matchingGroup = currentGroup;
                else
                    matchingGroup = repositoryGroups.find((group) => group.name() == currentGroup.name());
            }
            if (!matchingGroup && repositoryGroups.length)
                matchingGroup = repositoryGroups[0];
            this._repositoryGroupByConfiguration[name] = matchingGroup;
        }
    }

    _updateCommitSetMap()
    {
        const newBaseline = this._computeCommitSet('Baseline');
        let newComparison = this._computeCommitSet('Comparison');
        if (newBaseline && newComparison && newBaseline.equals(newComparison))
            newComparison = null;

        const currentBaseline = this._commitSetMap['Baseline'];
        const currentComparison = this._commitSetMap['Baseline'];
        if (newBaseline == currentBaseline && newComparison == currentComparison)
            return; // Both of them are null.

        if (newBaseline && currentBaseline && newBaseline.equals(currentBaseline)
            && newComparison && currentComparison && newComparison.equals(currentComparison))
            return;

        this._commitSetMap = {'Baseline': newBaseline, 'Comparison': newComparison};

        this.dispatchAction('commitSetChange');
        this.enqueueToRender();
    }

    _computeCommitSet(configurationName)
    {
        const repositoryGroup = this._repositoryGroupByConfiguration[configurationName];
        if (!repositoryGroup)
            return null;

        const fileUploader = this._fileUploaders[configurationName];
        if (!fileUploader || fileUploader.hasFileToUpload())
            return null;

        const commitSet = new CustomCommitSet;
        for (let repository of repositoryGroup.repositories()) {
            let revision = this._specifiedRevisions[configurationName].get(repository);
            if (!revision)
                revision = this._fetchedRevisions[configurationName].get(repository);
            if (!revision)
                return null;
            commitSet.setRevisionForRepository(repository, revision);
        }

        for (let uploadedFile of fileUploader.uploadedFiles())
            commitSet.addCustomRoot(uploadedFile);

        return commitSet;
    }

    _renderRepositoryPanes(triggerable, error, platform, repositoryGroupByConfiguration, showComparison)
    {
        this.content('repository-configuration-error-pane').style.display = error ? null : 'none';
        this.content('error').textContent = error;

        this.content('baseline-configuration-pane').style.display = triggerable ? null : 'none';
        this.content('specify-comparison-pane').style.display = triggerable && !showComparison ? null : 'none';
        this.content('comparison-configuration-pane').style.display = triggerable && showComparison ? null : 'none';

        if (!triggerable)
            return;

        const repositoryGroups = TriggerableRepositoryGroup.sortByNamePreferringSmallerRepositories(triggerable.repositoryGroups());

        const repositorySet = new Set;
        for (let group of repositoryGroups) {
            for (let repository of group.repositories())
                repositorySet.add(repository);
        }

        const repositories = Repository.sortByNamePreferringOnesWithURL([...repositorySet]);
        const requiredRepositories = repositories.filter((repository) => {
            return repositoryGroups.every((group) => group.repositories().includes(repository));
        });
        const alwaysAcceptsCustomRoots = repositoryGroups.every((group) => group.acceptsCustomRoots());

        console.log(repositoryGroups, alwaysAcceptsCustomRoots);

        this._renderBaselineRevisionTable(platform, repositoryGroups, requiredRepositories, repositoryGroupByConfiguration, alwaysAcceptsCustomRoots);

        if (showComparison)
            this._renderComparisonRevisionTable(platform, repositoryGroups, requiredRepositories, repositoryGroupByConfiguration, alwaysAcceptsCustomRoots);
    }

    _renderBaselineRevisionTable(platform, repositoryGroups, requiredRepositories, repositoryGroupByConfiguration, alwaysAcceptsCustomRoots)
    {
        let currentGroup = repositoryGroupByConfiguration['Baseline'];
        const optionalRepositoryList = this._optionalRepositoryList(currentGroup, requiredRepositories);
        this.renderReplace(this.content('baseline-revision-table'),
            this._buildRevisionTable('Baseline', repositoryGroups, currentGroup, platform, requiredRepositories, optionalRepositoryList, alwaysAcceptsCustomRoots));
    }

    _renderComparisonRevisionTable(platform, repositoryGroups, requiredRepositories, repositoryGroupByConfiguration, alwaysAcceptsCustomRoots)
    {
        let currentGroup = repositoryGroupByConfiguration['Comparison'];
        const optionalRepositoryList = this._optionalRepositoryList(currentGroup, requiredRepositories);
        this.renderReplace(this.content('comparison-revision-table'),
            this._buildRevisionTable('Comparison', repositoryGroups, currentGroup, platform, requiredRepositories, optionalRepositoryList, alwaysAcceptsCustomRoots));
    }

    _optionalRepositoryList(currentGroup, requiredRepositories)
    {
        if (!currentGroup)
            return [];
        return Repository.sortByNamePreferringOnesWithURL(currentGroup.repositories().filter((repository) => !requiredRepositories.includes(repository)));
    }

    _buildRevisionTable(configurationName, repositoryGroups, currentGroup, platform, requiredRepositories, optionalRepositoryList, alwaysAcceptsCustomRoots)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;

        const customRootsTBody = element('tbody', [
            element('tr', [
                element('th', 'Roots'),
                element('td', this._fileUploaders[configurationName]),
            ]),
        ]);

        return [
            element('tbody',
                requiredRepositories.map((repository) => {
                    return element('tr', [
                        element('th', repository.name()),
                        element('td', this._buildRevisionInput(configurationName, repository, platform))
                    ]);
                })),
            alwaysAcceptsCustomRoots ? customRootsTBody : [],
            element('tbody', [
                element('tr', {'class': 'group-row'},
                    element('td', {colspan: 2}, this._buildRepositoryGroupList(repositoryGroups, currentGroup, configurationName))),
            ]),
            !alwaysAcceptsCustomRoots && currentGroup && currentGroup.acceptsCustomRoots() ? customRootsTBody : [],
            element('tbody',
                optionalRepositoryList.map((repository) => {
                    return element('tr',[
                        element('th', repository.name()),
                        element('td', this._buildRevisionInput(configurationName, repository, platform))
                    ]);
                })
            )];
    }

    _buildRepositoryGroupList(repositoryGroups, currentGroup, configurationName)
    {
        const element = ComponentBase.createElement;
        return repositoryGroups.map((group) => {
            const input = element('input', {
                type: 'radio',
                name: 'repositoryGroup-for-' + configurationName.toLowerCase(),
                checked: currentGroup == group,
                onchange: () => this._selectRepositoryGroup(configurationName, group)
            });
            return [element('label', [input, group.description()])];
        });
    }

    _selectRepositoryGroup(configurationName, group)
    {
        const source = this._repositoryGroupByConfiguration;
        const clone = {};
        for (let key in source)
            clone[key] = source[key];
        clone[configurationName] = group;
        this._repositoryGroupByConfiguration = clone;
        this._updateCommitSetMap();
        this.enqueueToRender();
    }

    _buildRevisionInput(configurationName, repository, platform)
    {
        const revision = this._specifiedRevisions[configurationName].get(repository) || '';
        const element = ComponentBase.createElement;
        const input = element('input', {value: revision, oninput: () => {
            unmodifiedInput = null;
            this._specifiedRevisions[configurationName].set(repository, input.value);
            this._updateCommitSetMap();
        }});
        let unmodifiedInput = input;

        if (!revision) {
            CommitLog.fetchLatestCommitForPlatform(repository, platform).then((commit) => {
                if (commit && unmodifiedInput) {
                    unmodifiedInput.value = commit.revision();
                    this._fetchedRevisions[configurationName].set(repository, commit.revision());
                    this._updateCommitSetMap();
                }
            });
        }

        return input;
    }

    static htmlTemplate()
    {
        return `
            <section id="test-pane" class="pane">
                <h2>1. Select a Test</h2>
                <ul id="test-list" class="config-list"></ul>
            </section>
            <section id="platform-pane" class="pane">
                <h2>2. Select a Platform</h2>
                <ul id="platform-list" class="config-list"></ul>
            </section>
            <section id="repository-configuration-error-pane" class="pane">
                <h2>Incompatible tests</h2>
                <p id="error"></p>
            </section>
            <section id="baseline-configuration-pane" class="pane">
                <h2>3. Configure Baseline</h2>
                <table id="baseline-revision-table" class="revision-table"></table>
            </section>
            <section id="specify-comparison-pane" class="pane">
                <button id="specify-comparison-button">Configure to Compare</button>
            </section>
            <section id="comparison-configuration-pane" class="pane">
                <h2>4. Configure Comparison</h2>
                <table id="comparison-revision-table" class="revision-table"></table>
            </section>`;
    }

    static cssTemplate()
    {
        return `
            :host {
                display: flex !important;
                flex-direction: row !important;
            }
            .pane {
                margin-right: 1rem;
                padding: 0;
            }
            .pane h2 {
                padding: 0;
                margin: 0;
                margin-bottom: 0.5rem;
                font-size: 1.2rem;
                font-weight: inherit;
                text-align: center;
                white-space: nowrap;
            }

            .config-list {
                height: 20rem;
                overflow: scroll;
                display: block;
                margin: 0;
                padding: 0;
                list-style: none;
                font-size: inherit;
                font-weight: inherit;
                border: none;
                border-top: solid 1px #ddd;
                border-bottom: solid 1px #ddd;
                white-space: nowrap;
            }

            #platform-list:empty:before {
                content: "No matching platform";
                display: block;
                margin: 1rem 0.5rem;
                text-align: center;
            }

            .config-list label {
                display: block;
                padding: 0.1rem 0.2rem;
            }

            .config-list label:hover,
            .config-list a:hover {
                background: rgba(204, 153, 51, 0.1);
            }

            .config-list label.selected,
            .config-list a.selected {
                background: #eee;
            }

            .config-list a {
                display: block;
                padding: 0.1rem 0.2rem;
                text-decoration: none;
                color: inherit;
            }

            #repository-configuration-pane {
                position: relative;
            }

            #repository-configuration-pane > button  {
                margin-left: 19.5rem;
            }

            .revision-table {
                border: none;
                border-collapse: collapse;
                font-size: 1rem;
            }

            .revision-table thead {
                font-size: 1.2rem;
            }

            .revision-table tbody:empty {
                display: none;
            }

            .revision-table tbody tr:first-child td,
            .revision-table tbody tr:first-child th {
                border-top: solid 1px #ddd;
                padding-top: 0.5rem;
            }

            .revision-table tbody tr:last-child td,
            .revision-table tbody tr:last-child th {
                padding-bottom: 0.5rem;
            }

            .revision-table td,
            .revision-table th {
                width: 15rem;
                height: 1.8rem;
                padding: 0 0.2rem;
                border: none;
                font-weight: inherit;
            }

            .revision-table thead th {
                text-align: center;
            }

            .revision-table th close-button {
                vertical-align: bottom;
            }

            .revision-table td:first-child,
            .revision-table th:first-child {
                width: 6rem;
            }

            .revision-table tr.group-row td {
                padding-left: 5rem;
            }

            label {
                white-space: nowrap;
                display: block;
            }

            input:not([type=radio]) {
                width: calc(100% - 0.6rem);
                padding: 0.1rem 0.2rem;
                font-size: 0.9rem;
                font-weight: inherit;
            }

            #specify-comparison-pane button {
                margin-top: 1.5rem;
                font-size: 1.1rem;
                font-weight: inherit;
            }

            #start-pane button {
                margin-top: 1.5rem;
                font-size: 1.2rem;
                font-weight: inherit;
            }
`;
    }
}

ComponentBase.defineElement('custom-analysis-task-configurator', CustomAnalysisTaskConfigurator);
