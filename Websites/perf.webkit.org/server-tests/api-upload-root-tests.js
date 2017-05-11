'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

function makeReport(rootFile, buildRequestId = 1)
{
    return {
        slaveName: 'someSlave',
        slavePassword: 'somePassword',
        builderName: 'someBuilder',
        buildNumber: 123,
        buildTime: '2017-05-10T02:54:08.666',
        buildRequest: buildRequestId,
        rootFile: rootFile,
        repositoryList: '["WebKit"]',
    };
}

function addSlaveAndCreateRootFile(slaveInfo = makeReport())
{
    return addSlaveForReport(slaveInfo).then(() => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content');
    });
}

function createTestGroupWihPatch()
{
    const triggerableConfiguration = {
        'slaveName': 'sync-slave',
        'slavePassword': 'password',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()},
        ],
        'repositoryGroups': [
            {name: 'webkit', repositories: [
                {repository: MockData.webkitRepositoryId(), acceptsPatch: true},
                {repository: MockData.sharedRepositoryId()},
            ]}
        ]
    };

    return MockData.addMockData(TestServer.database()).then(() => {
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
        return TestGroup.createWithTask('custom task', Platform.findById(MockData.somePlatformId()), someTest, 'some group', 2, [set1, set2]);
    }).then((task) => {
        return TestGroup.findAllByTask(task.id())[0];
    })
}

