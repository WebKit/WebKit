'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;

function webkitCommit()
{
    return new CommitLog(1, {
        repository: MockModels.webkit,
        revision: '200805',
        time: +(new Date('2016-05-13T00:55:57.841344Z')),
    });
}

function oldWebKitCommit()
{
    return new CommitLog(2, {
        repository: MockModels.webkit,
        revision: '200574',
        time: +(new Date('2016-05-09T14:59:23.553767Z')),
    });
}

function gitWebKitCommit()
{
    return new CommitLog(3, {
        repository: MockModels.webkit,
        revision: '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
        time: +(new Date('2016-05-13T00:55:57.841344Z')),
    });
}

function oldGitWebKitCommit()
{
    return new CommitLog(4, {
        repository: MockModels.webkit,
        revision: 'ffda14e6db0746d10d0f050907e4a7325851e502',
        time: +(new Date('2016-05-09T14:59:23.553767Z')),
    });
}

function osxCommit()
{
    return new CommitLog(5, {
        repository: MockModels.osx,
        revision: '10.11.4 15E65',
        time: null,
    });
}

function oldOSXCommit()
{
    return new CommitLog(6, {
        repository: MockModels.osx,
        revision: '10.11.3 15D21',
        time: null,
    });
}

describe('CommitLog', function () {
    MockModels.inject();

    describe('label', function () {
        it('should prefix SVN revision with "r"', function () {
            assert.equal(webkitCommit().label(), 'r200805');
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.equal(gitWebKitCommit().label(), '6f8b0dbb');
        });

        it('should not modify OS X version', function () {
            assert.equal(osxCommit().label(), '10.11.4 15E65');
        });
    });

    describe('title', function () {
        it('should prefix SVN revision with "r"', function () {
            assert.equal(webkitCommit().title(), 'WebKit at r200805');
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.equal(gitWebKitCommit().title(), 'WebKit at 6f8b0dbb');
        });

        it('should not modify OS X version', function () {
            assert.equal(osxCommit().title(), 'OS X at 10.11.4 15E65');
        });
    });

    describe('diff', function () {
        it('should use label() as the label the previous commit is missing', function () {
            assert.deepEqual(webkitCommit().diff(), {
                label: 'r200805',
                url: 'http://trac.webkit.org/changeset/200805',
                repository: MockModels.webkit
            });

            assert.deepEqual(gitWebKitCommit().diff(), {
                label: '6f8b0dbb',
                url: 'http://trac.webkit.org/changeset/6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
                repository: MockModels.webkit,
            });

            assert.deepEqual(osxCommit().diff(), {
                label: '10.11.4 15E65',
                url: '',
                repository: MockModels.osx,
            });
        });

        it('should use increment the old SVN revision by 1', function () {
            assert.deepEqual(webkitCommit().diff(oldWebKitCommit()), {
                label: 'r200574-r200805',
                url: '',
                repository: MockModels.webkit
            });
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.deepEqual(gitWebKitCommit().diff(oldGitWebKitCommit()), {
                label: 'ffda14e6..6f8b0dbb',
                url: '',
                repository: MockModels.webkit
            });
        });

        it('should surround "-" with spaces', function () {
            assert.deepEqual(osxCommit().diff(oldOSXCommit()), {
                label: '10.11.3 15D21 - 10.11.4 15E65',
                url: '',
                repository: MockModels.osx
            });
        });
    });
});
