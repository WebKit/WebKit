"use strict";

var pg = require('pg');
var config = require('./config.js');

class Database {
    constructor()
    {
        this._client = null;
    }

    connect(options)
    {
        console.assert(this._client === null);

        let username = config.value('database.username');
        let password = config.value('database.password');
        let host = config.value('database.host');
        let port = config.value('database.port');
        let name = config.value('database.name');

        // No need to worry about escaping strings since they are only set by someone who can write to config.json.
        let connectionString = `tcp://${username}:${password}@${host}:${port}/${name}`;

        let client = new pg.Client(connectionString);
        if (!options || !options.keepAlive) {
            client.on('drain', function () {
                client.end();
            });
        }

        this._client = client;

        return new Promise(function (resolve, reject) {
            client.connect(function (error) {
                if (error)
                    reject(error);
                resolve();
            });
        });
    }

    disconnect()
    {
        if (this._client) {
            this._client.end();
            this._client = null;
        }
    }
}

if (typeof module != 'undefined')
    module.exports = Database;
