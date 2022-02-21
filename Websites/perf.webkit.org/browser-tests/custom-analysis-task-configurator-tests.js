describe('CustomAnalysisTaskConfigurator', () => {
    const scripts = ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js',
        'models/data-model.js', 'models/commit-log.js', 'models/commit-set.js', 'models/repository.js',
        'models/metric.js', 'models/triggerable.js', 'models/test.js', 'models/platform.js',
        'components/test-group-form.js', 'components/custom-analysis-task-configurator.js',
        'components/instant-file-uploader.js', 'lazily-evaluated-function.js'];

    async function createCustomAnalysisTaskConfiguratorWithContext(context)
    {
        await context.importScripts(scripts, 'ComponentBase', 'LabeledObject', 'DataModelObject', 'Repository',
            'CommitLog', 'Platform', 'Test', 'Metric', 'Triggerable', 'TriggerableConfiguration',
            'TriggerableRepositoryGroup', 'CustomCommitSet', 'CustomAnalysisTaskConfigurator', 'MockRemoteAPI',
            'LazilyEvaluatedFunction');
        const customAnalysisTaskConfigurator = new context.symbols.CustomAnalysisTaskConfigurator;
        context.document.body.appendChild(customAnalysisTaskConfigurator.element());
        return customAnalysisTaskConfigurator;
    }

    async function sleep(timeout)
    {
        await new Promise((resolve) => setTimeout(() => resolve(), timeout));
    }

    it('Should be able to schedule A/B test even fetching commit information never returns', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);
        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 1;

        const test = new context.symbols.Test(1, {name: 'Speedometer'});
        const platform = new context.symbols.Platform(1, {
            name: 'Mojave',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const triggerableRepositoryGroup = new context.symbols.TriggerableRepositoryGroup(1, {repositories: [{repository}]});
        new context.symbols.Triggerable(1, {
            name: 'test-triggerable',
            isDisabled: false,
            repositoryGroups: [triggerableRepositoryGroup],
            configurations: [{test, platform, supportedRepetitionTypes: ['alternating', 'sequential']}],
        });
        customAnalysisTaskConfigurator.selectTests([test]);
        customAnalysisTaskConfigurator.selectPlatform(platform);

        await waitForComponentsToRender(context);

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(1);
        expect(requests[0].url).to.be('/api/commits/1/latest?platform=1');
        requests[0].reject();

        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value = '123';
        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(2);
        expect(requests[1].url).to.be('/api/commits/1/123?prefix-match=true');

        customAnalysisTaskConfigurator._configureComparison();
        await waitForComponentsToRender(context);

        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').value = '456';
        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(3);
        expect(requests[2].url).to.be('/api/commits/1/456?prefix-match=true');

        const commitSets = customAnalysisTaskConfigurator.commitSets();
        expect(commitSets.length).to.be(2);
        expect(commitSets[0].repositories().length).to.be(1);
        expect(commitSets[0].revisionForRepository(repository)).to.be('123');
        expect(commitSets[1].repositories().length).to.be(1);
        expect(commitSets[1].revisionForRepository(repository)).to.be('456');

        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 100;
    });

    const webkitCommit = {
        "id": "185338",
        "revision": "abcdefg",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": +new Date("2017-01-20T03:49:37.887Z"),
        "authorName": "Commit Queue",
        "authorEmail": "commit-queue@webkit.org",
        "message": "another message",
    };

    const anotherWebkitCommit = {
        "id": "185334",
        "revision": "aabcdef",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": +new Date("2017-01-20T03:23:50.645Z"),
        "authorName": "Chris Dumez",
        "authorEmail": "cdumez@apple.com",
        "message": "some message",
    };

    it('Should use full commit revision even if UI does not have the full revision', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);
        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 1;

        const test = new context.symbols.Test(1, {name: 'Speedometer'});
        const platform = new context.symbols.Platform(1, {
            name: 'Mojave',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const triggerableRepositoryGroup = new context.symbols.TriggerableRepositoryGroup(1, {repositories: [{repository}]});
        new context.symbols.Triggerable(1, {
            name: 'test-triggerable',
            isDisabled: false,
            repositoryGroups: [triggerableRepositoryGroup],
            configurations: [{test, platform, supportedRepetitionTypes: ['alternating', 'sequential']}],
        });
        customAnalysisTaskConfigurator.selectTests([test]);
        customAnalysisTaskConfigurator.selectPlatform(platform);

        await waitForComponentsToRender(context);

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(1);
        expect(requests[0].url).to.be('/api/commits/1/latest?platform=1');
        requests[0].reject();

        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value = 'abc';
        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(2);
        expect(requests[1].url).to.be('/api/commits/1/abc?prefix-match=true');
        requests[1].resolve({commits: [webkitCommit]});

        customAnalysisTaskConfigurator._configureComparison();
        await waitForComponentsToRender(context);

        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').value = 'aabc';
        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(3);
        expect(requests[2].url).to.be('/api/commits/1/aabc?prefix-match=true');
        requests[2].resolve({commits: [anotherWebkitCommit]});

        await waitForComponentsToRender(context);
        const commitSets = customAnalysisTaskConfigurator.commitSets();
        expect(commitSets.length).to.be(2);
        expect(commitSets[0].repositories().length).to.be(1);
        expect(commitSets[0].revisionForRepository(repository)).to.be('abcdefg');
        expect(commitSets[1].repositories().length).to.be(1);
        expect(commitSets[1].revisionForRepository(repository)).to.be('aabcdef');

        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 100;
    });

    it('Should not update commitSetMap if baseline is set and unmodified but comparison is null', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);

        await waitForComponentsToRender(context);
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const commitSet = new context.symbols.CustomCommitSet;
        commitSet.setRevisionForRepository(repository, '210948', null);
        customAnalysisTaskConfigurator._commitSetMap = {'Baseline': commitSet, 'Comparison': null};
        customAnalysisTaskConfigurator._repositoryGroupByConfiguration['Baseline'] = new context.symbols.TriggerableRepositoryGroup(1, {repositories: [{repository}]});
        customAnalysisTaskConfigurator._specifiedRevisions['Baseline'].set(repository, '210948');

        const originalCommitSet = customAnalysisTaskConfigurator._commitSetMap;
        await customAnalysisTaskConfigurator._updateCommitSetMap();

        expect(customAnalysisTaskConfigurator._commitSetMap).to.be(originalCommitSet);
    });

    it('Should preserve user specified revision if user has ever modified the revision', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);
        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 1;

        const test = new context.symbols.Test(1, {name: 'Speedometer'});
        const mojave = new context.symbols.Platform(1, {
            name: 'Mojave',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const highSierra = new context.symbols.Platform(2, {
            name: 'High Sierra',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const triggerableRepositoryGroup = new context.symbols.TriggerableRepositoryGroup(1, {repositories: [{repository}]});
        new context.symbols.Triggerable(1, {
            name: 'test-triggerable',
            isDisabled: false,
            repositoryGroups: [triggerableRepositoryGroup],
            configurations: [{test, platform: mojave, supportedRepetitionTypes: ['alternating', 'sequential']},
                {test, platform: highSierra, supportedRepetitionTypes: ['alternating', 'sequential']}],
        });
        customAnalysisTaskConfigurator.selectTests([test]);
        customAnalysisTaskConfigurator.selectPlatform(mojave);

        await waitForComponentsToRender(context);

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(1);
        expect(requests[0].url).to.be('/api/commits/1/latest?platform=1');
        requests[0].reject();

        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value = '123';
        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(2);
        expect(requests[1].url).to.be('/api/commits/1/123?prefix-match=true');

        customAnalysisTaskConfigurator._configureComparison();
        await waitForComponentsToRender(context);

        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').value = '456';
        customAnalysisTaskConfigurator.content('comparison-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(3);
        expect(requests[2].url).to.be('/api/commits/1/456?prefix-match=true');

        let commitSets = customAnalysisTaskConfigurator.commitSets();
        expect(commitSets.length).to.be(2);
        expect(commitSets[0].repositories().length).to.be(1);
        expect(commitSets[0].revisionForRepository(repository)).to.be('123');
        expect(commitSets[1].repositories().length).to.be(1);
        expect(commitSets[1].revisionForRepository(repository)).to.be('456');

        customAnalysisTaskConfigurator.selectPlatform(highSierra);
        await waitForComponentsToRender(context);

        commitSets = customAnalysisTaskConfigurator.commitSets();
        expect(commitSets.length).to.be(2);
        expect(commitSets[0].repositories().length).to.be(1);
        expect(commitSets[0].revisionForRepository(repository)).to.be('123');
        expect(commitSets[1].repositories().length).to.be(1);
        expect(commitSets[1].revisionForRepository(repository)).to.be('456');
        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 100;
    });

    it('Should reset commit set if user has never modified the revision', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);

        const test = new context.symbols.Test(1, {name: 'Speedometer'});
        const mojave = new context.symbols.Platform(1, {
            name: 'Mojave',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const highSierra = new context.symbols.Platform(2, {
            name: 'High Sierra',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const triggerableRepositoryGroup = new context.symbols.TriggerableRepositoryGroup(1, {name: 'test-triggerable', repositories: [{repository}]});
        new context.symbols.Triggerable(1, {
            name: 'test-triggerable',
            isDisabled: false,
            repositoryGroups: [triggerableRepositoryGroup],
            configurations: [{test, platform: mojave, supportedRepetitionTypes: ['alternating', 'sequential']},
                {test, platform: highSierra, supportedRepetitionTypes: ['alternating', 'sequential']}],
        });
        customAnalysisTaskConfigurator.selectTests([test]);
        customAnalysisTaskConfigurator.selectPlatform(mojave);
        await waitForComponentsToRender(context);

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(1);
        expect(requests[0].url).to.be('/api/commits/1/latest?platform=1');
        requests[0].resolve({commits: [{
            id: 1,
            revision: '123',
            repository: repository.id(),
            time: Date.now(),
        }]});
        await waitForComponentsToRender(context);

        expect(customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value).to.be('123');
        customAnalysisTaskConfigurator.selectPlatform(highSierra);

        await waitForComponentsToRender(context);
        expect(customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value).to.be('');
    });

    it('Should dispatch "testConfigChange" action when selected platform changed', async () => {
        const context = new BrowsingContext();
        const customAnalysisTaskConfigurator = await createCustomAnalysisTaskConfiguratorWithContext(context);
        let testConfigChangeActionCount = 0;
        context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval = 1;

        customAnalysisTaskConfigurator.listenToAction("testConfigChange", () => testConfigChangeActionCount += 1);

        const test = new context.symbols.Test(1, {name: 'Speedometer'});
        const mojave = new context.symbols.Platform(1, {
            name: 'Mojave',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const highSierra = new context.symbols.Platform(2, {
            name: 'High Sierra',
            metrics: [
                new context.symbols.Metric(1, {
                    name: 'Allocation',
                    aggregator: 'Arithmetic',
                    test
                })
            ],
            lastModifiedByMetric: Date.now(),
        });
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});
        const triggerableRepositoryGroup = new context.symbols.TriggerableRepositoryGroup(1, {repositories: [{repository}]});
        new context.symbols.Triggerable(1, {
            name: 'test-triggerable',
            isDisabled: false,
            repositoryGroups: [triggerableRepositoryGroup],
            configurations: [{test, platform: mojave, supportedRepetitionTypes: ['alternating', 'sequential']},
                {test, platform: highSierra, supportedRepetitionTypes: ['alternating', 'sequential']}],
        });
        customAnalysisTaskConfigurator.selectTests([test]);
        customAnalysisTaskConfigurator.selectPlatform(mojave);

        await waitForComponentsToRender(context);
        expect(testConfigChangeActionCount).to.be(2);

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(1);
        expect(requests[0].url).to.be('/api/commits/1/latest?platform=1');
        requests[0].reject();

        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').value = '123';
        customAnalysisTaskConfigurator.content('baseline-revision-table').querySelector('input').dispatchEvent(new Event('input'));
        await sleep(context.symbols.CustomAnalysisTaskConfigurator.commitFetchInterval);
        expect(requests.length).to.be(2);
        expect(requests[1].url).to.be('/api/commits/1/123?prefix-match=true');

        customAnalysisTaskConfigurator._configureComparison();
        await waitForComponentsToRender(context);
        expect(testConfigChangeActionCount).to.be(3);

        customAnalysisTaskConfigurator.selectPlatform(highSierra);
        await waitForComponentsToRender(context);
        expect(testConfigChangeActionCount).to.be(4);
    });
});