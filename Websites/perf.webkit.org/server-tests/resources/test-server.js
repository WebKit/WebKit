'use strict';

let assert = require('assert');
let childProcess = require('child_process');
let fs = require('fs');
let path = require('path');

let Config = require('../../tools/js/config.js');
let Database = require('../../tools/js/database.js');
let RemoteAPI = require('../../tools/js/remote.js').RemoteAPI;

let TestServer = (new class TestServer {
    constructor()
    {
        this._pidFile = null;
        this._testConfigPath = Config.path('testServer.config');
        this._dataDirectory = Config.path('dataDirectory');
        this._backupDataPath = null;
        this._pidWaitStart = null;
        this._shouldLog = false;
        this._pgsqlDirectory = null;
        this._server = null;

        this._databaseName = Config.value('testDatabaseName');
        this._databaseUser = Config.value('database.username');
        this._databaseHost = Config.value('database.host');
        this._databasePort = Config.value('database.port');
        this._database = null;
    }

    start()
    {        
        let testConfigContent = this._constructTestConfig(this._dataDirectory);
        fs.writeFileSync(this._testConfigPath, JSON.stringify(testConfigContent, null, '    '));

        this._ensureTestDatabase();
        this._ensureDataDirectory();

        return this._startApache();
    }

    stop()
    {
        this._restoreDataDirectory();

        return this._stopApache();
    }

    remoteAPI()
    {
        assert(this._server);
        RemoteAPI.configure(this._server);
        return RemoteAPI;
    }

    database()
    {
        assert(this._databaseName);
        if (!this._database)
            this._database = new Database(this._databaseName);
        return this._database;
    }

    _constructTestConfig(dataDirectory)
    {
        return {
            'siteTitle': 'Test Dashboard',
            'debug': true,
            'jsonCacheMaxAge': 600,
            'dataDirectory': dataDirectory,
            'database': {
                'host': Config.value('database.host'),
                'port': Config.value('database.port'),
                'username': Config.value('database.username'),
                'password': Config.value('database.password'),
                'name': Config.value('testDatabaseName'),
            },
            'universalSlavePassword': null,
            'maintenanceMode': false,
            'clusterStart': [2000, 1, 1, 0, 0],
            'clusterSize': [0, 2, 0],
            'defaultDashboard': [[]],
            'dashboards': {}
        }
    }

    _ensureDataDirectory()
    {

        let backupPath = path.resolve(this._dataDirectory, '../original-data');
        if (fs.existsSync(this._dataDirectory)) {
            assert.ok(!fs.existsSync(backupPath), `Both ${this._dataDirectory} and ${backupPath} exist. Cannot make a backup of data`);
            fs.rename(this._dataDirectory, backupPath);
            this._backupDataPath = backupPath;
        } else {
            if (fs.existsSync(backupPath)) // Assume this is a backup from the last failed run
                this._backupDataPath = backupPath;
            fs.mkdirSync(this._dataDirectory, 0o755);
        }
    }

    _restoreDataDirectory()
    {
        childProcess.execFileSync('rm', ['-rf', this._dataDirectory]);
        if (this._backupDataPath)
            fs.rename(this._backupDataPath, this._dataDirectory);
    }

    _ensureTestDatabase()
    {
        this._executePgsqlCommand('dropdb');
        this._executePgsqlCommand('createdb');
        this._executePgsqlCommand('psql', ['--command', `grant all privileges on database "${this._databaseName}" to "${this._databaseUser}";`]);
        this.initDatabase();
    }

    initDatabase()
    {
        if (this._database)
            this._database.disconnect();
        this._database = null;

        let initFilePath = Config.pathFromRoot('init-database.sql');
        this._executePgsqlCommand('psql', ['--username', this._databaseUser, '--file', initFilePath],
            {stdio: ['ignore', 'ignore', 'ignore']});
    }

    _executePgsqlCommand(command, args, options)
    {
        if (!this._pgsqlDirectory)
            this._pgsqlDirectory = this._determinePgsqlDirectory();
        childProcess.execFileSync(path.resolve(this._pgsqlDirectory, command),
            [this._databaseName, '--host', this._databaseHost, '--port', this._databasePort].concat(args || []), options);
    }

    _determinePgsqlDirectory()
    {
        try {
            let initdbLocation = childProcess.execFileSync('which', ['initdb']);
            return path.dirname(initdbLocation);
        } catch (error) {
            let serverPgsqlLocation = '/Applications/Server.app/Contents/ServerRoot/usr/bin/';
            childProcess.execFileSync(path.resolve(serverPgsqlLocation, 'initdb'), ['--version']);
            return serverPgsqlLocation;
        }
    }

    _startApache()
    {
        let pidFile = Config.path('testServer.httpdPID');
        let httpdConfig = Config.path('testServer.httpdConfig');
        let port = Config.value('testServer.port');
        let errorLog = Config.path('testServer.httpdErrorLog');
        let mutexFile = Config.path('testServer.httpdMutexDir');

        if (!fs.existsSync(mutexFile))
            fs.mkdirSync(mutexFile, 0o755);

        let args = [
            '-f', httpdConfig,
            '-c', `SetEnv ORG_WEBKIT_PERF_CONFIG_PATH ${this._testConfigPath}`,
            '-c', `Listen ${port}`,
            '-c', `PidFile ${pidFile}`,
            '-c', `ErrorLog ${errorLog}`,
            '-c', `Mutex file:${mutexFile}`,
            '-c', `DocumentRoot ${Config.serverRoot()}`];

        if (this._shouldLog)
            console.log(args);

        childProcess.execFileSync('httpd', args);

        this._server = {
            scheme: 'http',
            host: 'localhost',
            port: port,
        }
        this._pidWaitStart = Date.now();
        this._pidFile = pidFile;
        return new Promise(this._waitForPid.bind(this, true));
    }

    _stopApache()
    {
        if (!this._pidFile)
            return;

        let pid = fs.readFileSync(this._pidFile, 'utf-8').trim();

        if (this._shouldLog)
            console.log('Stopping', pid);

        childProcess.execFileSync('kill', ['-TERM', pid]);

        return new Promise(this._waitForPid.bind(this, false));
    }

    _waitForPid(shouldExist, resolve, reject)
    {
        if (fs.existsSync(this._pidFile) != shouldExist) {
            if (Date.now() - this._pidWaitStart > 5000)
                reject();
            else
                setTimeout(this._waitForPid.bind(this, shouldExist, resolve, reject), 100);
            return;
        }
        resolve();
    }
});


before(function () {
    this.timeout(5000);
    return TestServer.start();
});

beforeEach(function () {
    this.timeout(5000);
    return TestServer.initDatabase();
});

after(function () {
    this.timeout(5000);
    return TestServer.stop();
});

if (typeof module != 'undefined')
    module.exports = TestServer;
