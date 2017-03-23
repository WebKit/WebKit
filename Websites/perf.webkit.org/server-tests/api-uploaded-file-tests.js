'use strict';

require('../tools/js/v3-models.js');

const assert = require('assert');
global.FormData = require('form-data');

const TestServer = require('./resources/test-server.js');
const TemporaryFile = require('./resources/temporary-file.js').TemporaryFile;

describe('/api/uploaded-file', function () {
    this.timeout(5000);
    TestServer.inject();

    TemporaryFile.inject();

    it('should return "InvalidArguments" when neither path nor sha256 query is set', () => {
        return TestServer.remoteAPI().getJSON('/api/uploaded-file').then((content) => {
            assert.equal(content['status'], 'InvalidArguments');
            return TestServer.remoteAPI().getJSON('/api/uploaded-file/');
        }).then((content) => {
            assert.equal(content['status'], 'InvalidArguments');
        });
    });

    it('should return 404 when there is no file with the specified ID', () => {
        return TestServer.remoteAPI().getJSON('/api/uploaded-file/1').then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 404);
        });
    });

    it('should return 404 when the specified ID is not a valid integer', () => {
        return TestServer.remoteAPI().getJSON('/api/uploaded-file/foo').then((content) => {
            assert(false, 'should never be reached');
        }, (error) => {
            assert.equal(error, 404);
        });
    });

    it('should return the file content matching the specified file ID', () => {
        let uploadedFile;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${uploadedFile['id']}`, 'GET', null, null);
        }).then((response) => {
            assert.equal(response.responseText, 'some content');
        });
    });

    it('should return the file content with createdAt using POSIX timestamp in UTC', () => {
        let uploadedFile;
        const startTime = +Date.now();
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            console.assert(typeof(uploadedFile.createdAt) == 'number')
            const createdAt = +new Date(uploadedFile.createdAt);
            const endTime = +Date.now();
            assert(startTime <= createdAt, 'createdAt should be after the time POST request was made');
            assert(createdAt <= endTime, 'createdAt should be before the uploadedFile response had finished');
        });
    });

    it('should return "NotFound" when the specified SHA256 is invalid', () => {
        return TestServer.remoteAPI().getJSON('/api/uploaded-file/?sha256=abc').then((content) => {
            assert.equal(content['status'], 'NotFound');
        });
    });

    it('should return "NotFound" when there is no file matching the specified SHA256 ', () => {
        return TestServer.remoteAPI().getJSON('/api/uploaded-file/?sha256=5256ec18f11624025905d057d6befb03d77b243511ac5f77ed5e0221ce6d84b5').then((content) => {
            assert.equal(content['status'], 'NotFound');
        });
    });

    it('should return the meta data of the file with the specified SHA256', () => {
        let uploadedFile;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            return TestServer.remoteAPI().getJSON(`/api/uploaded-file/?sha256=${uploadedFile['sha256']}`);
        }).then((response) => {
            assert.deepEqual(uploadedFile, response['uploadedFile']);
        });
    });

    it('should return "NotFound" when the file matching the specified SHA256 had already been deleted', () => {
        let uploadedFile;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            const db = TestServer.database();
            return db.connect().then(() => db.query(`UPDATE uploaded_files SET file_deleted_at = now() at time zone 'utc'`));
        }).then(() => {
            return TestServer.remoteAPI().getJSON(`/api/uploaded-file/?sha256=${uploadedFile['sha256']}`);
        }).then((content) => {
            assert.equal(content['status'], 'NotFound');
        });
    });


    it('should respond with ETag, Acccept-Ranges, Content-Disposition, Content-Length, and Last-Modified headers', () => {
        let uploadedFile;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            uploadedFile = response['uploadedFile'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${uploadedFile['id']}`, 'GET', null, null);
        }).then((response) => {
            const headers = response.headers;

            assert(Object.keys(headers).includes('etag'));
            assert.equal(headers['etag'], uploadedFile['sha256']);

            assert(Object.keys(headers).includes('accept-ranges'));
            assert.equal(headers['accept-ranges'], 'bytes');

            assert(Object.keys(headers).includes('content-disposition'));
            assert.equal(headers['content-disposition'], `attachment; filename*=utf-8''some.dat`);

            assert(Object.keys(headers).includes('content-length'));
            assert.equal(headers['content-length'], uploadedFile['size']);

            assert(Object.keys(headers).includes('last-modified'));
        });
    });

    it('should respond with the same Last-Modified each time', () => {
        let id;
        let lastModified;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null);
        }).then((response) => {
            lastModified = response.headers['last-modified'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null);
        }).then((response) => {
            assert.equal(response.headers['last-modified'], lastModified);
        });
    });

    it('should respond with Content-Range when requested after X bytes', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=5-'}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 5-11/12');
            assert.equal(response.responseText, 'content');
        });
    });

    it('should respond with Content-Range when requested between X-Y bytes', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=4-9'}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 4-9/12');
            assert.equal(response.responseText, ' conte');
        });
    });

    it('should respond with Content-Range when requested for the last X bytes', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=-4'}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 8-11/12');
            assert.equal(response.responseText, 'tent');
        });
    });

    it('should respond with Content-Range for the whole content when the suffix length is larger than the content', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=-100'}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 0-11/12');
            assert.equal(response.responseText, 'some content');
        });
    });

    it('should return 416 when the starting byte is after the file size', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=12-'}})
                .then(() => assert(false, 'should never be reached'), (error) => assert.equal(error, 416));
        });
    });

    it('should return 416 when the starting byte after the ending byte', () => {
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            const id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null, {headers: {'Range': 'bytes=2-1'}})
                .then(() => assert(false, 'should never be reached'), (error) => assert.equal(error, 416));
        });
    });

    it('should respond with Content-Range when If-Range matches the last modified date', () => {
        let id;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null);
        }).then((response) => {
            assert.equal(response.statusCode, 200);
            assert.equal(response.responseText, 'some content');
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null,
                {headers: {'Range': 'bytes = 9-10', 'If-Range': response.headers['last-modified']}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 9-10/12');
            assert.equal(response.responseText, 'en');
        });
    });

    it('should respond with Content-Range when If-Range matches ETag', () => {
        let id;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null);
        }).then((response) => {
            assert.equal(response.statusCode, 200);
            assert.equal(response.responseText, 'some content');
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null,
                {headers: {'Range': 'bytes = 9-10', 'If-Range': response.headers['etag']}});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], 'bytes 9-10/12');
            assert.equal(response.responseText, 'en');
        });
    });

    it('should return the full content when If-Range does not match the last modified date or ETag', () => {
        let id;
        return TemporaryFile.makeTemporaryFile('some.dat', 'some content').then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null);
        }).then((response) => {
            assert.equal(response.statusCode, 200);
            assert.equal(response.responseText, 'some content');
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null,
                {'Range': 'bytes = 9-10', 'If-Range': 'foo'});
        }).then((response) => {
            assert.equal(response.statusCode, 200);
            assert.equal(response.responseText, 'some content');
        });
    });

    it('should respond with Content-Range across 64KB streaming chunks', () => {
        let id;
        const fileSize = 256 * 1024;
        const tokens = "0123456789abcdefghijklmnopqrstuvwxyz";
        let buffer = Buffer.allocUnsafe(fileSize);
        for (let i = 0; i < fileSize; i++)
            buffer[i] = Math.floor(Math.random() * 256);
        let startByte = 63 * 1024;
        let endByte = 128 * 1024 - 1;

        let responseBufferList = [];
        const responseHandler = (response) => {
            response.on('data', (chunk) => responseBufferList.push(chunk));
        };

        function verifyBuffer()
        {
            const responseBuffer = Buffer.concat(responseBufferList);
            for (let i = 0; i < endByte - startByte + 1; i++) {
                const actual = responseBuffer[i];
                const expected = buffer[startByte + i];
                assert.equal(actual, expected, `The byte at index ${i} should be identical. Expected ${expected} but got ${actual}`);
            }
        }

        return TemporaryFile.makeTemporaryFile('some.dat', buffer).then((stream) => {
            return PrivilegedAPI.sendRequest('upload-file', {newFile: stream}, {useFormData: true});
        }).then((response) => {
            id = response['uploadedFile']['id'];
            return TestServer.remoteAPI().sendHttpRequest(`/api/uploaded-file/${id}`, 'GET', null, null,
                {headers: {'Range': `bytes = ${startByte}-${endByte}`}, responseHandler});
        }).then((response) => {
            const headers = response.headers;
            assert.equal(response.statusCode, 206);
            assert.equal(headers['content-range'], `bytes ${startByte}-${endByte}/${fileSize}`);
            verifyBuffer();
        });
    });

});
