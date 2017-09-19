"use strict";

const assert = require('assert');
require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;

function createPatch()
{
    return new UploadedFile(453, {'createdAt': new Date('2017-05-01T19:16:53Z'), 'filename': 'patch.dat', 'extension': '.dat', 'author': 'some user',
        size: 534637, sha256: '169463c8125e07c577110fe144ecd63942eb9472d438fc0014f474245e5df8a1'});
}

function createRoot()
{
    return new UploadedFile(456, {'createdAt': new Date('2017-05-01T21:03:27Z'), 'filename': 'root.dat', 'extension': '.dat', 'author': 'some user',
        size: 16452234, sha256: '03eed7a8494ab8794c44b7d4308e55448fc56f4d6c175809ba968f78f656d58d'});
}

function customCommitSetWithoutOwnedCommit()
{
    const customCommitSet = new CustomCommitSet;
    customCommitSet.setRevisionForRepository(MockModels.osx, '10.11.4 15E65');
    customCommitSet.setRevisionForRepository(MockModels.webkit, '200805');
    return customCommitSet;
}

function customCommitSetWithOwnedCommit()
{
    const customCommitSet = new CustomCommitSet;
    customCommitSet.setRevisionForRepository(MockModels.osx, '10.11.4 15E65');
    customCommitSet.setRevisionForRepository(MockModels.ownerRepository, 'OwnerRepository-r0');
    customCommitSet.setRevisionForRepository(MockModels.ownedRepository, 'OwnedRepository-r0', null, 'OwnerRepository-r0');
    return customCommitSet;
}

function customCommitSetWithPatch()
{
    const customCommitSet = new CustomCommitSet;
    const patch = createPatch();
    customCommitSet.setRevisionForRepository(MockModels.osx, '10.11.4 15E65');
    customCommitSet.setRevisionForRepository(MockModels.webkit, '200805', patch);
    return customCommitSet;
}

function customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository()
{
    const customCommitSet = new CustomCommitSet;
    customCommitSet.setRevisionForRepository(MockModels.osx, '10.11.4 15E65');
    customCommitSet.setRevisionForRepository(MockModels.webkit, '200805');
    customCommitSet.setRevisionForRepository(MockModels.ownedWebkit, 'owned-200805', null, '10.11.4 15E65');
    return customCommitSet;
}

