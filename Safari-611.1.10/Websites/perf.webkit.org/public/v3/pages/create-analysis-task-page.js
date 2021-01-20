
class CreateAnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Create Analysis Task');
        this._message = null;
        this._renderMessageLazily = new LazilyEvaluatedFunction(this._renderMessage.bind(this));
    }

    title() { return 'Creating a New Analysis Task'; }
    routeName() { return 'analysis/task/create'; }

    updateFromSerializedState(state, isOpen)
    {
        if (state.error instanceof Set)
            state.error = Array.from(state.error.values())[0];
        this._message = state.error || (state.inProgress ? 'Creating the analysis task page...' : '');
    }

    didConstructShadowTree()
    {
        this.part('form').listenToAction('startTesting', this._createAnalysisTaskWithGroup.bind(this));
    }

    _createAnalysisTaskWithGroup(repetitionCount, testGroupName, commitSets, platform, test, taskName, notifyOnCompletion)
    {
        TestGroup.createWithTask(taskName, platform, test, testGroupName, repetitionCount, commitSets, notifyOnCompletion).then((task) => {
            const url = this.router().url(`analysis/task/${task.id()}`);
            location.href = this.router().url(`analysis/task/${task.id()}`);
        }, (error) => {
            alert('Failed to create a new test group: ' + error);
        });
    }

    render()
    {
        super.render();
        this._renderMessageLazily.evaluate(this._message);
    }

    _renderMessage(message)
    {
        const messageContainer = this.content('message');
        messageContainer.textContent = this._message;
        messageContainer.style.display = this._message ? null : 'none';
        this.content('form').style.display = this._message ? 'none' : null;

      }

    static htmlTemplate()
    {
        return `
            <p id="message"></p>
            <custom-configuration-test-group-form id="form"></custom-configuration-test-group-form>`;
    }

    static cssTemplate()
    {
        return `
            #message {
                text-align: center;
            }
            #form {
                margin: 1rem;
            }
`;
    }
}
