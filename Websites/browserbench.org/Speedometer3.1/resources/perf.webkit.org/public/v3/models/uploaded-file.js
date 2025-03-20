
class UploadedFile extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        this._createdAt = new Date(object.createdAt);
        this._deletedAt = object.deletedAt ? new Date(object.deletedAt) : null;
        this._filename = object.filename;
        this._extension = object.extension;
        this._author = object.author;
        this._size = object.size;
        this._sha256 = object.sha256;
        this.ensureNamedStaticMap('sha256')[object.sha256] = this;
    }

    createdAt() { return this._createdAt; }
    deletedAt() { return this._deletedAt; }
    filename() { return this._filename; }
    author() { return this._author; }
    size() { return this._size; }
    label() { return this.filename(); }
    url() { return RemoteAPI.url(`/api/uploaded-file/${this.id()}${this._extension}`); }

    static uploadFile(file, uploadProgressCallback = null)
    {
        if (file.size > UploadedFile.fileUploadSizeLimit)
            return Promise.reject('FileSizeLimitExceeded');
        return PrivilegedAPI.sendRequest('upload-file', {'newFile': file}, {useFormData: true, uploadProgressCallback}).then((rawData) => {
            return UploadedFile.ensureSingleton(rawData['uploadedFile'].id, rawData['uploadedFile']);
        });
    }

    static fetchUploadedFileWithIdenticalHash(file)
    {
        if (file.size > UploadedFile.fileUploadSizeLimit)
            return Promise.reject('FileSizeLimitExceeded');
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = () => reject();
            reader.readAsArrayBuffer(file);
        }).then((content) => {
            return this._computeSHA256Hash(content);
        }).then((sha256) => {
            const map = this.namedStaticMap('sha256');
            if (map && sha256 in map)
                return map[sha256];
            return RemoteAPI.getJSON(`../api/uploaded-file?sha256=${sha256}`).then((rawData) => {
                if (rawData['status'] == 'NotFound' || !rawData['uploadedFile'])
                    return null;
                if (rawData['status'] != 'OK')
                    return Promise.reject(rawData['status']);
                return UploadedFile.ensureSingleton(rawData['uploadedFile'].id, rawData['uploadedFile']);
            });
        });
    }

    static _computeSHA256Hash(content)
    {
        return (crypto.subtle || crypto.webkitSubtle).digest('SHA-256', content).then((digest) => {
            return Array.from(new Uint8Array(digest)).map((byte) => {
                if (byte < 0x10)
                    return '0' + byte.toString(16);
                return byte.toString(16);
            }).join('');
        });
    }

}

UploadedFile.fileUploadSizeLimit = 0;

if (typeof module != 'undefined')
    module.exports.UploadedFile = UploadedFile;
