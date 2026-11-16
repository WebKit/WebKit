"use strict";

class NodePrivilegedAPI {

    static sendRequest(path, data, options = {useFormData: false})
    {
        const clonedData = {workerName: this._workerName, workerPassword: this._workerPassword};
        for (const key in data)
            clonedData[key] = data[key];

        const fullPath = '/privileged-api/' + path;

        return options.useFormData
            ? RemoteAPI.postFormDataWithStatus(fullPath, clonedData, options)
            : RemoteAPI.postJSONWithStatus(fullPath, clonedData, options);
    }

    static configure(workerName, workerPassword)
    {
        this._workerName = workerName;
        this._workerPassword = workerPassword;
    }
}

if (typeof module != 'undefined')
    module.exports.PrivilegedAPI = NodePrivilegedAPI;