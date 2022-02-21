
describe('TestGroupFormTests', () => {
    const scripts = ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js',
        'components/test-group-form.js', 'components/repetition-type-selection.js', 'models/data-model.js',
        'models/triggerable.js', 'models/test.js', 'models/platform.js', 'lazily-evaluated-function.js'];

    async function createTestGroupFormWithContext(context)
    {
        await context.importScripts(scripts, 'ComponentBase', 'LabeledObject', 'DataModelObject', 'Test',
            'TestGroupForm', 'RepetitionTypeSelection', 'TriggerableConfiguration','Platform', 'LazilyEvaluatedFunction');
        const platform = new context.symbols.Platform(5, {name: 'SomePlatform', metrics: []});
        const test = new context.symbols.Test(10, {name: 'SomeTest'});
        new context.symbols.TriggerableConfiguration(`${test.id()}-${platform.id()}`,
            {platform, test, supportedRepetitionTypes: ['alternating', 'sequential']});
        const testGroupForm = new context.symbols.TestGroupForm;
        testGroupForm.setTestAndPlatform(test, platform);
        context.document.body.appendChild(testGroupForm.element());
        return testGroupForm;
    }

    it('must dispatch "startTesting" action with the number of repetitions the user clicks on "Start A/B testing"', () => {
        const context = new BrowsingContext();
        return createTestGroupFormWithContext(context).then((testGroupForm) => {
            const calls = [];
            testGroupForm.listenToAction('startTesting', (...args) => calls.push(args));
            expect(calls).to.eql({});
            testGroupForm.content('start-button').click();
            expect(calls).to.eql([[4, 'alternating', true]]);
        });
    });

    it('must dispatch "startTesting" action with the repetition type user selects on "Start A/B testing"', async () => {
        const context = new BrowsingContext();
        const testGroupForm = await createTestGroupFormWithContext(context);
        const calls = [];
        testGroupForm.listenToAction('startTesting', (...args) => calls.push(args));
        expect(calls).to.eql({});
        testGroupForm.content('start-button').click();
        expect(calls).to.eql([[4, 'alternating', true]]);
        testGroupForm.part('repetition-type-selector').selectedRepetitionType = 'sequential';
        testGroupForm.content('start-button').click();
        expect(calls).to.eql([[4, 'alternating', true], [4, 'sequential', true]]);
    });

    it('must update the repetition count when the user selected a different count', () => {
        const context = new BrowsingContext();
        return createTestGroupFormWithContext(context).then((testGroupForm) => {
            const calls = [];
            testGroupForm.listenToAction('startTesting', (...args) => calls.push(args));
            expect(calls).to.eql({});
            testGroupForm.content('start-button').click();
            expect(calls).to.eql([[4, 'alternating', true]]);
            const countForm = testGroupForm.content('repetition-count');
            countForm.value = '6';
            countForm.dispatchEvent(new Event('change'));
            testGroupForm.content('start-button').click();
            expect(calls).to.eql([[4, 'alternating', true], [6, 'alternating', true]]);
        });
    });

    it('must update "notify on completion" when it is unchecked', () => {
        const context = new BrowsingContext();
        return createTestGroupFormWithContext(context).then((testGroupForm) => {
            const calls = [];
            testGroupForm.listenToAction('startTesting', (...args) => calls.push(args));
            expect(calls).to.eql({});
            testGroupForm.content('start-button').click();
            expect(calls).to.eql([[4, 'alternating', true]]);
            const countForm = testGroupForm.content('repetition-count');
            countForm.value = '6';
            countForm.dispatchEvent(new Event('change'));
            const notifyOnCompletionCheckbox = testGroupForm.content('notify-on-completion-checkbox');
            notifyOnCompletionCheckbox.checked = false;
            notifyOnCompletionCheckbox.dispatchEvent(new Event('change'));
            testGroupForm.content('start-button').click();
            expect(calls).to.eql([[4, 'alternating', true], [6, 'alternating', false]]);
        });
    });

    describe('setRepetitionCount', () => {
        it('must update the visible repetition count', () => {
            const context = new BrowsingContext();
            return createTestGroupFormWithContext(context).then((testGroupForm) => {
                expect(testGroupForm.content('repetition-count').value).to.be('4');
                testGroupForm.setRepetitionCount(2);
                return waitForComponentsToRender(context).then(() => {
                    expect(testGroupForm.content('repetition-count').value).to.be('2');
                });
            });
        });

        it('must update the repetition count passed to "startTesting" action', () => {
            const context = new BrowsingContext();
            return createTestGroupFormWithContext(context).then((testGroupForm) => {
                const calls = [];
                testGroupForm.listenToAction('startTesting', (...args) => calls.push(args));
                expect(calls).to.eql({});
                testGroupForm.content().querySelector('button').click();
                expect(calls).to.eql([[4, 'alternating', true]]);
                testGroupForm.setRepetitionCount(8);
                testGroupForm.content().querySelector('button').click();
                expect(calls).to.eql([[4, 'alternating', true], [8, 'alternating', true]]);
            });
        });
    });
});
