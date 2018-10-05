'use strict';

const assert = require('assert');
const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const Config = require('../../tools/js/config.js');
const Database = require('../../tools/js/database.js');
const RemoteAPI = require('../../tools/js/remote.js').RemoteAPI;
const BrowserPrivilegedAPI = require('../../public/v3/privileged-api.js').PrivilegedAPI;
const NodePrivilegedAPI = require('../../tools/js/privileged-api').PrivilegedAPI;

class TestServer {
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

        this._remote = null
    }

    start()
    {
        let testConfigContent = this.testConfig();
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
        assert(this._remote);
        return this._remote;
    }

    database()
    {
        assert(this._databaseName);
        if (!this._database)
            this._database = new Database(this._databaseName);
        return this._database;
    }

    testConfig()
    {
        return {
            'siteTitle': 'Test Dashboard',
            'debug': true,
            'jsonCacheMaxAge': 600,
            'dataDirectory': Config.value('dataDirectory'),
            'database': {
                'host': Config.value('database.host'),
                'port': Config.value('database.port'),
                'username': Config.value('database.username'),
                'password': Config.value('database.password'),
                'name': Config.value('testDatabaseName'),
            },
            'uploadFileLimitInMB': 2,
            'uploadUserQuotaInMB': 5,
            'uploadTotalQuotaInMB': 6,
            'uploadDirectory': Config.value('dataDirectory') + '/uploaded',
            'universalSlavePassword': null,
            'maintenanceMode': false,
            'clusterStart': [2000, 1, 1, 0, 0],
            'clusterSize': [0, 2, 0],
            'defaultDashboard': [[]],
            'dashboards': {},
            'summaryPages': []
        }
    }

    _ensureDataDirectory()
    {
        let backupPath = path.resolve(this._dataDirectory, '../original-data');
        if (fs.existsSync(this._dataDirectory)) {
            assert.ok(!fs.existsSync(backupPath), `Both ${this._dataDirectory} and ${backupPath} exist. Cannot make a backup of data`);
            fs.renameSync(this._dataDirectory, backupPath);
            this._backupDataPath = backupPath;
        } else if (fs.existsSync(backupPath)) // Assume this is a backup from the last failed run
            this._backupDataPath = backupPath;
        fs.mkdirSync(this._dataDirectory, 0o755);
        fs.mkdirSync(path.resolve(this._dataDirectory, 'uploaded'), 0o755);
    }

    _restoreDataDirectory()
    {
        childProcess.execFileSync('rm', ['-rf', this._dataDirectory]);
        if (this._backupDataPath)
            fs.renameSync(this._backupDataPath, this._dataDirectory);
    }

    cleanDataDirectory()
    {
        let fileList = fs.readdirSync(this._dataDirectory);
        for (let filename of fileList) {
            if (filename != 'uploaded')
                fs.unlinkSync(path.resolve(this._dataDirectory, filename));
        }
        fileList = fs.readdirSync(path.resolve(this._dataDirectory, 'uploaded'));
        for (let filename of fileList)
            fs.unlinkSync(path.resolve(this._dataDirectory, 'uploaded', filename));
    }

    _ensureTestDatabase()
    {
        try {
            this._executePgsqlCommand('dropdb');
        } catch (error) { }
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
            let initdbLocation = childProcess.execFileSync('which', ['initdb']).toString();
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
        let phpVersion = childProcess.execFileSync('php', ['-v'], {stdio: ['pipe', 'pipe', 'ignore']}).toString().includes('PHP 5') ? 'PHP5' : 'PHP7';

        if (!fs.existsSync(mutexFile))
            fs.mkdirSync(mutexFile, 0o755);

        let args = [
            '-f', httpdConfig,
            '-c', `SetEnv ORG_WEBKIT_PERF_CONFIG_PATH ${this._testConfigPath}`,
            '-c', `Listen ${port}`,
            '-c', `PidFile ${pidFile}`,
            '-c', `ErrorLog ${errorLog}`,
            '-c', `Mutex file:${mutexFile}`,
            '-c', `DocumentRoot ${Config.serverRoot()}`,
            '-D', phpVersion];

        if (this._shouldLog)
            console.log(args);

        childProcess.execFileSync('httpd', args);

        this._server = {
            scheme: 'http',
            host: 'localhost',
            port: port,
        };
        this._pidWaitStart = Date.now();
        this._pidFile = pidFile;

        this._remote = new RemoteAPI(this._server);

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

        this._pidWaitStart = Date.now();
        return new Promise(this._waitForPid.bind(this, false));
    }

    _waitForPid(shouldExist, resolve, reject)
    {
        if (fs.existsSync(this._pidFile) != shouldExist) {
            if (Date.now() - this._pidWaitStart > 8000)
                reject();
            else
                setTimeout(this._waitForPid.bind(this, shouldExist, resolve, reject), 100);
            return;
        }
        resolve();
    }

    inject(privilegedAPIType='browser')
    {
        console.assert(privilegedAPIType === 'browser' || privilegedAPIType === 'node');
        const useNodePrivilegedAPI = privilegedAPIType === 'node';
        const self = this;
        before(function () {
            this.timeout(10000);
            return self.start();
        });

        let originalRemote;
        let originalPrivilegedAPI;

        beforeEach(function () {
            this.timeout(10000);
            self.initDatabase();
            self.cleanDataDirectory();
            originalRemote = global.RemoteAPI;
            global.RemoteAPI = self._remote;
            self._remote.clearCookies();

            originalPrivilegedAPI = global.PrivilegedAPI;
            global.PrivilegedAPI = useNodePrivilegedAPI ? NodePrivilegedAPI : BrowserPrivilegedAPI;

            if (!useNodePrivilegedAPI) {
                global.PrivilegedAPI._token = null;
                global.PrivilegedAPI._expiration = null;
            }
        });

        after(function () {
            this.timeout(10000);
            global.RemoteAPI = originalRemote;
            global.PrivilegedAPI = originalPrivilegedAPI;
            return self.stop();
        });
    }
}

if (!global.TestServer)
    global.TestServer = new TestServer;

if (typeof module != 'undefined')
    module.exports = global.TestServer;
