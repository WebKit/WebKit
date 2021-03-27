'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const TestServer = require('./resources/test-server.js');
const MockData = require('./resources/mock-data.js');
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/update-triggerable/', function () {
    prepareServerTest(this);

    const emptyUpdate = {
        'workerName': 'someWorker',
        'workerPassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [],
    };

    const smallUpdate = {
        'workerName': 'someWorker',
        'workerPassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()}
        ],
    };

    it('should reject when worker name is missing', () => {
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', {}).then((response) => {
            assert.strictEqual(response['status'], 'MissingWorkerName');
        });
    });

    it('should reject when there are no workers', () => {
        const update = {workerName: emptyUpdate.workerName, workerPassword: emptyUpdate.workerPassword};
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update).then((response) => {
            assert.strictEqual(response['status'], 'WorkerNotFound');
        });
    });

    it('should reject when the worker password doesn\'t match', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addWorkerForReport(emptyUpdate);
        }).then(() => {
            const report = {workerName: emptyUpdate.workerName, workerPassword: 'badPassword'};
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
        });
    });

    it('should accept an empty report', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addWorkerForReport(emptyUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
        });
    });

    it('delete existing configurations when accepting an empty report', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return Promise.all([
                addWorkerForReport(emptyUpdate),
                db.insert('triggerable_configurations', {'triggerable': 1000 // build-webkit
                    , 'test': MockData.someTestId(), 'platform': MockData.somePlatformId()})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.strictEqual(rows.length, 0);
        });
    });

    it('should add configurations in the update', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return addWorkerForReport(smallUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', smallUpdate);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0]['test'], smallUpdate.configurations[0]['test']);
            assert.strictEqual(rows[0]['platform'], smallUpdate.configurations[0]['platform']);
        });
    });

    it('should reject when a configuration is malformed', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addWorkerForReport(smallUpdate);
        }).then(() => {
            const update = {
                'workerName': 'someWorker',
                'workerPassword': 'somePassword',
                'triggerable': 'build-webkit',
                'configurations': [{}],
            };
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidConfigurationEntry');
        });
    });

    function updateWithOSXRepositoryGroup()
    {
        return {
            'workerName': 'someWorker',
            'workerPassword': 'somePassword',
            'triggerable': 'empty-triggerable',
            'configurations': [
                {test: MockData.someTestId(), platform: MockData.somePlatformId()}
            ],
            'repositoryGroups': [
                {name: 'system-only', repositories: [
                    {repository: MockData.macosRepositoryId(), acceptsPatch: false},
                ]},
            ]
        };
    }

    it('should reject when repositoryGroups is not an array', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups = 1;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryGroups');
        });
    });

    it('should reject when the name of a repository group is not specified', () => {
        const update = updateWithOSXRepositoryGroup();
        delete update.repositoryGroups[0].name;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when the repository list is not specified for a repository group', () => {
        const update = updateWithOSXRepositoryGroup();
        delete update.repositoryGroups[0].repositories;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when the repository list of a repository group is not an array', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories = 'hi';
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when a repository group contains a repository data that is not an array', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories[0] = 999;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryData');
        });
    });

    it('should reject when a repository group contains an invalid repository id', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories[0] = {repository: 999};
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepository');
        });
    });

    it('should reject when a repository group contains a duplicate repository id', () => {
        const update = updateWithOSXRepositoryGroup();
        const group = update.repositoryGroups[0];
        group.repositories.push(group.repositories[0]);
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addWorkerForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.strictEqual(response['status'], 'DuplicateRepository');
        });
    });

    it('should add a new repository group when there are none', () => {
        const db = TestServer.database();
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(updateWithOSXRepositoryGroup());
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            const [configurations, repositoryGroups] = result;

            assert.strictEqual(configurations.length, 1);
            assert.strictEqual(configurations[0]['test'], MockData.someTestId());
            assert.strictEqual(configurations[0]['platform'], MockData.somePlatformId());

            assert.strictEqual(repositoryGroups.length, 1);
            assert.strictEqual(repositoryGroups[0]['name'], 'system-only');
            assert.strictEqual(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
        });
    });

    it('should not add a duplicate repository group when there is a group of the same name', () => {
        const db = TestServer.database();
        let initialResult;
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(updateWithOSXRepositoryGroup());
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            initialResult = result;
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            const [initialConfigurations, initialRepositoryGroups] = initialResult;
            const [configurations, repositoryGroups] = result;
            assert.deepStrictEqual(configurations, initialConfigurations);
            assert.deepStrictEqual(repositoryGroups, initialRepositoryGroups);
        })
    });

    it('should not add a duplicate repository group when there is a group of the same name', () => {
        const db = TestServer.database();
        let initialResult;
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(updateWithOSXRepositoryGroup());
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            initialResult = result;
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            const [initialConfigurations, initialRepositoryGroups] = initialResult;
            const [configurations, repositoryGroups] = result;
            assert.deepStrictEqual(configurations, initialConfigurations);
            assert.deepStrictEqual(repositoryGroups, initialRepositoryGroups);
        })
    });

    it('should update the description of a repository group when the name matches', () => {
        const db = TestServer.database();
        const initialUpdate = updateWithOSXRepositoryGroup();
        const secondUpdate = updateWithOSXRepositoryGroup();
        secondUpdate.repositoryGroups[0].description = 'this group is awesome';
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => db.selectAll('triggerable_repository_groups')).then((repositoryGroups) => {
            assert.strictEqual(repositoryGroups.length, 1);
            assert.strictEqual(repositoryGroups[0]['name'], 'system-only');
            assert.strictEqual(repositoryGroups[0]['description'], null);
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => db.selectAll('triggerable_repository_groups')).then((repositoryGroups) => {
            assert.strictEqual(repositoryGroups.length, 1);
            assert.strictEqual(repositoryGroups[0]['name'], 'system-only');
            assert.strictEqual(repositoryGroups[0]['description'], 'this group is awesome');
        });
    });

    function updateWithMacWebKitRepositoryGroups()
    {
        return {
            'workerName': 'someWorker',
            'workerPassword': 'somePassword',
            'triggerable': 'empty-triggerable',
            'configurations': [
                {test: MockData.someTestId(), platform: MockData.somePlatformId()}
            ],
            'repositoryGroups': [
                {name: 'system-only', repositories: [{repository: MockData.macosRepositoryId()}]},
                {name: 'system-and-webkit', repositories:
                    [{repository: MockData.webkitRepositoryId()}, {repository: MockData.macosRepositoryId()}]},
            ]
        };
    }

    function mapRepositoriesByGroup(repositories)
    {
        const map = {};
        for (const row of repositories) {
            const groupId = row['group'];
            if (!(groupId in map))
                map[groupId] = [];
            map[groupId].push(row['repository']);
        }
        return map;
    }

    it('should update the acceptable of custom roots and patches', () => {
        const db = TestServer.database();
        const initialUpdate = updateWithMacWebKitRepositoryGroups();
        const secondUpdate = updateWithMacWebKitRepositoryGroups();
        secondUpdate.repositoryGroups[0].acceptsRoots = true;
        secondUpdate.repositoryGroups[1].repositories[0].acceptsPatch = true;
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then(() => Manifest.fetch()).then(() => {
            const repositoryGroups = TriggerableRepositoryGroup.sortByName(TriggerableRepositoryGroup.all());
            const webkit = Repository.findTopLevelByName('WebKit');
            const macos = Repository.findTopLevelByName('macOS');
            assert.strictEqual(repositoryGroups.length, 2);
            assert.strictEqual(repositoryGroups[0].name(), 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0].description(), 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0].acceptsCustomRoots(), false);
            assert.deepStrictEqual(repositoryGroups[0].repositories(), [webkit, macos]);
            assert.strictEqual(repositoryGroups[0].acceptsPatchForRepository(webkit), false);
            assert.strictEqual(repositoryGroups[0].acceptsPatchForRepository(macos), false);

            assert.strictEqual(repositoryGroups[1].name(), 'system-only');
            assert.strictEqual(repositoryGroups[1].description(), 'system-only');
            assert.strictEqual(repositoryGroups[1].acceptsCustomRoots(), false);
            assert.deepStrictEqual(repositoryGroups[1].repositories(), [macos]);
            assert.strictEqual(repositoryGroups[1].acceptsPatchForRepository(webkit), false);
            assert.strictEqual(repositoryGroups[1].acceptsPatchForRepository(macos), false);
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => Manifest.fetch()).then(() => {
            const repositoryGroups = TriggerableRepositoryGroup.sortByName(TriggerableRepositoryGroup.all());
            const webkit = Repository.findTopLevelByName('WebKit');
            const macos = Repository.findTopLevelByName('macOS');
            assert.strictEqual(repositoryGroups.length, 2);
            assert.strictEqual(repositoryGroups[0].name(), 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0].description(), 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0].acceptsCustomRoots(), false);
            assert.deepStrictEqual(repositoryGroups[0].repositories(), [webkit, macos]);
            assert.strictEqual(repositoryGroups[0].acceptsPatchForRepository(webkit), true);
            assert.strictEqual(repositoryGroups[0].acceptsPatchForRepository(macos), false);

            assert.strictEqual(repositoryGroups[1].name(), 'system-only');
            assert.strictEqual(repositoryGroups[1].description(), 'system-only');
            assert.strictEqual(repositoryGroups[1].acceptsCustomRoots(), true);
            assert.deepStrictEqual(repositoryGroups[1].repositories(), [macos]);
            assert.strictEqual(repositoryGroups[1].acceptsPatchForRepository(webkit), false);
            assert.strictEqual(repositoryGroups[1].acceptsPatchForRepository(macos), false);
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        });
    });

    it('should replace a repository when the repository group name matches', () => {
        const db = TestServer.database();
        const initialUpdate = updateWithMacWebKitRepositoryGroups();
        const secondUpdate = updateWithMacWebKitRepositoryGroups();
        let initialGroups;
        secondUpdate.repositoryGroups[1].repositories[0] = {repository: MockData.gitWebkitRepositoryId()}
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.strictEqual(repositoryGroups.length, 2);
            assert.strictEqual(repositoryGroups[0]['name'], 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
            assert.strictEqual(repositoryGroups[1]['name'], 'system-only');
            assert.strictEqual(repositoryGroups[1]['triggerable'], MockData.emptyTriggeragbleId());
            initialGroups = repositoryGroups;

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.strictEqual(Object.keys(repositoriesByGroup).length, 2);
            assert.deepStrictEqual(repositoriesByGroup[repositoryGroups[0]['id']], [MockData.macosRepositoryId(), MockData.webkitRepositoryId()]);
            assert.deepStrictEqual(repositoriesByGroup[repositoryGroups[1]['id']], [MockData.macosRepositoryId()]);

            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.deepStrictEqual(repositoryGroups, initialGroups);

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.strictEqual(Object.keys(repositoriesByGroup).length, 2);
            assert.deepStrictEqual(repositoriesByGroup[initialGroups[0]['id']], [MockData.macosRepositoryId(), MockData.gitWebkitRepositoryId()]);
            assert.deepStrictEqual(repositoriesByGroup[initialGroups[1]['id']], [MockData.macosRepositoryId()]);
        });
    });

    it('should replace a repository when the list of repositories matches', () => {
        const db = TestServer.database();
        const initialUpdate = updateWithMacWebKitRepositoryGroups();
        const secondUpdate = updateWithMacWebKitRepositoryGroups();
        let initialGroups;
        let initialRepositories;
        secondUpdate.repositoryGroups[0].name = 'mac-only';
        return MockData.addEmptyTriggerable(db).then(() => {
            return addWorkerForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.strictEqual(repositoryGroups.length, 2);
            assert.strictEqual(repositoryGroups[0]['name'], 'system-and-webkit');
            assert.strictEqual(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
            assert.strictEqual(repositoryGroups[1]['name'], 'system-only');
            assert.strictEqual(repositoryGroups[1]['triggerable'], MockData.emptyTriggeragbleId());
            initialGroups = repositoryGroups;

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.strictEqual(Object.keys(repositoriesByGroup).length, 2);
            assert.deepStrictEqual(repositoriesByGroup[repositoryGroups[0]['id']], [MockData.macosRepositoryId(), MockData.webkitRepositoryId()]);
            assert.deepStrictEqual(repositoriesByGroup[repositoryGroups[1]['id']], [MockData.macosRepositoryId()]);
            initialRepositories = repositories;

            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;

            assert.strictEqual(repositoryGroups.length, 2);
            assert.strictEqual(repositoryGroups[0]['name'], 'mac-only');
            assert.strictEqual(repositoryGroups[0]['triggerable'], initialGroups[1]['triggerable']);
            assert.strictEqual(repositoryGroups[1]['name'], 'system-and-webkit');
            assert.strictEqual(repositoryGroups[1]['triggerable'], initialGroups[0]['triggerable']);

            assert.deepStrictEqual(repositories, initialRepositories);
        });
    });

});
