'use strict';

require('../tools/js/v3-models.js');

const assert = require('assert');
global.FormData = require('form-data');

const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;

describe('/privileged-api/upload-file', function () {
    this.timeout(5000);
    TestServer.inject();

    TemporaryFile.inject();

    it('should return "NotFileSpecified" when newFile not is specified', () => {
        return PrivilegedAPI.sendRequest('upload-file', {}, {useFormData: true}).then(() => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 'NoFileSpecified');
        });
    });

    it('should return "FileSizeLimitExceeded" when the file is too big', () => {
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', TestServer.testConfig().uploadFileLimitInMB + 1).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true}).then(() => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'FileSizeLimitExceeded');
            });
        });
    });

    it('should upload a file when the filesize is smaller than the limit', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let uploadedFile;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            return db.connect().then(() => db.selectAll('uploaded_files', 'id'));
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0].id, uploadedFile.id);
            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].size, uploadedFile.size);
            assert.equal(rows[0].filename, 'some.dat');
            assert.equal(rows[0].filename, uploadedFile.filename);
            assert.equal(rows[0].extension, '.dat');
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.equal(rows[0].sha256, uploadedFile.sha256);
        });
    });

    it('should not create a duplicate files when the identical files are uploaded', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let uploadedFile1;
        let uploadedFile2;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile1 = response['uploadedFile'];
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB);
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile2 = response['uploadedFile'];
            return db.connect().then(() => db.selectAll('uploaded_files', 'id'));
        }).then((rows) => {
            assert.deepEqual(uploadedFile1, uploadedFile2);
            assert.equal(rows.length, 1);
            assert.equal(rows[0].id, uploadedFile1.id);
            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].size, uploadedFile1.size);
            assert.equal(rows[0].filename, 'some.dat');
            assert.equal(rows[0].filename, uploadedFile1.filename);
            assert.equal(rows[0].extension, '.dat');
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.equal(rows[0].sha256, uploadedFile1.sha256);
        });
    });

    it('should not create a duplicate files when the identical files are uploaded', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let uploadedFile1;
        let uploadedFile2;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile1 = response['uploadedFile'];
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB);
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile2 = response['uploadedFile'];
            return db.connect().then(() => db.selectAll('uploaded_files', 'id'));
        }).then((rows) => {
            assert.deepEqual(uploadedFile1, uploadedFile2);
            assert.equal(rows.length, 1);
            assert.equal(rows[0].id, uploadedFile1.id);
            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].size, uploadedFile1.size);
            assert.equal(rows[0].filename, 'some.dat');
            assert.equal(rows[0].filename, uploadedFile1.filename);
            assert.equal(rows[0].extension, '.dat');
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.equal(rows[0].sha256, uploadedFile1.sha256);
        });
    });

    it('should re-upload the file when the previously uploaded file had been deleted', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let uploadedFile1;
        let uploadedFile2;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile1 = response['uploadedFile'];
            return db.connect().then(() => db.query(`UPDATE uploaded_files SET file_deleted_at = now() at time zone 'utc'`));
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB);
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile2 = response['uploadedFile'];
            return db.connect().then(() => db.selectAll('uploaded_files', 'id'));
        }).then((rows) => {
            assert.notEqual(uploadedFile1.id, uploadedFile2.id);
            assert.equal(rows.length, 2);
            assert.equal(rows[0].id, uploadedFile1.id);
            assert.equal(rows[1].id, uploadedFile2.id);

            assert.equal(rows[0].filename, 'some.dat');
            assert.equal(rows[0].filename, uploadedFile1.filename);
            assert.equal(rows[1].filename, 'other.dat');
            assert.equal(rows[1].filename, uploadedFile2.filename);

            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].size, uploadedFile1.size);
            assert.equal(rows[0].size, uploadedFile2.size);
            assert.equal(rows[0].size, rows[1].size);
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.equal(rows[0].sha256, uploadedFile1.sha256);
            assert.equal(rows[0].sha256, uploadedFile2.sha256);
            assert.equal(rows[0].sha256, rows[1].sha256);
            assert.equal(rows[0].extension, '.dat');
            assert.equal(rows[1].extension, '.dat');
        });
    });

    it('should pick up at most two file extensions', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.other.tar.gz', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.connect().then(() => db.selectAll('uploaded_files', 'id'))
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].mime, 'application/octet-stream');
            assert.equal(rows[0].filename, 'some.other.tar.gz');
            assert.equal(rows[0].extension, '.tar.gz');
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
        });
    });

    it('should return "FileSizeQuotaExceeded" when the total file size exceeds the quota allowed per user', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB, 'a').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'c');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true}).then(() => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.equal(error, 'FileSizeQuotaExceeded');
            });
        });
    });
});
