#!/usr/local/bin/node

var assert = require('assert');
var crypto = require('crypto');
var fs = require('fs');
var http = require('http');
var path = require('path');
var vm = require('vm');

function connect(keepAlive) {
    var pg = require('pg');
    var database = config('database');
    var connectionString = 'tcp://' + database.username + ':' + database.password + '@' + database.host + ':' + database.port
        + '/' + database.name;

    var client = new pg.Client(connectionString);
    if (!keepAlive) {
        client.on('drain', function () {
            client.end();
            client = undefined;
        });
    }
    client.connect();

    return client;
}

function pathToDatabseSQL(relativePath) {
    return path.resolve(__dirname, 'init-database.sql');
}

function pathToTests(testName) {
    return path.resolve(__dirname, 'tests', testName);
}

var configurationJSON = require('./config.json');
function config(key) {
    return configurationJSON[key];
}

function TaskQueue() {
    var queue = [];
    var numberOfRemainingTasks = 0;
    var emptyQueueCallback;

    function startTasksInQueue() {
        if (!queue.length)
            return emptyQueueCallback();

        var swappedQueue = queue;
        queue = [];

        // Increase the counter before the loop in the case taskCallback is called synchronously.
        numberOfRemainingTasks += swappedQueue.length;
        for (var i = 0; i < swappedQueue.length; ++i)
            swappedQueue[i](null, taskCallback);
    }

    function taskCallback(error) {
        // FIXME: Handle error.
        console.assert(numberOfRemainingTasks > 0);
        numberOfRemainingTasks--;
        if (!numberOfRemainingTasks)
            setTimeout(startTasksInQueue, 0);
    }

    this.addTask = function (task) { queue.push(task); }
    this.start = function (callback) {
        emptyQueueCallback = callback;
        startTasksInQueue();
    }
}

function SerializedTaskQueue() {
    var queue = [];

    function executeNextTask(error) {
        // FIXME: Handle error.
        var callback = queue.pop();
        setTimeout(function () { callback(null, executeNextTask); }, 0);
    }

    this.addTask = function (task) { queue.push(task); }
    this.start = function (callback) {
        queue.push(callback);
        queue.reverse();
        executeNextTask();
    }
}

function main() {
    var client = connect(true);
    confirmUserWantsDatabaseToBeInitializedIfNeeded(client, function (error, shouldContinue) {
        if (error)
            console.error(error);

        if (error || !shouldContinue) {
            client.end();
            process.exit(1);
            return;
        }

        initializeDatabase(client, function (error) {
            if (error) {
                console.error('Failed to initialize the database', error);
                client.end();
                process.exit(1);
            }

            var testCaseQueue = new SerializedTaskQueue();
            var testFileQueue = new SerializedTaskQueue();
            fs.readdirSync(pathToTests()).map(function (testFile) {
                if (!testFile.match(/.js$/))
                    return;
                testFileQueue.addTask(function (error, callback) {
                    var testContent = fs.readFileSync(pathToTests(testFile), 'utf-8');
                    var environment = new TestEnvironment(testCaseQueue);
                    vm.runInNewContext(testContent, environment, pathToTests(testFile));
                    callback();
                });
            });
            testFileQueue.start(function () {
                client.end();
                testCaseQueue.start(function () {
                    console.log('DONE');
                });
            });
        });
    });
}

function confirmUserWantsDatabaseToBeInitializedIfNeeded(client, callback) {
    function fetchTableNames(error, callback) {
        if (error)
            return callback(error);

        client.query('SELECT table_name FROM information_schema.tables WHERE table_type = \'BASE TABLE\' and table_schema = \'public\'', function (error, result) {
            if (error)
                return callback(error);
            callback(null, result.rows.map(function (row) { return row['table_name']; }));            
        });
    }

    function findNonEmptyTable(error, list, callback) {
        if (error || !list.length)
            return callback(error);

        var tableName = list.shift();
        client.query('SELECT COUNT(*) FROM ' + tableName + ' LIMIT 1', function (error, result) {
            if (error)
                return callback(error);

            if (result.rows[0]['count'])
                return callback(null, tableName);

            findNonEmptyTable(null, list, callback);
        });
    }

    fetchTableNames(null, function (error, tableNames) {
        if (error)
            return callback(error, false);

        findNonEmptyTable(null, tableNames, function (error, nonEmptyTable) {
            if (error)
                return callback(error, false);

            if (!nonEmptyTable)
                return callback(null, true);

            console.warn('Table ' + nonEmptyTable + ' is not empty but running tests will drop all tables.');
            askYesOrNoQuestion(null, 'Do you really want to continue?', callback);
        });
    });
}

