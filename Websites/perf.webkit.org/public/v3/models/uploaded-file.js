
class UploadedFile extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        this._createdAt = new Date(object.createdAt);
        this._filename = object.filename;
        this._author = object.author;
        this._size = object.size;
        this._sha256 = object.sha256;
        this.ensureNamedStaticMap('sha256')[object.sha256] = this;
    }

    static uploadFile(file, uploadProgressCallback = null)
    {
        return PrivilegedAPI.sendRequest('upload-file', {'newFile': file}, {useFormData: true, uploadProgressCallback}).then((rawData) => {
            return UploadedFile.ensureSingleton(rawData['uploadedFile'].id, rawData['uploadedFile']);
        });
    }

    static fetchUnloadedFileWithIdenticalHash(file)
    {
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
            return RemoteAPI.getJSONWithStatus(`../api/uploaded-file?sha256=${sha256}`).then((rawData) => {
                if (!rawData['uploadedFile'])
                    return null;
                return UploadedFile.ensureSingleton(rawData['uploadedFile'].id, rawData['uploadedFile']);
            });
        });
    }

    static _computeSHA256Hash(content)
    {
        return crypto.subtle.digest('SHA-256', content).then((digest) => {
            return Array.from(new Uint8Array(digest)).map((byte) => {
                if (byte < 0x10)
                    return '0' + byte.toString(16);
                return byte.toString(16);
            }).join('');
        });
    }

}
