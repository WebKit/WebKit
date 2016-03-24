"use strict";

var pg = require('pg');
var config = require('./config.js');

class Database {
    constructor(databaseName)
    {
        this._client = null;
        this._databaseName = databaseName || config.value('database.name');
    }

    connect(options)
    {
        console.assert(this._client === null);

        let username = config.value('database.username');
        let password = config.value('database.password');
        let host = config.value('database.host');
        let port = config.value('database.port');

        // No need to worry about escaping strings since they are only set by someone who can write to config.json.
        let connectionString = `tcp://${username}:${password}@${host}:${port}/${this._databaseName}`;

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

    query(statement, parameters)
    {
        console.assert(this._client);
        var client = this._client;
        return new Promise(function (resolve, reject) {
            client.query(statement, parameters || [], function (error, result) {
                if (error)
                    reject(error);
                else
                    resolve(result);
            });
        });
    }

    insert(table, parameters)
    {
        let columnNames = [];
        let placeholders = [];
        let values = [];
        for (let name in parameters) {
            values.push(parameters[name]);
            if (table in tableToPrefixMap)
                name = tableToPrefixMap[table] + '_' + name;
            columnNames.push(name);
            placeholders.push(`\$${placeholders.length + 1}`);
        }
        return this.query(`INSERT INTO ${table} (${columnNames}) VALUES (${placeholders})`, values);
    }
}

let tableToPrefixMap = {
    'analysis_tasks': 'task',
    'analysis_test_groups': 'testgroup',
    'build_triggerables': 'triggerable',
    'build_requests': 'request',
    'commits': 'commit',
    'test_metrics': 'metric',
    'tests': 'test',
    'platforms': 'platform',
    'repositories': 'repository',
    'root_sets': 'rootset',
    'roots': 'root',
}

if (typeof module != 'undefined')
    module.exports = Database;
