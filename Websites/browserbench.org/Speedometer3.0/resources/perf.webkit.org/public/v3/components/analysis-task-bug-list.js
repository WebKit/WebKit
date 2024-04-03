
class AnalysisTaskBugList extends ComponentBase {

    constructor()
    {
        super('analysis-task-bug-list');
        this._task = null;
    }

    setTask(task)
    {
        console.assert(task == null || task instanceof AnalysisTask);
        this._task = task;
        this.enqueueToRender();
    }

    didConstructShadowTree()
    {
        this.part('bug-list').setKindList(BugTracker.all());
        this.part('bug-list').listenToAction('addItem', (tracker, bugNumber) => this._associateBug(tracker, bugNumber));
    }

    render()
    {
        const bugList = this._task ? this._task.bugs().map((bug) => {
            return new MutableListItem(bug.bugTracker(), bug.label(), bug.title(), bug.url(),
                'Dissociate this bug', () => this._dissociateBug(bug));
        }) : [];
        this.part('bug-list').setList(bugList);
    }

    _associateBug(tracker, bugNumber)
    {
        console.assert(tracker instanceof BugTracker);
        bugNumber = parseInt(bugNumber);

        return this._task.associateBug(tracker, bugNumber).then(() => this.enqueueToRender(), (error) => {
            this.enqueueToRender();
            alert('Failed to associate the bug: ' + error);
        });
    }

    _dissociateBug(bug)
    {
        return this._task.dissociateBug(bug).then(() => this.enqueueToRender(), (error) => {
            this.enqueueToRender();
            alert('Failed to dissociate the bug: ' + error);
        });
    }

    static htmlTemplate() { return `<mutable-list-view id="bug-list"></mutable-list-view>`; }

}

ComponentBase.defineElement('analysis-task-bug-list', AnalysisTaskBugList);