function askYesOrNoQuestion(error, question, callback) {
    if (error)
        return callback(error);

    process.stdout.write(question + ' (y/n):');
    process.stdin.resume();
    process.stdin.setEncoding('utf-8');
    process.stdin.on('data', function (line) {
        line = line.trim();
        if (line === 'y') {
            process.stdin.pause();
            callback(null, true);
        } else if (line === 'n') {
            process.stdin.pause();
            callback(null, false);
        } else
            console.warn('Invalid input:', line);
    });
}

function initializeDatabase(client, callback) {
    var commaSeparatedSqlStatements = fs.readFileSync(pathToDatabseSQL(), "utf8");

    var firstError;
    var queue = new TaskQueue();
    commaSeparatedSqlStatements.split(/;\s*/).forEach(function (statement) {
        queue.addTask(function (error, callback) {
            client.query(statement, function (error) {
                if (error && !firstError)
                    firstError = error;
                callback();
            });
        })
    });

    queue.start(function () { callback(firstError); });
}

var currentTestContext;
function TestEnvironment(testCaseQueue) {
    var currentTestGroup;

    this.assert = assert;
    this.console = console;

    // describe("~", function () {
    //     it("~", function () { assert(true); });
    // });
    this.describe = function (testGroup, callback) {
        currentTestGroup = testGroup;
        callback();
    }

    this.it = function (testCaseDescription, testCase) {
        testCaseQueue.addTask(function (error, callback) {
            currentTestContext = new TestContext(currentTestGroup, testCaseDescription, function () {
                currentTestContext = null;
                initializeDatabase(connect(), callback);
            });
            testCase();
        });
    }

    this.postJSON = function (path, content, callback) {
        sendHttpRequest(path, 'POST', 'application/json', JSON.stringify(content), function (error, response) {
            assert.ifError(error);
            callback(response);
        });
    }

    this.httpGet = function (path, callback) {
        sendHttpRequest(path, 'GET', null, '', function (error, response) {
            assert.ifError(error);
            callback(response);
        });
    }

    this.httpPost= function (path, content, callback) {
        var contentType = null;
        if (typeof(content) != "string") {
            contentType = 'application/x-www-form-urlencoded';
            var components = [];
            for (var key in content)
                components.push(key + '=' + escape(content[key]));
            content = components.join('&');
        }
        sendHttpRequest(path, 'POST', contentType, content, function (error, response) {
            assert.ifError(error);
            callback(response);
        });
    }

    this.queryAndFetchAll = function (query, parameters, callback) {
        var client = connect();
        client.query(query, parameters, function (error, result) {
            setTimeout(function () {
                assert.ifError(error);
                callback(result.rows);
            }, 0);
        });
    }

    this.sha256 = function (data) {
        var hash = crypto.createHash('sha256');
        hash.update(data);
        return hash.digest('hex');
    }

    this.notifyDone = function () { currentTestContext.done(); }
}

process.on('uncaughtException', function (error) {
    if (!currentTestContext)
        throw error;
    currentTestContext.logError('Uncaught exception', error);
    currentTestContext.done();
});

function sendHttpRequest(path, method, contentType, content, callback) {
    var options = config('testServer');
    options.path = path;
    options.method = method;

    var request = http.request(options, function (response) {
        var responseText = '';
        response.setEncoding('utf8');
        response.on('data', function (chunk) { responseText += chunk; });
        response.on('end', function () {
            setTimeout(function () {
                callback(null, {statusCode: response.statusCode, responseText: responseText});
            }, 0);
        });
    });
    request.on('error', callback);
    if (contentType)
        request.setHeader('Content-Type', contentType);
    if (content)
        request.write(content);
    request.end();
}

function TestContext(testGroup, testCase, callback) {
    var failed = false;

    this.description = function () {
        return testGroup + ' ' + testCase;
    }
    this.done = function () {
        if (!failed)
            console.log('PASS');
        callback();
    }
    this.logError = function (error, details) {
        failed = true;
        console.error(error, details);
    }

    process.stdout.write(this.description() + ': ');
}

main();
