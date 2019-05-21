'use strict';

require('../tools/js/v3-models.js');

const assert = require('assert');
global.FormData = require('form-data');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/privileged-api/upload-file', function () {
    prepareServerTest(this);
    TemporaryFile.inject();
    function makeRandomAlnumStringForLength(length)
    {
        let string = '';
        const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        const charactersLength = characters.length;
        for (let i = 0; i < length; i++)
            string += characters.charAt(Math.floor(Math.random() * charactersLength));
        return string;
    }

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
            return db.selectAll('uploaded_files', 'id');
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
            return db.selectAll('uploaded_files', 'id');
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
            return db.selectAll('uploaded_files', 'id');
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
            return db.query(`UPDATE uploaded_files SET file_deleted_at = now() at time zone 'utc'`);
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB);
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile2 = response['uploadedFile'];
            return db.selectAll('uploaded_files', 'id');
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
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.other.tar.gz5', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.selectAll('uploaded_files', 'id');
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0].size, limitInMB * 1024 * 1024);
            assert.equal(rows[0].mime, 'application/octet-stream');
            assert.equal(rows[0].filename, 'some.other.tar.gz5');
            assert.equal(rows[0].extension, '.tar.gz5');
            assert.equal(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
        });
    });

    it('should delete an old file when uploading the file would result in the quota being exceeded', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB, 'a').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.selectAll('uploaded_files', 'id');
        }).then((rows) => {
            assert.equal(rows.length, 3);
            assert.equal(rows[0].filename, 'some.dat');
            assert.notEqual(rows[0].deleted_at, null);
            assert.equal(rows[1].filename, 'other.dat');
            assert.equal(rows[1].deleted_at, null);
            assert.equal(rows[2].filename, 'another.dat');
            assert.equal(rows[2].deleted_at, null);
        })
    });

    it('should prune all removable files to get space for a file upload', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        const userQuotaInMB = TestServer.testConfig().uploadUserQuotaInMB;
        const splitCount = 40;
        const contentLength = userQuotaInMB * 1024 * 1024 / splitCount;
        for (let i = 0; i < splitCount; i += 1) {
            const content = makeRandomAlnumStringForLength(contentLength);
            const stream = await TemporaryFile.makeTemporaryFile('file-' + i, content);
            await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }
        let rows = await db.selectAll('uploaded_files');
        assert.equal(rows.length, splitCount);

        const stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        rows = await db.selectAll('uploaded_files');
        assert.equal(rows.length, splitCount + 1);
        const deletedFiles = rows.filter((row) => row['deleted_at']);
        assert.equal(deletedFiles.length, 16);
    });

    it('should not prune files that have been deleted', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'a');
        let response = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const fileA = response.uploadedFile;
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'b');
        response = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const fileB = response.uploadedFile;

        await MockData.addMockData(db, ['completed', 'completed', 'completed', 'completed']);
        await db.query('UPDATE commit_set_items SET commitset_root_file=$1 WHERE commitset_set=401 AND commitset_commit=87832', [fileA.id]);
        await db.query('UPDATE commit_set_items SET commitset_root_file=$1 WHERE commitset_set=401 AND commitset_commit=93116', [fileB.id]);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'c');
        response = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const fileC = response.uploadedFile;
        await db.query('UPDATE commit_set_items SET commitset_root_file=$1 WHERE commitset_set=402 AND commitset_commit=87832', [fileC.id]);

        const deletedFileA = await db.selectFirstRow('uploaded_files', {id: fileA.id});
        assert.ok(deletedFileA.deleted_at);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const deletedFileB = await db.selectFirstRow('uploaded_files', {id: fileB.id});
        assert.ok(deletedFileB.deleted_at);
    });

    it('should return "FileSizeQuotaExceeded" when there is no file to delete', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let fileA;
        return MockData.addMockData(db).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('some.patch', limitInMB, 'a');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            fileA = result.uploadedFile;
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.patch', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            const fileB = result.uploadedFile;
            return Promise.all([
                db.query('UPDATE commit_set_items SET (commitset_patch_file, commitset_requires_build) = ($1, TRUE) WHERE commitset_set = 402 AND commitset_commit = 87832', [fileA.id]),
                db.query('UPDATE commit_set_items SET (commitset_patch_file, commitset_requires_build) = ($1, TRUE) WHERE commitset_set = 402 AND commitset_commit = 96336', [fileB.id])
            ]);
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

    it('should delete old patches that belong to finished build requests', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let fileA;
        return MockData.addMockData(db, ['completed', 'completed', 'failed', 'canceled']).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('some.patch', limitInMB, 'a');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            fileA = result.uploadedFile;
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.patch', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            const fileB = result.uploadedFile;
            return Promise.all([
                db.query('UPDATE commit_set_items SET (commitset_patch_file, commitset_requires_build) = ($1, TRUE) WHERE commitset_set = 402 AND commitset_commit = 87832', [fileA.id]),
                db.query('UPDATE commit_set_items SET (commitset_patch_file, commitset_requires_build) = ($1, TRUE) WHERE commitset_set = 402 AND commitset_commit = 96336', [fileB.id])
            ]);
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.selectAll('uploaded_files', 'id');
        }).then((rows) => {
            assert.equal(rows.length, 3);
            assert.equal(rows[0].filename, 'some.patch');
            assert.notEqual(rows[0].deleted_at, null);
            assert.equal(rows[1].filename, 'other.patch');
            assert.equal(rows[1].deleted_at, null);
            assert.equal(rows[2].filename, 'another.dat');
            assert.equal(rows[2].deleted_at, null);
        });
    });

    it('should delete old build products that belong to finished build requests before deleting patches', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let fileA;
        return MockData.addMockData(db, ['completed', 'completed', 'failed', 'canceled']).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('some.patch', limitInMB, 'a');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            fileA = result.uploadedFile;
            return TemporaryFile.makeTemporaryFileOfSizeInMB('root.tar.gz', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((result) => {
            const fileB = result.uploadedFile;
            return db.query(`UPDATE commit_set_items SET (commitset_patch_file, commitset_root_file, commitset_requires_build) = ($1, $2, TRUE)
                WHERE commitset_set = 402 AND commitset_commit = 87832`, [fileA.id, fileB.id]);
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.selectAll('uploaded_files', 'id');
        }).then((rows) => {
            assert.equal(rows.length, 3);
            assert.equal(rows[0].filename, 'some.patch');
            assert.equal(rows[0].deleted_at, null);
            assert.equal(rows[1].filename, 'root.tar.gz');
            assert.notEqual(rows[1].deleted_at, null);
            assert.equal(rows[2].filename, 'another.dat');
            assert.equal(rows[2].deleted_at, null);
        });
    });

    it('should return "FileSizeQuotaExceeded" when the total quota is exceeded due to files uploaded by other users', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let fileA;
        return MockData.addMockData(db).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB, 'a');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'b');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.query('UPDATE uploaded_files SET file_author = $1', ['someUser']);
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then(() => {
            return db.query('UPDATE uploaded_files SET file_author = $1 WHERE file_author IS NULL', ['anotherUser']);
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
