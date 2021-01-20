'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const TestServer = require('./resources/test-server.js');
const MockData = require('./resources/mock-data.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/update-triggerable/', function () {
    prepareServerTest(this);

    const emptyUpdate = {
        'slaveName': 'someSlave',
        'slavePassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [],
    };

    const smallUpdate = {
        'slaveName': 'someSlave',
        'slavePassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()}
        ],
    };

    it('should reject when slave name is missing', () => {
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', {}).then((response) => {
            assert.equal(response['status'], 'MissingSlaveName');
        });
    });

    it('should reject when there are no slaves', () => {
        const update = {slaveName: emptyUpdate.slaveName, slavePassword: emptyUpdate.slavePassword};
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update).then((response) => {
            assert.equal(response['status'], 'SlaveNotFound');
        });
    });

    it('should reject when the slave password doesn\'t match', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(emptyUpdate);
        }).then(() => {
            const report = {slaveName: emptyUpdate.slaveName, slavePassword: 'badPassword'};
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it('should accept an empty report', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(emptyUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it('delete existing configurations when accepting an empty report', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return Promise.all([
                addSlaveForReport(emptyUpdate),
                db.insert('triggerable_configurations', {'triggerable': 1000 // build-webkit
                    , 'test': MockData.someTestId(), 'platform': MockData.somePlatformId()})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.equal(rows.length, 0);
        });
    });

    it('should add configurations in the update', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return addSlaveForReport(smallUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', smallUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0]['test'], smallUpdate.configurations[0]['test']);
            assert.equal(rows[0]['platform'], smallUpdate.configurations[0]['platform']);
        });
    });

    it('should reject when a configuration is malformed', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(smallUpdate);
        }).then(() => {
            const update = {
                'slaveName': 'someSlave',
                'slavePassword': 'somePassword',
                'triggerable': 'build-webkit',
                'configurations': [{}],
            };
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidConfigurationEntry');
        });
    });

    function updateWithOSXRepositoryGroup()
    {
        return {
            'slaveName': 'someSlave',
            'slavePassword': 'somePassword',
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
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryGroups');
        });
    });

    it('should reject when the name of a repository group is not specified', () => {
        const update = updateWithOSXRepositoryGroup();
        delete update.repositoryGroups[0].name;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when the repository list is not specified for a repository group', () => {
        const update = updateWithOSXRepositoryGroup();
        delete update.repositoryGroups[0].repositories;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when the repository list of a repository group is not an array', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories = 'hi';
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryGroup');
        });
    });

    it('should reject when a repository group contains a repository data that is not an array', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories[0] = 999;
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryData');
        });
    });

    it('should reject when a repository group contains an invalid repository id', () => {
        const update = updateWithOSXRepositoryGroup();
        update.repositoryGroups[0].repositories[0] = {repository: 999};
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepository');
        });
    });

    it('should reject when a repository group contains a duplicate repository id', () => {
        const update = updateWithOSXRepositoryGroup();
        const group = update.repositoryGroups[0];
        group.repositories.push(group.repositories[0]);
        return MockData.addEmptyTriggerable(TestServer.database()).then(() => {
            return addSlaveForReport(update);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'DuplicateRepository');
        });
    });

    it('should add a new repository group when there are none', () => {
        const db = TestServer.database();
        return MockData.addEmptyTriggerable(db).then(() => {
            return addSlaveForReport(updateWithOSXRepositoryGroup());
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', updateWithOSXRepositoryGroup());
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return Promise.all([db.selectAll('triggerable_configurations', 'test'), db.selectAll('triggerable_repository_groups')]);
        }).then((result) => {
            const [configurations, repositoryGroups] = result;

            assert.equal(configurations.length, 1);
            assert.equal(configurations[0]['test'], MockData.someTestId());
            assert.equal(configurations[0]['platform'], MockData.somePlatformId());

            assert.equal(repositoryGroups.length, 1);
            assert.equal(repositoryGroups[0]['name'], 'system-only');
            assert.equal(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
        });
    });

    it('should not add a duplicate repository group when there is a group of the same name', () => {
        const db = TestServer.database();
        let initialResult;
        return MockData.addEmptyTriggerable(db).then(() => {
            return addSlaveForReport(updateWithOSXRepositoryGroup());
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
            assert.deepEqual(configurations, initialConfigurations);
            assert.deepEqual(repositoryGroups, initialRepositoryGroups);
        })
    });

    it('should not add a duplicate repository group when there is a group of the same name', () => {
        const db = TestServer.database();
        let initialResult;
        return MockData.addEmptyTriggerable(db).then(() => {
            return addSlaveForReport(updateWithOSXRepositoryGroup());
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
            assert.deepEqual(configurations, initialConfigurations);
            assert.deepEqual(repositoryGroups, initialRepositoryGroups);
        })
    });

    it('should update the description of a repository group when the name matches', () => {
        const db = TestServer.database();
        const initialUpdate = updateWithOSXRepositoryGroup();
        const secondUpdate = updateWithOSXRepositoryGroup();
        secondUpdate.repositoryGroups[0].description = 'this group is awesome';
        return MockData.addEmptyTriggerable(db).then(() => {
            return addSlaveForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => db.selectAll('triggerable_repository_groups')).then((repositoryGroups) => {
            assert.equal(repositoryGroups.length, 1);
            assert.equal(repositoryGroups[0]['name'], 'system-only');
            assert.equal(repositoryGroups[0]['description'], null);
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => db.selectAll('triggerable_repository_groups')).then((repositoryGroups) => {
            assert.equal(repositoryGroups.length, 1);
            assert.equal(repositoryGroups[0]['name'], 'system-only');
            assert.equal(repositoryGroups[0]['description'], 'this group is awesome');
        });
    });

    function updateWithMacWebKitRepositoryGroups()
    {
        return {
            'slaveName': 'someSlave',
            'slavePassword': 'somePassword',
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
            return addSlaveForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then(() => Manifest.fetch()).then(() => {
            const repositoryGroups = TriggerableRepositoryGroup.sortByName(TriggerableRepositoryGroup.all());
            const webkit = Repository.findTopLevelByName('WebKit');
            const macos = Repository.findTopLevelByName('macOS');
            assert.equal(repositoryGroups.length, 2);
            assert.equal(repositoryGroups[0].name(), 'system-and-webkit');
            assert.equal(repositoryGroups[0].description(), 'system-and-webkit');
            assert.equal(repositoryGroups[0].acceptsCustomRoots(), false);
            assert.deepEqual(repositoryGroups[0].repositories(), [webkit, macos]);
            assert.equal(repositoryGroups[0].acceptsPatchForRepository(webkit), false);
            assert.equal(repositoryGroups[0].acceptsPatchForRepository(macos), false);

            assert.equal(repositoryGroups[1].name(), 'system-only');
            assert.equal(repositoryGroups[1].description(), 'system-only');
            assert.equal(repositoryGroups[1].acceptsCustomRoots(), false);
            assert.deepEqual(repositoryGroups[1].repositories(), [macos]);
            assert.equal(repositoryGroups[1].acceptsPatchForRepository(webkit), false);
            assert.equal(repositoryGroups[1].acceptsPatchForRepository(macos), false);
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => Manifest.fetch()).then(() => {
            const repositoryGroups = TriggerableRepositoryGroup.sortByName(TriggerableRepositoryGroup.all());
            const webkit = Repository.findTopLevelByName('WebKit');
            const macos = Repository.findTopLevelByName('macOS');
            assert.equal(repositoryGroups.length, 2);
            assert.equal(repositoryGroups[0].name(), 'system-and-webkit');
            assert.equal(repositoryGroups[0].description(), 'system-and-webkit');
            assert.equal(repositoryGroups[0].acceptsCustomRoots(), false);
            assert.deepEqual(repositoryGroups[0].repositories(), [webkit, macos]);
            assert.equal(repositoryGroups[0].acceptsPatchForRepository(webkit), true);
            assert.equal(repositoryGroups[0].acceptsPatchForRepository(macos), false);

            assert.equal(repositoryGroups[1].name(), 'system-only');
            assert.equal(repositoryGroups[1].description(), 'system-only');
            assert.equal(repositoryGroups[1].acceptsCustomRoots(), true);
            assert.deepEqual(repositoryGroups[1].repositories(), [macos]);
            assert.equal(repositoryGroups[1].acceptsPatchForRepository(webkit), false);
            assert.equal(repositoryGroups[1].acceptsPatchForRepository(macos), false);
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
            return addSlaveForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.equal(repositoryGroups.length, 2);
            assert.equal(repositoryGroups[0]['name'], 'system-and-webkit');
            assert.equal(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
            assert.equal(repositoryGroups[1]['name'], 'system-only');
            assert.equal(repositoryGroups[1]['triggerable'], MockData.emptyTriggeragbleId());
            initialGroups = repositoryGroups;

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.equal(Object.keys(repositoriesByGroup).length, 2);
            assert.deepEqual(repositoriesByGroup[repositoryGroups[0]['id']], [MockData.macosRepositoryId(), MockData.webkitRepositoryId()]);
            assert.deepEqual(repositoriesByGroup[repositoryGroups[1]['id']], [MockData.macosRepositoryId()]);

            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.deepEqual(repositoryGroups, initialGroups);

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.equal(Object.keys(repositoriesByGroup).length, 2);
            assert.deepEqual(repositoriesByGroup[initialGroups[0]['id']], [MockData.macosRepositoryId(), MockData.gitWebkitRepositoryId()]);
            assert.deepEqual(repositoriesByGroup[initialGroups[1]['id']], [MockData.macosRepositoryId()]);
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
            return addSlaveForReport(initialUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', initialUpdate);
        }).then((response) => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;
            assert.equal(repositoryGroups.length, 2);
            assert.equal(repositoryGroups[0]['name'], 'system-and-webkit');
            assert.equal(repositoryGroups[0]['triggerable'], MockData.emptyTriggeragbleId());
            assert.equal(repositoryGroups[1]['name'], 'system-only');
            assert.equal(repositoryGroups[1]['triggerable'], MockData.emptyTriggeragbleId());
            initialGroups = repositoryGroups;

            const repositoriesByGroup = mapRepositoriesByGroup(repositories);
            assert.equal(Object.keys(repositoriesByGroup).length, 2);
            assert.deepEqual(repositoriesByGroup[repositoryGroups[0]['id']], [MockData.macosRepositoryId(), MockData.webkitRepositoryId()]);
            assert.deepEqual(repositoriesByGroup[repositoryGroups[1]['id']], [MockData.macosRepositoryId()]);
            initialRepositories = repositories;

            return TestServer.remoteAPI().postJSONWithStatus('/api/update-triggerable/', secondUpdate);
        }).then(() => {
            return Promise.all([db.selectAll('triggerable_repository_groups', 'name'), db.selectAll('triggerable_repositories', 'repository')]);
        }).then((result) => {
            const [repositoryGroups, repositories] = result;

            assert.equal(repositoryGroups.length, 2);
            assert.equal(repositoryGroups[0]['name'], 'mac-only');
            assert.equal(repositoryGroups[0]['triggerable'], initialGroups[1]['triggerable']);
            assert.equal(repositoryGroups[1]['name'], 'system-and-webkit');
            assert.equal(repositoryGroups[1]['triggerable'], initialGroups[0]['triggerable']);

            assert.deepEqual(repositories, initialRepositories);
        });
    });

});