describe('/api/upload-root/', function () {
    prepareServerTest(this);
    TemporaryFile.inject();

    it('should reject when root file is missing', () => {
        return TestServer.remoteAPI().postFormData('/api/upload-root/', {}).then((response) => {
            assert.equal(response['status'], 'NoFileSpecified');
        });
    });

    it('should reject when there are no slaves', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile));
        }).then((response) => {
            assert.equal(response['status'], 'SlaveNotFound');
        });
    });

    it('should reject when slave name is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.slaveName;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'MissingSlaveName');
        });
    });

    it('should reject when builder name is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.builderName;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuilderName');
        });
    });

    it('should reject when build number is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildNumber;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildNumber');
        });
    });

    it('should reject when build number is not a number', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.buildNumber = '1abc';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildNumber');
        });
    });

    it('should reject when build time is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildTime;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildTime');
        });
    });

    it('should reject when build time is malformed', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.buildTime = 'Wed, 10 May 2017 03:02:30 GMT';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildTime');
        });
    });

    it('should reject when build request ID is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.buildRequest;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildRequest');
        });
    });

    it('should reject when build request ID is not a number', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.buildRequest = 'abc';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildRequest');
        });
    });

    it('should reject when build request does not exist', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile));
        }).then((response) => {
            assert.equal(response['status'], 'InvalidBuildRequestType');
        });
    });

    it('should reject when repository list is missing', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            delete report.repositoryList;
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryList');
        });
    });

    it('should reject when repository list is not a valid JSON string', () => {
        return addSlaveAndCreateRootFile().then((rootFile) => {
            const report = makeReport(rootFile);
            report.repositoryList = 'a+b';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryList');
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
            assert.equal(testGroup.buildRequests().length, 6);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending()); 
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.revisionForRepository(shared), '80229');
            assert.equal(commitSet.patchForRepository(shared), null);
            assert.equal(commitSet.rootForRepository(shared), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.revisionForRepository(shared), '80229');
            assert.equal(otherCommitSet.patchForRepository(shared), null);
            assert.equal(otherCommitSet.rootForRepository(shared), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            return TestServer.remoteAPI().postFormData('/api/upload-root/', makeReport(rootFile, buildRequest.id()));
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.equal(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {

            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group, testGroup);
            assert.equal(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.equal(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending()); 
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), webkitPatch);
            const webkitRoot = commitSet.rootForRepository(webkit);
            assert.equal(webkitRoot, uploadedRoot);
            assert.equal(commitSet.revisionForRepository(shared), '80229');
            assert.equal(commitSet.patchForRepository(shared), null);
            assert.equal(commitSet.rootForRepository(shared), null);
            assert.deepEqual(commitSet.allRootFiles(), [uploadedRoot]);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.revisionForRepository(shared), '80229');
            assert.equal(otherCommitSet.patchForRepository(shared), null);
            assert.equal(otherCommitSet.rootForRepository(shared), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);
        });
    });

    it('should reject when the repository list is not an array', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '"a"';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepositoryList');
        });
    });

    it('should reject when the repository list refers to a non-existent repository', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "BadRepositoryName"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepository');
            assert.equal(response['repositoryName'], 'BadRepositoryName');
        });
    });

    it('should reject when the repository list refers to a repository not present in the commit set', () => {
        let buildRequest;
        return createTestGroupWihPatch().then((group) => {
            buildRequest = group.buildRequests()[0];
            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "macOS"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidRepository');
            assert.equal(response['repositoryName'], 'macOS');
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
            assert.equal(testGroup.buildRequests().length, 6);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending()); 
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.revisionForRepository(shared), '80229');
            assert.equal(commitSet.patchForRepository(shared), null);
            assert.equal(commitSet.rootForRepository(shared), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.revisionForRepository(shared), '80229');
            assert.equal(otherCommitSet.patchForRepository(shared), null);
            assert.equal(otherCommitSet.rootForRepository(shared), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "Shared"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.equal(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {

            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group, testGroup);
            assert.equal(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.equal(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending()); 
            assert.notEqual(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            assert.equal(commitSet.patchForRepository(webkit), webkitPatch);
            assert.equal(commitSet.rootForRepository(webkit), uploadedRoot);
            assert.equal(commitSet.revisionForRepository(shared), '80229');
            assert.equal(commitSet.patchForRepository(shared), null);
            assert.equal(commitSet.rootForRepository(shared), uploadedRoot);
            assert.deepEqual(commitSet.allRootFiles(), [uploadedRoot]);

            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.revisionForRepository(shared), '80229');
            assert.equal(otherCommitSet.patchForRepository(shared), null);
            assert.equal(otherCommitSet.rootForRepository(shared), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);
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
            assert.equal(testGroup.buildRequests().length, 6);
            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(!buildRequest.hasFinished());
            assert(buildRequest.isPending()); 
            assert.equal(buildRequest.buildId(), null);

            const commitSet = buildRequest.commitSet();
            assert.equal(commitSet.revisionForRepository(webkit), '191622');
            webkitPatch = commitSet.patchForRepository(webkit);
            assert(webkitPatch instanceof UploadedFile);
            assert.equal(webkitPatch.filename(), 'patch.dat');
            assert.equal(commitSet.rootForRepository(webkit), null);
            assert.equal(commitSet.revisionForRepository(shared), '80229');
            assert.equal(commitSet.patchForRepository(shared), null);
            assert.equal(commitSet.rootForRepository(shared), null);
            assert.deepEqual(commitSet.allRootFiles(), []);

            otherBuildRequest = testGroup.buildRequests()[1];
            const otherCommitSet = otherBuildRequest.commitSet();
            assert.equal(otherCommitSet.revisionForRepository(webkit), '191622');
            assert.equal(otherCommitSet.patchForRepository(webkit), null);
            assert.equal(otherCommitSet.rootForRepository(webkit), null);
            assert.equal(otherCommitSet.revisionForRepository(shared), '80229');
            assert.equal(otherCommitSet.patchForRepository(shared), null);
            assert.equal(otherCommitSet.rootForRepository(shared), null);
            assert.deepEqual(otherCommitSet.allRootFiles(), []);

            return addSlaveAndCreateRootFile();
        }).then((rootFile) => {
            const report = makeReport(rootFile, buildRequest.id());
            report.repositoryList = '["WebKit", "Shared"]';
            return TestServer.remoteAPI().postFormData('/api/upload-root/', report);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            const uploadedRootRawData = response['uploadedFile'];
            uploadedRoot = UploadedFile.ensureSingleton(uploadedRootRawData.id, uploadedRootRawData);
            assert.equal(uploadedRoot.filename(), 'some.dat');
            return TestGroup.fetchForTask(buildRequest.testGroup().task().id(), true);
        }).then((testGroups) => {
            assert.equal(testGroups.length, 1);
            const group = testGroups[0];
            assert.equal(group, testGroup);
            assert.equal(testGroup.buildRequests().length, 6);

            const updatedBuildRequest = testGroup.buildRequests()[0];
            assert.equal(updatedBuildRequest, buildRequest);

            assert(buildRequest.isBuild());
            assert(!buildRequest.isTest());
            assert(buildRequest.hasFinished());
            assert(!buildRequest.isPending()); 
            assert.notEqual(buildRequest.buildId(), null);

            assert.deepEqual(buildRequest.commitSet().allRootFiles(), [uploadedRoot]);
            assert.deepEqual(otherBuildRequest.commitSet().allRootFiles(), []);
        });
    });

});