describe('CustomCommitSet', () => {
    MockModels.inject();

    describe('Test custom commit set without owned commit', () => {
        it('should have right revision for a given repository', () => {
            const commitSet = customCommitSetWithoutOwnedCommit();
            assert.equal(commitSet.revisionForRepository(MockModels.osx), '10.11.4 15E65');
            assert.equal(commitSet.revisionForRepository(MockModels.webkit), '200805');
        });

        it('should have no patch for any repository', () => {
            const commitSet = customCommitSetWithoutOwnedCommit();
            assert.equal(commitSet.patchForRepository(MockModels.osx), null);
            assert.equal(commitSet.patchForRepository(MockModels.webkit), null);
        });

        it('should have no owner revision for a given repository', () => {
            const commitSet = customCommitSetWithoutOwnedCommit();
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.osx), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.webkit), null);
        });

        it('should return all repositories in it', () => {
            const commitSet = customCommitSetWithoutOwnedCommit();
            assert.deepEqual(commitSet.repositories(), [MockModels.osx, MockModels.webkit]);
        });
    });

    describe('Test custom commit set with owned commit', () => {
        it('should have right revision for a given repository', () => {
            const commitSet = customCommitSetWithOwnedCommit();
            assert.equal(commitSet.revisionForRepository(MockModels.osx), '10.11.4 15E65');
            assert.equal(commitSet.revisionForRepository(MockModels.ownerRepository), 'OwnerRepository-r0');
            assert.equal(commitSet.revisionForRepository(MockModels.ownedRepository), 'OwnedRepository-r0');
        });

        it('should have no patch for any repository', () => {
            const commitSet = customCommitSetWithOwnedCommit();
            assert.equal(commitSet.patchForRepository(MockModels.osx), null);
            assert.equal(commitSet.patchForRepository(MockModels.ownerRepository), null);
            assert.equal(commitSet.patchForRepository(MockModels.ownedRepository), null);
        });

        it('should have right owner revision for an owned repository', () => {
            const commitSet = customCommitSetWithOwnedCommit();
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.osx), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.ownerRepository), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.ownedRepository), 'OwnerRepository-r0');
        });

        it('should return all repositories in it', () => {
            const commitSet = customCommitSetWithOwnedCommit();
            assert.deepEqual(commitSet.repositories(), [MockModels.osx, MockModels.ownerRepository, MockModels.ownedRepository]);
        });
    });

    describe('Test custom commit set with patch', () => {
        it('should have right revision for a given repository', () => {
            const commitSet = customCommitSetWithPatch();
            assert.equal(commitSet.revisionForRepository(MockModels.osx), '10.11.4 15E65');
            assert.equal(commitSet.revisionForRepository(MockModels.webkit), '200805');
        });

        it('should have a patch for a repository with patch specified', () => {
            const commitSet = customCommitSetWithPatch();
            assert.equal(commitSet.patchForRepository(MockModels.osx), null);
            assert.deepEqual(commitSet.patchForRepository(MockModels.webkit), createPatch());
        });

        it('should have no owner revision for a given repository', () => {
            const commitSet = customCommitSetWithPatch();
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.osx), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.webkit), null);
        });

        it('should return all repositories in it', () => {
            const commitSet = customCommitSetWithPatch();
            assert.deepEqual(commitSet.repositories(), [MockModels.osx, MockModels.webkit]);
        });
    });

    describe('Test custom commit set with owned repository has same name as non-owned repository',  () => {
        it('should have right revision for a given repository', () => {
            const commitSet = customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository();
            assert.equal(commitSet.revisionForRepository(MockModels.osx), '10.11.4 15E65');
            assert.equal(commitSet.revisionForRepository(MockModels.webkit), '200805');
            assert.equal(commitSet.revisionForRepository(MockModels.ownedWebkit), 'owned-200805');
        });

        it('should have no patch for any repository', () => {
            const commitSet = customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository();
            assert.equal(commitSet.patchForRepository(MockModels.osx), null);
            assert.equal(commitSet.patchForRepository(MockModels.webkit), null);
            assert.equal(commitSet.patchForRepository(MockModels.ownedWebkit), null);
        });

        it('should have right owner revision for an owned repository', () => {
            const commitSet = customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository();
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.osx), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.webkit), null);
            assert.equal(commitSet.ownerRevisionForRepository(MockModels.ownedWebkit), '10.11.4 15E65');
        });

        it('should return all repositories in it', () => {
            const commitSet = customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository();
            assert.deepEqual(commitSet.repositories(), [MockModels.osx, MockModels.webkit, MockModels.ownedWebkit]);
        });
    });

    describe('Test custom commit set equality function', () => {
        it('should be equal to same custom commit set', () => {
            assert.deepEqual(customCommitSetWithoutOwnedCommit(), customCommitSetWithoutOwnedCommit());
            assert.deepEqual(customCommitSetWithOwnedCommit(), customCommitSetWithOwnedCommit());
            assert.deepEqual(customCommitSetWithPatch(), customCommitSetWithPatch());
            assert.deepEqual(customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository(),
                customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository());
        });

        it('should not be equal even if non-owned revisions are the same', () => {
            const commitSet0 = customCommitSetWithoutOwnedCommit();
            const commitSet1 = customCommitSetWithOwnedRepositoryHasSameNameAsNotOwnedRepository();
            assert.equal(commitSet0.equals(commitSet1), false);
        });
    });

    describe('Test custom commit set custom root operations', () => {
        it('should return empty custom roots if no custom root specified', () => {
            assert.deepEqual(customCommitSetWithoutOwnedCommit().customRoots(), []);
        });

        it('should return root if root is added into commit set', () => {
            const commitSet = customCommitSetWithoutOwnedCommit();
            commitSet.addCustomRoot(createRoot());
            assert.deepEqual(commitSet.customRoots(), [createRoot()]);
        });
    });
});
