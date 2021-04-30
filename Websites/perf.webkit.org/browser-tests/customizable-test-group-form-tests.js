
describe('CustomizableTestGroupFormTests', () => {
    const scripts = ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'models/data-model.js', 'models/commit-log.js',
        'models/commit-set.js', 'models/repository.js', 'components/test-group-form.js', 'components/customizable-test-group-form.js'];

    async function createCustomizableTestGroupFormWithContext(context)
    {
        await context.importScripts(scripts, 'ComponentBase', 'DataModelObject', 'Repository', 'CommitLog', 'CommitSet', 'CustomizableTestGroupForm', 'MockRemoteAPI');
        const customizableTestGroupForm = new context.symbols.CustomizableTestGroupForm;
        context.document.body.appendChild(customizableTestGroupForm.element());
        return customizableTestGroupForm;
    }

    const commitObjectA = {
        "id": "185326",
        "revision": "210948",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": 1541494949681,
        "authorName": "Zalan Bujtas",
        "authorEmail": "zalan@apple.com",
        "message": "a message",
    };

    const commitObjectB = {
        "id": "185334",
        "revision": "210949",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": 1541494949682,
        "authorName": "Chris Dumez",
        "authorEmail": "cdumez@apple.com",
        "message": "some message",
    };

    const commitObjectC = {
        "id": "185336",
        "revision": "210950",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": 1541494949682,
        "authorName": "Chris Dumez",
        "authorEmail": "cdumez@apple.com",
        "message": "some message",
    };

    function cloneObject(object)
    {
        const clone = {};
        for (const [key, value] of Object.entries(object))
            clone[key] = value;
        return clone;
    }

    it('Changing the value in revision editor should update corresponding commitSet as long as the repository of that row does not have owner', async () => {
        const context = new BrowsingContext();
        const customizableTestGroupForm = await createCustomizableTestGroupFormWithContext(context);
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});

        const commitA = cloneObject(commitObjectA);
        const commitB = cloneObject(commitObjectB);
        commitA.repository = repository;
        commitB.repository = repository;
        const webkitCommitA = context.symbols.CommitLog.ensureSingleton(185326, commitA);
        const webkitCommitB = context.symbols.CommitLog.ensureSingleton(185334, commitB);
        const commitSetA = context.symbols.CommitSet.ensureSingleton(1, {revisionItems: [{commit: webkitCommitA}]});
        const commitSetB = context.symbols.CommitSet.ensureSingleton(2, {revisionItems: [{commit: webkitCommitB}]});

        customizableTestGroupForm.setCommitSetMap({A: commitSetA, B: commitSetB});
        customizableTestGroupForm.content('customize-link').click();

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(2);
        expect(requests[0].url).to.be('/api/commits/1/210948?prefix-match=true');
        expect(requests[1].url).to.be('/api/commits/1/210949?prefix-match=true');
        requests[0].resolve({commits: [commitObjectA]});
        requests[1].resolve({commits: [commitObjectB]});

        await requests[0].promise;
        await requests[1].promise;

        await waitForComponentsToRender(context);

        const radioButton = customizableTestGroupForm.content('custom-table').querySelector('input[type="radio"][name="A-1-radio"]:not(:checked)');
        radioButton.click();
        expect(radioButton.checked).to.be(true);

        let revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        expect(revisionEditors.length).to.be(2);
        let revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210949');

        await waitForComponentsToRender(context);
        revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        revisionEditor = revisionEditors[0];
        revisionEditor.value = '210948';
        revisionEditor.dispatchEvent(new Event('change'));

        customizableTestGroupForm.content('name').value = 'a/b test';
        customizableTestGroupForm.content('name').dispatchEvent(new Event('input'));

        await waitForComponentsToRender(context);

        // This is a hack by accessing the private member
        // customizableTestGroupForm will use commitSet.updateRevisionForOwnerRepository to update the revision, need to await this to see the input value gets updated to 210948
        await customizableTestGroupForm._commitSetMap.get('A')._fetchCommitLogAndOwnedCommits(repository, '210948');
        // It will enqueue to render, draw a new input with right revision
        await waitForComponentsToRender(context);

        revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210948');
    });

    it('should allow user to only provide prefix of a commit as long as the commit is unique in the repository', async () => {
        const context = new BrowsingContext();
        const customizableTestGroupForm = await createCustomizableTestGroupFormWithContext(context);
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});

        const commitA = cloneObject(commitObjectA);
        const commitB = cloneObject(commitObjectB);
        const commitC = cloneObject(commitObjectC);
        commitA.repository = repository;
        commitB.repository = repository;
        commitC.repository = repository;
        const webkitCommitA = context.symbols.CommitLog.ensureSingleton(185326, commitA);
        const webkitCommitB = context.symbols.CommitLog.ensureSingleton(185334, commitB);
        const commitSetA = context.symbols.CommitSet.ensureSingleton(1, {revisionItems: [{commit: webkitCommitA}]});
        const commitSetB = context.symbols.CommitSet.ensureSingleton(2, {revisionItems: [{commit: webkitCommitB}]});

        customizableTestGroupForm.setCommitSetMap({A: commitSetA, B: commitSetB});
        customizableTestGroupForm.content('customize-link').click();

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(2);
        expect(requests[0].url).to.be('/api/commits/1/210948?prefix-match=true');
        expect(requests[1].url).to.be('/api/commits/1/210949?prefix-match=true');
        requests[0].resolve({commits: [commitObjectA]});
        requests[1].resolve({commits: [commitObjectB]});

        await requests[0].promise;
        await requests[1].promise;

        await waitForComponentsToRender(context);

        const radioButton = customizableTestGroupForm.content('custom-table').querySelector('input[type="radio"][name="A-1-radio"]:not(:checked)');
        radioButton.click();
        expect(radioButton.checked).to.be(true);

        let revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        expect(revisionEditors.length).to.be(2);
        let revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210949');
        revisionEditor.value = '21095';
        revisionEditor.dispatchEvent(new Event('change'));

        customizableTestGroupForm.content('name').value = 'a/b test';
        customizableTestGroupForm.content('name').dispatchEvent(new Event('input'));
        expect(requests.length).to.be(3);
        expect(requests[2].url).to.be('/api/commits/1/21095?prefix-match=true');
        requests[2].resolve({commits: [commitObjectC]});

        await requests[2].promise;

        await waitForComponentsToRender(context);

        revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210950');
    });

    it('should use the commit set map when customize button is clicked as the behavior of radio buttons', async () => {
        const context = new BrowsingContext();
        const customizableTestGroupForm = await createCustomizableTestGroupFormWithContext(context);
        const repository = context.symbols.Repository.ensureSingleton(1, {name: 'WebKit'});

        const commitA = cloneObject(commitObjectA);
        const commitB = cloneObject(commitObjectB);
        commitA.repository = repository;
        commitB.repository = repository;
        const webkitCommitA = context.symbols.CommitLog.ensureSingleton(185326, commitA);
        const webkitCommitB = context.symbols.CommitLog.ensureSingleton(185334, commitB);
        const commitSetA = context.symbols.CommitSet.ensureSingleton(1, {revisionItems: [{commit: webkitCommitA}]});
        const commitSetB = context.symbols.CommitSet.ensureSingleton(2, {revisionItems: [{commit: webkitCommitB}]});

        customizableTestGroupForm.setCommitSetMap({A: commitSetA, B: commitSetB});
        customizableTestGroupForm.content('customize-link').click();

        const requests = context.symbols.MockRemoteAPI.requests;
        expect(requests.length).to.be(2);
        expect(requests[0].url).to.be('/api/commits/1/210948?prefix-match=true');
        expect(requests[1].url).to.be('/api/commits/1/210949?prefix-match=true');
        requests[0].resolve({commits: [commitObjectA]});
        requests[1].resolve({commits: [commitObjectB]});
        await requests[0].promise;
        await requests[1].promise;

        await waitForComponentsToRender(context);

        let revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        expect(revisionEditors.length).to.be(2);
        let revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210948');
        revisionEditor.value = '210949';
        revisionEditor.dispatchEvent(new Event('change'));

        customizableTestGroupForm.content('name').value = 'a/b test';
        customizableTestGroupForm.content('name').dispatchEvent(new Event('input'));

        await waitForComponentsToRender(context);

        let radioButton = customizableTestGroupForm.content('custom-table').querySelector('input[type="radio"][name="A-1-radio"]:not(:checked)');
        radioButton.click();
        expect(radioButton.checked).to.be(true);
        radioButton = customizableTestGroupForm.content('custom-table').querySelector('input[type="radio"][name="A-1-radio"]:not(:checked)');
        radioButton.click();
        expect(radioButton.checked).to.be(true);

        revisionEditors = customizableTestGroupForm.content('custom-table').querySelectorAll('input:not([type="radio"])');
        revisionEditor = revisionEditors[0];
        expect(revisionEditor.value).to.be('210948');
    });
});
