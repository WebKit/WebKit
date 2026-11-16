'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const addWorkerForReport = require('./resources/common-operations.js').addWorkerForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

function makeReport(rootFile, buildRequestId = 1, repositoryList = ['WebKit'], buildTime = '2017-05-10T02:54:08.666')
{
    return {
        workerName: 'someWorker',
        workerPassword: 'somePassword',
        builderName: 'someBuilder',
        buildTag: "123",
        buildTime: buildTime,
        buildRequest: buildRequestId,
        rootFile: rootFile,
        repositoryList: JSON.stringify(repositoryList),
    };
}

function addWorkerAndCreateRootFile(workerInfo = makeReport())
{
    return addWorkerForReport(workerInfo).then(() => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
    });
}

function createTestGroupWihPatch()
{
    const triggerableConfiguration = {
        'workerName': 'sync-worker',
        'workerPassword': 'password',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId(), supportedRepetitionTypes: ['alternating', 'sequential']},
        ],
        'repositoryGroups': [
            {name: 'webkit', repositories: [
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true},
                {repository: MockData.sharedRepositoryId()},
            ]}
        ]
    };

    const db = TestServer.database();
    return MockData.addMockData(db).then(() => {
        return Promise.all([TemporaryFile.makeTemporaryFile('patch.dat', 'patch file'), Manifest.fetch()]);
    }).then((result) => {
        const patchFile = result[0];
        return Promise.all([UploadedFile.uploadFile(patchFile), TestServer.remoteAPI().postJSON('/api/update-triggerable/', triggerableConfiguration)]);
    }).then((result) => {
        const patchFile = result[0];
        const someTest = Test.findById(MockData.someTestId());
        const webkit = Repository.findById(MockData.webkitRepositoryId());
        const shared = Repository.findById(MockData.sharedRepositoryId());
        const set1 = new CustomCommitSet;
        set1.setRevisionForRepository(webkit, '191622', patchFile);
        set1.setRevisionForRepository(shared, '80229');
        const set2 = new CustomCommitSet;
        set2.setRevisionForRepository(webkit, '191622');
        set2.setRevisionForRepository(shared, '80229');
        return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, 'alternating', [set1, set2]);
    }).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    }).then((group) => {
        return db.query('UPDATE analysis_test_groups SET testgroup_author = $1', ['someUser']).then(() => group);
    });
}

function createTestGroupWithPatchAndOwnedCommits()
{
    const triggerableConfiguration = {
        'workerName': 'sync-worker',
        'workerPassword': 'password',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId(), supportedRepetitionTypes: ['alternating', 'sequential']},
        ],
        'repositoryGroups': [
            {name: 'webkit', repositories: [
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true}
            ]}
        ]
    };

    const db = TestServer.database();
    return MockData.addMockData(db).then(() => {
        return Promise.all([TemporaryFile.makeTemporaryFile('patch.dat', 'patch file'), Manifest.fetch()]);
    }).then((result) => {
        const patchFile = result[0];
        return Promise.all([UploadedFile.uploadFile(patchFile), TestServer.remoteAPI().postJSON('/api/update-triggerable/', triggerableConfiguration)]);
    }).then((result) => {
        const patchFile = result[0];
        const someTest = Test.findById(MockData.someTestId());
        const webkit = Repository.findById(MockData.webkitRepositoryId());
        const ownedSJC = Repository.findById(MockData.ownedJSCRepositoryId());
        const set1 = new CustomCommitSet;
        set1.setRevisionForRepository(webkit, '191622', patchFile);
        set1.setRevisionForRepository(ownedSJC, 'owned-jsc-6161', null, '191622');
        const set2 = new CustomCommitSet;
        set2.setRevisionForRepository(webkit, '192736');
        set2.setRevisionForRepository(ownedSJC, 'owned-jsc-9191', null, '192736');

        return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, 'alternating', [set1, set2]);
    }).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    }).then((group) => {
        return db.query('UPDATE analysis_test_groups SET testgroup_author = $1', ['someUser']).then(() => group);
    });
}

