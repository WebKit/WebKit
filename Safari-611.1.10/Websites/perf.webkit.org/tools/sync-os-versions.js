#!/usr/local/bin/node
'use strict';


let OSBuildFetcher = require('./js/os-build-fetcher.js').OSBuildFetcher;
let RemoteAPI = require('./js/remote.js').RemoteAPI;
let Subprocess = require('./js/subprocess.js').Subprocess;
let fs = require('fs');
let parseArguments = require('./js/parse-arguments.js').parseArguments;

function main(argv)
{
    let options = parseArguments(argv, [
        {name: '--os-config-json', required: true},
        {name: '--server-config-json', required: true},
        {name: '--seconds-to-sleep', type: parseFloat, default: 43200},
    ]);
    if (!options)
        return;

    syncLoop(options);
}

function syncLoop(options)
{
    let osConfigList = JSON.parse(fs.readFileSync(options['--os-config-json'], 'utf8'));
    let serverConfig = JSON.parse(fs.readFileSync(options['--server-config-json'], 'utf8'));

    const remoteAPI = new RemoteAPI(serverConfig.server);

    const fetchers = osConfigList.map((osConfig) => new OSBuildFetcher(osConfig, remoteAPI, serverConfig.slave, new Subprocess, console));
    OSBuildFetcher.fetchReportAndUpdateCommits(fetchers).catch((error) => {
        console.error(error);
        if (typeof(error.stack) == 'string') {
            for (let line of error.stack.split('\n'))
                console.error(line);
        }
    }).then(() => {
        const secondsToSleep = options['--seconds-to-sleep'];
        console.log(`Sleeping for ${Math.floor(secondsToSleep / 3600)}h ${Math.floor(secondsToSleep / 60) % 60}m ${secondsToSleep % 60}s`);
        setTimeout(() => syncLoop(options), secondsToSleep * 1000);
    });
}

main(process.argv);
