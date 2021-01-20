
const assert = require('assert');
const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const Config = require('../../tools/js/config.js');

class TemporaryFile {
    static makeTemporaryFileOfSizeInMB(name, sizeInMB, characterToFill = 'a')
    {
        let megabyteString = characterToFill;
        for (let i = 0; i < 20; i++)
            megabyteString = megabyteString + megabyteString;
        assert.equal(megabyteString.length, 1024 * 1024);

        let content = '';
        for (let i = 0; i < sizeInMB; i++)
            content += megabyteString;
        
        return this.makeTemporaryFile(name, content);
    }

    static makeTemporaryFile(name, content)
    {
        const newPath = path.resolve(TemporaryFile._tempDir, name);
        return new Promise((resolve) => {
            return fs.writeFile(newPath, content, () => {
                resolve(fs.createReadStream(newPath));
            });
        });
    }

    static inject()
    {
        beforeEach(() => {
            this._tempDir = fs.mkdtempSync(path.resolve(Config.path('dataDirectory'), 'temp/'));
        });

        afterEach(() => {
            childProcess.execFileSync('rm', ['-rf', this._tempDir]);
            this._tempDir = null;
        });
    }
}
TemporaryFile._tempDir = null;

if (typeof module != 'undefined')
    module.exports.TemporaryFile = TemporaryFile;
