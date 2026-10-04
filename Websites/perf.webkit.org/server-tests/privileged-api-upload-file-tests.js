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
            assert.strictEqual(error, 'NoFileSpecified');
        });
    });

    it('should return "FileSizeLimitExceeded" when the file is too big', () => {
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', TestServer.testConfig().uploadFileLimitInMB + 1).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true}).then(() => {
                assert(false, 'should never be reached');
            }, (error) => {
                assert.strictEqual(error, 'FileSizeLimitExceeded');
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
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0].id, parseInt(uploadedFile.id));
            assert.strictEqual(parseInt(rows[0].size), limitInMB * 1024 * 1024);
            assert.strictEqual(parseInt(rows[0].size), parseInt(uploadedFile.size));
            assert.strictEqual(rows[0].filename, 'some.dat');
            assert.strictEqual(rows[0].filename, uploadedFile.filename);
            assert.strictEqual(rows[0].extension, '.dat');
            assert.strictEqual(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.strictEqual(rows[0].sha256, uploadedFile.sha256);
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
            assert.deepStrictEqual(uploadedFile1, uploadedFile2);
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0].id, parseInt(uploadedFile1.id));
            assert.strictEqual(parseInt(rows[0].size), limitInMB * 1024 * 1024);
            assert.strictEqual(parseInt(rows[0].size), parseInt(uploadedFile1.size));
            assert.strictEqual(rows[0].filename, 'some.dat');
            assert.strictEqual(rows[0].filename, uploadedFile1.filename);
            assert.strictEqual(rows[0].extension, '.dat');
            assert.strictEqual(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.strictEqual(rows[0].sha256, uploadedFile1.sha256);
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
            assert.deepStrictEqual(uploadedFile1, uploadedFile2);
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0].id, parseInt(uploadedFile1.id));
            assert.strictEqual(parseInt(rows[0].size), limitInMB * 1024 * 1024);
            assert.strictEqual(parseInt(rows[0].size), parseInt(uploadedFile1.size));
            assert.strictEqual(rows[0].filename, 'some.dat');
            assert.strictEqual(rows[0].filename, uploadedFile1.filename);
            assert.strictEqual(rows[0].extension, '.dat');
            assert.strictEqual(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.strictEqual(rows[0].sha256, uploadedFile1.sha256);
        });
    });

    it('should re-upload the file when the previously uploaded file had been deleted and reuse the file row with updated creation time and cleared deletion time', () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let firstUploadCreationTime;
        let secondUploadCreationTime;
        let uploadedFile1;
        let uploadedFile2;
        return TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile1 = response['uploadedFile'];
            firstUploadCreationTime = uploadedFile1.createdAt;
            return db.query(`UPDATE uploaded_files SET file_deleted_at = now() at time zone 'utc'`);
        }).then(() => {
            return TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB);
        }).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile2 = response['uploadedFile'];
            secondUploadCreationTime = uploadedFile2.createdAt;
            return db.selectAll('uploaded_files', 'id');
        }).then((rows) => {
            assert.strictEqual(uploadedFile1.id, uploadedFile2.id);
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(rows[0].id, parseInt(uploadedFile1.id));

            assert.strictEqual(rows[0].filename, 'some.dat');
            assert.strictEqual(rows[0].filename, uploadedFile1.filename);

            assert.notEqual(firstUploadCreationTime, secondUploadCreationTime);

            assert.strictEqual(parseInt(rows[0].size), limitInMB * 1024 * 1024);
            assert.strictEqual(parseInt(rows[0].size), parseInt(uploadedFile1.size));
            assert.strictEqual(parseInt(rows[0].size), parseInt(uploadedFile2.size));
            assert.strictEqual(rows[0].deleted_at, null);
            assert.strictEqual(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
            assert.strictEqual(rows[0].sha256, uploadedFile1.sha256);
            assert.strictEqual(rows[0].sha256, uploadedFile2.sha256);
            assert.strictEqual(rows[0].extension, '.dat');
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
            assert.strictEqual(rows.length, 1);
            assert.strictEqual(parseInt(rows[0].size), limitInMB * 1024 * 1024);
            assert.strictEqual(rows[0].mime, 'application/octet-stream');
            assert.strictEqual(rows[0].filename, 'some.other.tar.gz5');
            assert.strictEqual(rows[0].extension, '.tar.gz5');
            assert.strictEqual(rows[0].sha256, '5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5');
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
            assert.strictEqual(rows.length, 3);
            assert.strictEqual(rows[0].filename, 'some.dat');
            assert.notStrictEqual(rows[0].deleted_at, null);
            assert.strictEqual(rows[1].filename, 'other.dat');
            assert.strictEqual(rows[1].deleted_at, null);
            assert.strictEqual(rows[2].filename, 'another.dat');
            assert.strictEqual(rows[2].deleted_at, null);
        })
    });

    it('should delete an old file that is out of grace period since the creation time when uploading the file would result in the quota being exceeded', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        TestServer.overwriteTestConfig({uploadFileGracePeriodInHours: 1});
        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'b');
        const response = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const otherFile = response.uploadedFile;
        await db.query(`UPDATE uploaded_files SET file_created_at = to_timestamp(${Math.floor(Date.now() / 1000) - 3600 * 2}) WHERE file_id = ${otherFile.id}`);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 3);
        assert.strictEqual(rows[0].filename, 'some.dat');
        assert.strictEqual(rows[0].deleted_at, null);
        assert.strictEqual(rows[1].filename, 'other.dat');
        assert.notStrictEqual(rows[1].deleted_at, null);
        assert.strictEqual(rows[2].filename, 'another.dat');
        assert.strictEqual(rows[2].deleted_at, null);
    });

    it('should prune to 80% of quota when uploading would exceed quota', async () => {
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
        assert.strictEqual(rows.length, splitCount);

        const stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('limit.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        rows = await db.selectAll('uploaded_files');
        assert.strictEqual(rows.length, splitCount + 1);
        const deletedFiles = rows.filter((row) => row['deleted_at']);
        const activeFiles = rows.filter((row) => !row['deleted_at']);

        const totalActiveSize = activeFiles.reduce((sum, file) => sum + parseInt(file.size), 0);
        const targetSize = userQuotaInMB * 0.8 * 1024 * 1024 + limitInMB * 1024 * 1024; // 80% + new file
        assert.ok(totalActiveSize <= targetSize,
            `Total size ${totalActiveSize} should be at most 80% of quota plus new file (${targetSize})`);
        assert.ok(deletedFiles.length > 0,
            `Expected some deleted files, got ${deletedFiles.length}`);
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
                assert.strictEqual(error, 'FileSizeQuotaExceeded');
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
            assert.strictEqual(rows.length, 3);
            assert.strictEqual(rows[0].filename, 'some.patch');
            assert.notStrictEqual(rows[0].deleted_at, null);
            assert.strictEqual(rows[1].filename, 'other.patch');
            assert.strictEqual(rows[1].deleted_at, null);
            assert.strictEqual(rows[2].filename, 'another.dat');
            assert.strictEqual(rows[2].deleted_at, null);
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
            assert.strictEqual(rows.length, 3);
            assert.strictEqual(rows[0].filename, 'some.patch');
            assert.strictEqual(rows[0].deleted_at, null);
            assert.strictEqual(rows[1].filename, 'root.tar.gz');
            assert.notStrictEqual(rows[1].deleted_at, null);
            assert.strictEqual(rows[2].filename, 'another.dat');
            assert.strictEqual(rows[2].deleted_at, null);
        });
    });

    it('should prune files from other users when the total quota is exceeded', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        await MockData.addMockData(db);

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('some.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('other.dat', limitInMB, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        await db.query('UPDATE uploaded_files SET file_author = $1', ['someUser']);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('another.dat', limitInMB, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        await db.query('UPDATE uploaded_files SET file_author = $1 WHERE file_author IS NULL', ['anotherUser']);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('newest.dat', limitInMB, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 4);
        assert.strictEqual(rows[0].filename, 'some.dat');
        assert.notStrictEqual(rows[0].deleted_at, null, 'Oldest file should be deleted');
        assert.strictEqual(rows[1].filename, 'other.dat');
        assert.notStrictEqual(rows[1].deleted_at, null, 'Second oldest file should also be deleted for 80% pruning');
        assert.strictEqual(rows[2].filename, 'another.dat');
        assert.strictEqual(rows[2].deleted_at, null);
        assert.strictEqual(rows[3].filename, 'newest.dat');
        assert.strictEqual(rows[3].deleted_at, null);
    });

    it('should not double counting a file if it is referenced by multiple commit_sets', async () => {
        const db = TestServer.database();
        TestServer.overwriteTestConfig({uploadFileLimitInMB: 3, uploadUserQuotaInMB: 5});
        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('root-1-2MB', 2, 'a');
        const firstResponse = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('root-2-2MB', 2, 'b');
        const secondResponse = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        await Promise.all([
            db.insert('repositories', {id: 1, name: 'WebKit'}),
            db.insert('commits', {id: 93116, repository: 1, revision: '191622', time: (new Date(1445945816878)).toISOString()}),
            db.insert('commit_sets', {id: 500}),
            db.insert('commit_set_items', {set: 500, commit: 93116, requires_build: true, root_file: firstResponse.uploadedFile.id}),
            db.insert('commit_sets', {id: 501}),
            db.insert('commit_set_items', {set: 501, commit: 93116, requires_build: true, root_file: secondResponse.uploadedFile.id}),
        ]);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('root-3-3MB', 3, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 3);
        assert.strictEqual(rows[0].filename, 'root-1-2MB');
        assert.notStrictEqual(rows[0].deleted_at, null);
        assert.strictEqual(rows[1].filename, 'root-2-2MB');
        assert.notStrictEqual(rows[1].deleted_at, null);
        assert.strictEqual(rows[2].filename, 'root-3-3MB');
        assert.strictEqual(rows[2].deleted_at, null);
    });

    it('should handle user slightly over personal quota from concurrent uploads', async () => {
        const db = TestServer.database();
        const userQuotaInMB = TestServer.testConfig().uploadUserQuotaInMB;
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-1.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-2.dat', limitInMB, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-3.dat', 1, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-4.dat', 1, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 4);

        const deletedFiles = rows.filter((row) => row['deleted_at']);
        assert.ok(deletedFiles.length >= 1, `Should have deleted at least 1 file, got ${deletedFiles.length}`);
        const activeFiles = rows.filter((row) => !row['deleted_at']);
        const totalSize = activeFiles.reduce((sum, file) => sum + parseInt(file.size), 0);
        assert.ok(totalSize <= userQuotaInMB * 1024 * 1024, `Total size ${totalSize} should be within quota`);
    });

    it('should prune to 80% threshold providing sufficient buffer space', async () => {
        const db = TestServer.database();
        const userQuotaInMB = TestServer.testConfig().uploadUserQuotaInMB;
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-1.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-2.dat', limitInMB, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('file-3.dat', limitInMB, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 3);
        const deletedFiles = rows.filter((row) => row['deleted_at']);
        assert.ok(deletedFiles.length >= 1,
            `Expected at least 1 deleted file, got ${deletedFiles.length}`);
        const activeFiles = rows.filter((row) => !row['deleted_at']);
        const totalSize = activeFiles.reduce((sum, file) => sum + parseInt(file.size), 0);
        const quotaBytes = userQuotaInMB * 1024 * 1024;
        const expectedMaxSize = quotaBytes * 0.8 + limitInMB * 1024 * 1024; // 80% + new file
        assert.ok(totalSize <= expectedMaxSize,
            `Total size ${totalSize} should be within 80% threshold plus new file`);
    });

    it('should prune globally from oldest files when total quota exceeded', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user1-old.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_author = $1, file_created_at = file_created_at - interval \'2 days\' WHERE file_filename = $2',
            ['user1', 'user1-old.dat']);
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user2-old.dat', limitInMB, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_author = $1, file_created_at = file_created_at - interval \'1 day\' WHERE file_filename = $2',
            ['user2', 'user2-old.dat']);
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user1-new.dat', limitInMB, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_author = $1 WHERE file_filename = $2', ['user1', 'user1-new.dat']);
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user3-file.dat', limitInMB, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        const rows = await db.selectAll('uploaded_files', 'id');
        assert.strictEqual(rows.length, 4);
        const user1OldFile = rows.find(r => r.filename === 'user1-old.dat');
        assert.notStrictEqual(user1OldFile.deleted_at, null, 'Oldest file should be deleted');
        const user2OldFile = rows.find(r => r.filename === 'user2-old.dat');
        assert.notStrictEqual(user2OldFile.deleted_at, null, 'Second oldest file should also be deleted');
        const user1NewFile = rows.find(r => r.filename === 'user1-new.dat');
        const user3File = rows.find(r => r.filename === 'user3-file.dat');
        assert.strictEqual(user1NewFile.deleted_at, null, 'user1-new.dat should not be deleted');
        assert.strictEqual(user3File.deleted_at, null, 'user3-file.dat should not be deleted');
    });

    it('should prioritize build products for deletion in global pruning', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        const totalQuotaInMB = TestServer.testConfig().uploadTotalQuotaInMB;
        await MockData.addMockData(db, ['completed', 'completed', 'completed', 'completed']);

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('older-regular.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_created_at = file_created_at - interval \'3 days\' WHERE file_filename = $1',
            ['older-regular.dat']);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('build-product.tar.gz', limitInMB, 'b');
        const buildResponse = await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_created_at = file_created_at - interval \'2 days\' WHERE file_id = $1',
            [buildResponse.uploadedFile.id]);
        await db.query('UPDATE commit_set_items SET commitset_root_file = $1, commitset_requires_build = TRUE WHERE commitset_set = 401',
            [buildResponse.uploadedFile.id]);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('another-regular.dat', limitInMB, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('trigger.dat', limitInMB, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        const buildProduct = rows.find(r => r.filename === 'build-product.tar.gz');
        const olderRegular = rows.find(r => r.filename === 'older-regular.dat');
        const anotherRegular = rows.find(r => r.filename === 'another-regular.dat');
        const trigger = rows.find(r => r.filename === 'trigger.dat');

        assert.notStrictEqual(buildProduct.deleted_at, null, 'Build product should be deleted (prioritized)');
        assert.notStrictEqual(olderRegular.deleted_at, null, 'Older regular file also deleted to meet 80% quota');
        assert.strictEqual(anotherRegular.deleted_at, null, 'Newer regular file should not be deleted');
        assert.strictEqual(trigger.deleted_at, null, 'Trigger file should not be deleted');
    });

    it('should handle both user and global quota pruning in single upload', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;
        const userQuotaInMB = TestServer.testConfig().uploadUserQuotaInMB;
        const totalQuotaInMB = TestServer.testConfig().uploadTotalQuotaInMB;

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user-file1.dat', 2, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user-file2.dat', 2, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('user-file3.dat', 0.5, 'c');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        await db.query('UPDATE uploaded_files SET file_author = $1 WHERE file_filename IN ($2, $3)',
            ['otherUser', 'user-file1.dat', 'user-file2.dat']);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('other-user1.dat', 2, 'd');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        await db.query('UPDATE uploaded_files SET file_author = $1, file_created_at = file_created_at - interval \'3 days\' WHERE file_filename = $2',
            ['anotherUser', 'other-user1.dat']);

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('trigger-both.dat', 1, 'e');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        const userFiles = rows.filter(r => r.author === null);
        const otherUserFiles = rows.filter(r => r.author === 'otherUser');
        const anotherUserFiles = rows.filter(r => r.author === 'anotherUser');

        const deletedFiles = rows.filter(r => r.deleted_at !== null);
        assert.ok(deletedFiles.length > 0, 'Some files should be deleted for quota compliance');

        const userActiveFiles = userFiles.filter(r => r.deleted_at === null);
        const userTotalSize = userActiveFiles.reduce((sum, file) => sum + parseInt(file.size), 0);
        assert.ok(userTotalSize <= userQuotaInMB * 1024 * 1024,
            `User total size ${userTotalSize} should be within user quota`);
    });

    it('should return FileSizeQuotaExceeded when no files can be deleted due to grace period', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        TestServer.overwriteTestConfig({uploadFileGracePeriodInHours: 24});

        let stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('protected1.dat', limitInMB, 'a');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('protected2.dat', limitInMB, 'b');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('exceeds.dat', limitInMB, 'c');
        try {
            await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
            assert(false, 'should never be reached');
        } catch (error) {
            assert.strictEqual(error, 'FileSizeQuotaExceeded');
        }
    });

    it('should correctly handle file deletions with proper transaction isolation', async () => {
        const db = TestServer.database();
        const limitInMB = TestServer.testConfig().uploadFileLimitInMB;

        let filesToCreate = 10;
        for (let i = 0; i < filesToCreate; i++) {
            const stream = await TemporaryFile.makeTemporaryFileOfSizeInMB(`batch-${i}.dat`, 0.5, String.fromCharCode(97 + i));
            await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }

        await db.query('UPDATE uploaded_files SET file_created_at = file_created_at - interval \'7 days\'');

        const stream = await TemporaryFile.makeTemporaryFileOfSizeInMB('trigger-batch.dat', limitInMB, 'z');
        await PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});

        const rows = await db.selectAll('uploaded_files', 'id');
        const deletedFiles = rows.filter(r => r.deleted_at !== null);
        const activeFiles = rows.filter(r => r.deleted_at === null);

        assert.ok(deletedFiles.length > 0, 'Batch deletion should have deleted multiple files');
        assert.ok(activeFiles.some(r => r.filename === 'trigger-batch.dat'),
            'New file should be successfully uploaded after batch deletion');
    });
});
