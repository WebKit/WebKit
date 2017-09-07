#!/usr/local/bin/node
'use strict';

let BuildbotTriggerable = require('./js/buildbot-triggerable.js').BuildbotTriggerable;
let RemoteAPI = require('./js/remote.js').RemoteAPI;
let fs = require('fs');
let parseArguments = require('./js/parse-arguments.js').parseArguments;

function main(argv)
{
    let options = parseArguments(argv, [
        {name: '--server-config-json', required: true},
        {name: '--buildbot-config-json', required: true},
        {name: '--seconds-to-sleep', type: parseFloat, default: 120},
    ]);
    if (!options)
        return;

    syncLoop(options);
}

function syncLoop(options)
{
    let serverConfig = JSON.parse(fs.readFileSync(options['--server-config-json'], 'utf8'));
    let buildbotConfig = JSON.parse(fs.readFileSync(options['--buildbot-config-json'], 'utf8'));
    let buildbotRemote = new RemoteAPI(buildbotConfig.server);

    // v3 models use the global RemoteAPI to access the perf dashboard.
    global.RemoteAPI = new RemoteAPI(serverConfig.server);

    console.log(`Fetching the manifest...`);

    const makeTriggerable = function () {
        return new BuildbotTriggerable(buildbotConfig, global.RemoteAPI, buildbotRemote, serverConfig.slave, console)
    };

    Manifest.fetch().then(() => {
        const triggerable = makeTriggerable();
        return triggerable.initSyncers().then(() => triggerable.updateTriggerable());
    }).then(() => {
        return Manifest.fetch();
    }).then(() => {
        const triggerable = makeTriggerable();
        return triggerable.initSyncers().then(() => triggerable.syncOnce());
    }).catch((error) => {
        console.error(error);
        if (typeof(error.stack) == 'string') {
            for (let line of error.stack.split('\n'))
                console.error(line);
        }
    }).then(() => {
        setTimeout(syncLoop.bind(global, options), options['--seconds-to-sleep'] * 1000);
    });
}

main(process.argv);
