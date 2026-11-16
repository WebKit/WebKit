'use strict';
const childProcess = require('child_process');

class Subprocess {
    execute(command) {
        return new Promise((resolve, reject) => {
            childProcess.execFile(command[0], command.slice(1), (error, stdout, stderr) => {
                if (error)
                    reject(stderr);
                else
                    resolve(stdout);
            });
        });
    }
};

if (typeof module != 'undefined')
    module.exports.Subprocess = Subprocess;
