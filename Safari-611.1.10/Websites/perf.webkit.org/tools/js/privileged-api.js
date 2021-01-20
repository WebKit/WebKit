"use strict";

class NodePrivilegedAPI {

    static sendRequest(path, data, options = {useFormData: false})
    {
        const clonedData = {slaveName: this._slaveName, slavePassword: this._slavePassword};
        for (const key in data)
            clonedData[key] = data[key];

        const fullPath = '/privileged-api/' + path;

        return options.useFormData
            ? RemoteAPI.postFormDataWithStatus(fullPath, clonedData, options)
            : RemoteAPI.postJSONWithStatus(fullPath, clonedData, options);
    }

    static configure(slaveName, slavePassword)
    {
        this._slaveName = slaveName;
        this._slavePassword = slavePassword;
    }
}

if (typeof module != 'undefined')
    module.exports.PrivilegedAPI = NodePrivilegedAPI;