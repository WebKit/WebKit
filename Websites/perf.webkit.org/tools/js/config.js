"use strict";

var fs = require('fs');
var path = require('path');

var Config = new (class Config {
    constructor()
    {
        this._rootDirectory =  path.resolve(__dirname, '../../'); 
        this._configPath = path.resolve(this._rootDirectory, 'config.json');
        this._content = null;
    }

    configFilePath() { return this._configPath; }

    value(key)
    {
        if (!this._content)
            this._content = JSON.parse(fs.readFileSync(this._configPath));

        let content = this._content;
        for (var key of key.split('.')) {
            if (!(key in content))
                return null;
            content = content[key];
        }

        return content;
    }

    path(key) { return path.resolve(this._rootDirectory, this.value(key)); }
    serverRoot() { return path.resolve(this._rootDirectory, 'public'); }
    pathFromRoot(relativePathFromRoot) { return path.resolve(this._rootDirectory, relativePathFromRoot); }
});

if (typeof module != 'undefined')
    module.exports = Config;
