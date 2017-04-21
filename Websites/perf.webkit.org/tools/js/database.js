"use strict";

const pg = require('pg');
const config = require('./config.js');

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
        if (!options || !options.keepAlive)
            client.on('drain', this.disconnect.bind(this));

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

    selectAll(table, columnToSortBy)
    {
        return this.selectRows(table, {}, {sortBy: columnToSortBy});
    }

    selectFirstRow(table, params, columnToSortBy)
    {
        return this.selectRows(table, params, {sortBy: columnToSortBy, limit: 1}).then(function (rows) {
            return rows[0];
        });
    }

    selectRows(table, params, options)
    {
        let prefix = '';
        if (table in tableToPrefixMap)
            prefix = tableToPrefixMap[table] + '_';

        options = options || {};

        let columnNames = [];
        let placeholders = [];
        let values = [];
        for (let name in params) {
            columnNames.push(prefix + name);
            placeholders.push('$' + columnNames.length);
            values.push(params[name]);
        }

        let qualifier = '';
        if (columnNames.length)
            qualifier += ` WHERE (${columnNames.join(',')}) = (${placeholders.join(',')})`;
        qualifier += ` ORDER BY ${prefix}${options.sortBy || 'id'}`;
        if (options && options.limit)
            qualifier += ` LIMIT ${options.limit}`;

        return this.query(`SELECT * FROM ${table}${qualifier}`, values).then(function (result) {
            return result.rows.map(function (row) {
                let formattedResult = {};
                for (let columnName in row) {
                    let formattedName = columnName;
                    if (formattedName.startsWith(prefix))
                        formattedName = formattedName.substring(prefix.length);
                    formattedResult[formattedName] = row[columnName];
                }
                return formattedResult;
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

const tableToPrefixMap = {
    'aggregators': 'aggregator',
    'analysis_strategies': 'strategy',
    'analysis_tasks': 'task',
    'analysis_test_groups': 'testgroup',
    'bug_trackers': 'tracker',
    'build_triggerables': 'triggerable',
    'build_requests': 'request',
    'build_slaves': 'slave',
    'builds': 'build',
    'builders': 'builder',
    'commits': 'commit',
    'commit_ownerships': 'commit',
    'committers': 'committer',
    'test_configurations': 'config',
    'test_metrics': 'metric',
    'test_runs': 'run',
    'tests': 'test',
    'tracker_repositories': 'tracrepo',
    'triggerable_configurations': 'trigconfig',
    'triggerable_repository_groups': 'repositorygroup',
    'triggerable_repositories': 'trigrepo',
    'platforms': 'platform',
    'reports': 'report',
    'repositories': 'repository',
    'commit_sets': 'commitset',
    'commit_set_items': 'commitset',
    'run_iterations': 'iteration',
    'uploaded_files': 'file',
}

if (typeof module != 'undefined')
    module.exports = Database;