describe('/api/upload-root/', function () {
    prepareServerTest(this);
    TemporaryFile.inject();

    it('should reject when root file is missing', () => {
        return TestServer.remoteAPI().postFormData('/api/upload-root/', {}).then((response) => {
            assert.strictEqual(response['status'], 'NoFileSpecified');
        });
    });

    it('should reject when there are no workers', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile));
        }).then((response) => {
            assert.strictEqual(response['status'], 'WorkerNotFound');
        });
    });

    it('should reject when worker name is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.workerName;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'MissingWorkerName');
        });
    });

    it('should reject when builder name is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.builderName;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuilderName');
        });
    });

    it('should reject when build tag is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildTag;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildTag');
        });
    });

    it('should reject when build number is not a number', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildTag;
            report.buildNumber = '1abc';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildNumber');
        });
    });

    it('should reject if both build number and build tag exists but different', async () => {
        const rootFile = await addWorkerAndCreateRootFile();
        const report = makeReport(rootFile);
        report.buildNumber = '456';
        const response = await TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        assert.strictEqual(response['status'], 'BuilderNumberTagMismatch');
    });

    it('should accept if both build number and build tag exists with same value', async () => {
        const testGroup = await createTestGroupWithPatchAndOwnedCommits();
        const webkit = Repository.findById(MockData.webkitRepositoryId());
        const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());

        const buildRequest = testGroup.buildRequests()[0];
        assert.strictEqual(testGroup.buildRequests().length, 6);
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert(!buildRequest.hasFinished());
        assert(buildRequest.isPending());
        assert.strictEqual(buildRequest.buildId(), null);

        const commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        const webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        assert.strictEqual(commitSet.rootForRepository(webkit), null);
        assert.strictEqual(commitSet.revisionForRepository(ownedJSC), 'owned-jsc-6161');
        assert.strictEqual(commitSet.patchForRepository(ownedJSC), null);
        assert.strictEqual(commitSet.rootForRepository(ownedJSC), null);
        assert.deepStrictEqual(commitSet.allRootFiles(), []);

        const otherBuildRequest = testGroup.buildRequests()[1];
        const otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.revisionForRepository(ownedJSC), 'owned-jsc-9191');
        assert.strictEqual(otherCommitSet.patchForRepository(ownedJSC), null);
        assert.strictEqual(otherCommitSet.rootForRepository(ownedJSC), null);
        assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

        const rootFile = await addWorkerAndCreateRootFile();
        const report = makeReport(rootFile, buildRequest.id(), ['WebKit']);
        report.buildNumber = report.buildTag;
        const response = await TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        assert.strictEqual(response['status'], 'OK');
    });

    it('should continue working with build number if build tag is not present', async () => {
        const testGroup = await createTestGroupWithPatchAndOwnedCommits();
        const webkit = Repository.findById(MockData.webkitRepositoryId());
        const ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());

        const buildRequest = testGroup.buildRequests()[0];
        assert.strictEqual(testGroup.buildRequests().length, 6);
        assert(buildRequest.isBuild());
        assert(!buildRequest.isTest());
        assert(!buildRequest.hasFinished());
        assert(buildRequest.isPending());
        assert.strictEqual(buildRequest.buildId(), null);

        const commitSet = buildRequest.commitSet();
        assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
        const webkitPatch = commitSet.patchForRepository(webkit);
        assert(webkitPatch instanceof UploadedFile);
        assert.strictEqual(webkitPatch.filename(), 'patch.dat');
        assert.strictEqual(commitSet.rootForRepository(webkit), null);
        assert.strictEqual(commitSet.revisionForRepository(ownedJSC), 'owned-jsc-6161');
        assert.strictEqual(commitSet.patchForRepository(ownedJSC), null);
        assert.strictEqual(commitSet.rootForRepository(ownedJSC), null);
        assert.deepStrictEqual(commitSet.allRootFiles(), []);

        const otherBuildRequest = testGroup.buildRequests()[1];
        const otherCommitSet = otherBuildRequest.commitSet();
        assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
        assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
        assert.strictEqual(otherCommitSet.revisionForRepository(ownedJSC), 'owned-jsc-9191');
        assert.strictEqual(otherCommitSet.patchForRepository(ownedJSC), null);
        assert.strictEqual(otherCommitSet.rootForRepository(ownedJSC), null);
        assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

        const rootFile = await addWorkerAndCreateRootFile();
        const report = makeReport(rootFile, buildRequest.id(), ['WebKit']);
        delete report.buildTag;
        report.buildNumber = '123';
        const response = await TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        assert.strictEqual(response['status'], 'OK');
    });

    it('should reject when build time is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildTime;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildTime');
        });
    });

    it('should reject when build time is malformed', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.buildTime = 'Wed, 10 May 2017 03:02:30 GMT';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildTime');
        });
    });

    it('should reject when build request ID is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildRequest;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildRequest');
        });
    });

    it('should reject when build request ID is not a number', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.buildRequest = 'abc';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildRequest');
        });
    });

    it('should reject when build request does not exist', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile));
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidBuildRequestType');
        });
    });

    it('should reject when repository list is missing', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.repositoryList;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryList');
        });
    });

    it('should reject when repository list is not a valid JSON string', () => {
        return addWorkerAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.repositoryList = 'a+b';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryList');
        });
    });

    it('should reject when using invalid key to specify an owned repository', () => {
        let webkit;
        let webkitPatch;
        let ownedJSC;
        let testGroup;
        let buildRequest;
        let otherBuildRequest;
        let uploadedRoot;
        return createTestGroupWithPatchAndOwnedCommits().then((group) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());

            testGroup = group;
            buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 6);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending());
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.strictEqual(commitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(commitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(ownedJSC), 'owned-jsc-9191');
            assert.strictEqual(otherCommitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(otherCommitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), ['WebKit']);
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert.strictEqual(buildRequest.buildId(), null);

            assert.deepStrictEqual(buildRequest.commitSet().allRootFiles(), [uploadedRoot]);
            assert.deepStrictEqual(otherBuildRequest.commitSet().allRootFiles(), []);
            return TemporaryFile.makeTemporaryFile('JavaScriptCore-Root.dat', 'JavaScript Content 0');
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), [{ownerRepositoryWrongKey: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidKeyForRepository');
        });
    });

    it('should reject when reporting an invalid owned repository', () => {
        let webkit;
        let webkitPatch;
        let ownedJSC;
        let testGroup;
        let buildRequest;
        let otherBuildRequest;
        let uploadedRoot;
        return createTestGroupWithPatchAndOwnedCommits().then((group) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());

            testGroup = group;
            buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 6);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending());
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.strictEqual(commitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(commitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(ownedJSC), 'owned-jsc-9191');
            assert.strictEqual(otherCommitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(otherCommitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), ['WebKit']);
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert.strictEqual(buildRequest.buildId(), null);

            assert.deepStrictEqual(buildRequest.commitSet().allRootFiles(), [uploadedRoot]);
            assert.deepStrictEqual(otherBuildRequest.commitSet().allRootFiles(), []);
            return TemporaryFile.makeTemporaryFile('JavaScriptCore-Root.dat', 'JavaScript Content 0');
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), [{ownerRepository: 'WebKit2', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepository');
        });
    });

    it('should accept when build request exists', () => {
        let webkit;
        let webkitPatch;
        let shared;
        let testGroup;
        let buildRequest;
        let otherBuildRequest;
        let uploadedRoot;
        return createTestGroupWihPatch().then((group) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            shared = Repository.findById(MockData.sharedRepositoryId());

            testGroup = group;
            buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 6);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending()); 
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(commitSet.patchForRepository(shared), null);
            assert.strictEqual(commitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(otherCommitSet.patchForRepository(shared), null);
            assert.strictEqual(otherCommitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile, buildRequest.id()));
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(uploadedRoot.filename(), 'some.dat');
            assert.strictEqual(uploadedRoot.author(), 'someUser');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {

            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending()); 
            assert.notStrictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), webkitPatch);
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert.strictEqual(webkitRoot, uploadedRoot);
            assert.strictEqual(commitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(commitSet.patchForRepository(shared), null);
            assert.strictEqual(commitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), [uploadedRoot]);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(otherCommitSet.patchForRepository(shared), null);
            assert.strictEqual(otherCommitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);
        });
    });

    it('should reject when the repository list is not an array', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '"a"';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepositoryList');
        });
    });

    it('should reject when the repository list refers to a non-existent repository', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "BadRepositoryName"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepository');
            assert.strictEqual(response['repositoryName'], 'BadRepositoryName');
        });
    });

    it('should reject when the repository list refers to a repository not present in the commit set', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "macOS"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'InvalidRepository');
            assert.strictEqual(response['repositoryName'], 'macOS');
        });
    });

    it('should update all commit set items in the repository listed', () => {
        let webkit;
        let webkitPatch;
        let shared;
        let testGroup;
        let buildRequest;
        let otherBuildRequest;
        let uploadedRoot;
        return createTestGroupWihPatch().then((group) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            shared = Repository.findById(MockData.sharedRepositoryId());

            testGroup = group;
            buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 6);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending()); 
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(commitSet.patchForRepository(shared), null);
            assert.strictEqual(commitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(otherCommitSet.patchForRepository(shared), null);
            assert.strictEqual(otherCommitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "Shared"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(uploadedRoot.filename(), 'some.dat');
            assert.strictEqual(uploadedRoot.author(), 'someUser');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {

            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending()); 
            assert.notStrictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(commitSet.patchForRepository(webkit), webkitPatch);
            assert.strictEqual(commitSet.rootForRepository(webkit), uploadedRoot);
            assert.strictEqual(commitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(commitSet.patchForRepository(shared), null);
            assert.strictEqual(commitSet.rootForRepository(shared), uploadedRoot);
            assert.deepStrictEqual(commitSet.allRootFiles(), [uploadedRoot]);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(shared), '80229');
            assert.strictEqual(otherCommitSet.patchForRepository(shared), null);
            assert.strictEqual(otherCommitSet.rootForRepository(shared), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);
        });
    });

    it('should only set build requests as complete when all roots are built', () => {
        let webkit;
        let webkitPatch;
        let ownedJSC;
        let testGroup;
        let buildRequest;
        let otherBuildRequest;
        let uploadedRoot;
        let jscRoot;
        return createTestGroupWithPatchAndOwnedCommits().then((group) => {
            webkit = Repository.findById(MockData.webkitRepositoryId());
            ownedJSC = Repository.findById(MockData.ownedJSCRepositoryId());

            testGroup = group;
            buildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(testGroup.buildRequests().length, 6);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending());
            assert.strictEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.strictEqual(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.strictEqual(webkitPatch.filename(), 'patch.dat');
            assert.strictEqual(commitSet.rootForRepository(webkit), null);
            assert.strictEqual(commitSet.revisionForRepository(ownedJSC), 'owned-jsc-6161');
            assert.strictEqual(commitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(commitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.strictEqual(otherCommitSet.revisionForRepository(webkit), '192736');
            assert.strictEqual(otherCommitSet.patchForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.rootForRepository(webkit), null);
            assert.strictEqual(otherCommitSet.revisionForRepository(ownedJSC), 'owned-jsc-9191');
            assert.strictEqual(otherCommitSet.patchForRepository(ownedJSC), null);
            assert.strictEqual(otherCommitSet.rootForRepository(ownedJSC), null);
            assert.deepStrictEqual(otherCommitSet.allRootFiles(), []);

            return addWorkerAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), ['WebKit']);
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert.strictEqual(buildRequest.buildId(), null);

            assert.deepStrictEqual(buildRequest.commitSet().allRootFiles(), [uploadedRoot]);
            assert.deepStrictEqual(otherBuildRequest.commitSet().allRootFiles(), []);
            return TemporaryFile.makeTemporaryFile('JavaScriptCore-Root.dat', 'JavaScript Content 0');
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id(), [{ownerRepository: 'WebKit', ownedRepository: 'JavaScriptCore'}], '2017-05-10T02:54:09.666');
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.strictEqual(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            jscRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.strictEqual(jscRoot.filename(), 'JavaScriptCore-Root.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {
            assert.strictEqual(testGroups.length, 1);
            const group = testGroups[0];
            assert.strictEqual(group, testGroup);
            assert.strictEqual(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.strictEqual(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending());
            assert.notStrictEqual(buildRequest.buildId(), null);

            assert.deepStrictEqual(buildRequest.commitSet().allRootFiles(), [uploadedRoot, jscRoot]);
            assert.deepStrictEqual(otherBuildRequest.commitSet().allRootFiles(), []);
        });
    });
});
