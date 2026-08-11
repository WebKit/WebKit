class ComboBox extends ComponentBase {

    constructor(candidates, maxCandidateListLength)
    {
        super('combo-box');
        this._candidates = candidates;
        this._maxCandidateListLength = maxCandidateListLength || 1000;
        this._candidateList = [];
        this._currentCandidateIndex = null;
        this._showCandidateList = false;
        this._renderCandidateListLazily = new LazilyEvaluatedFunction(this._renderCandidateList.bind(this));
        this._updateCandidateListLazily = new LazilyEvaluatedFunction(this._updateCandidateList.bind(this));
    }

    didConstructShadowTree()
    {
        super.didConstructShadowTree();
        const textField = this.content('text-field');
        textField.addEventListener('input', () => {
            this._showCandidateList = true;
            this._currentCandidateIndex = null;
            this.enqueueToRender();
        });
        textField.addEventListener('change', () => {
            this._autoCompleteIfOnlyOneMatchingItem();
            this._currentCandidateIndex = null;
        });
        textField.addEventListener('blur', () => {
            this._showCandidateList = false;
            this._autoCompleteIfOnlyOneMatchingItem();
            this.enqueueToRender();
        });
        textField.addEventListener('focus', () => {
            this._showCandidateList = true;
            this.enqueueToRender();
        });
        textField.addEventListener('keydown', (event) => {
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp')
                this._moveCandidate(event.key === 'ArrowDown');
            else if (event.key === 'Tab' || event.key === 'Enter') {
                let candidate = this._currentCandidateIndex === null ? null : this._candidateList[this._currentCandidateIndex];
                if (!candidate && this._candidateList.length === 1)
                    candidate = this._candidateList[0];
                if (candidate)
                    this.dispatchAction('update', candidate);
            }
        });
    }

    render()
    {
        super.render();

        const candidateElementList = this._renderCandidateListLazily.evaluate(this._candidates, this.content('text-field').value);

        console.assert(this._currentCandidateIndex === null || (this._currentCandidateIndex >= 0 && this._currentCandidateIndex < candidateElementList.length));
        const selectedCandidateElement = this._currentCandidateIndex === null ? null : candidateElementList[this._currentCandidateIndex];
        this._updateCandidateListLazily.evaluate(selectedCandidateElement, this._showCandidateList);
    }

    _autoCompleteIfOnlyOneMatchingItem()
    {
        const textFieldValueInLowerCase = this.content('text-field').value.toLowerCase();
        if (!textFieldValueInLowerCase.length)
            return;

        let matchingCandidateCount = 0;
        let matchingCandidate = null;
        for (const candidate of this._candidates) {
            if (candidate.toLowerCase().includes(textFieldValueInLowerCase)) {
                matchingCandidateCount += 1;
                matchingCandidate = candidate;
            }
            if (matchingCandidateCount > 1)
                break;
        }
        if (matchingCandidateCount === 1)
            this.dispatchAction('update', matchingCandidate);
    }

    _moveCandidate(forward)
    {
        const candidateListLength = this._candidateList.length;
        if (!candidateListLength)
            return;
        let newIndex = this._currentCandidateIndex;
        if (newIndex === null)
            newIndex = forward ? 0 : candidateListLength - 1;
        else {
            newIndex += forward ? 1 : -1;
            if (newIndex >= this._candidateList.length)
                newIndex = this._candidateList.length - 1;
            if (newIndex < 0)
                newIndex = 0;
        }
        this._currentCandidateIndex = newIndex;
        this.enqueueToRender();
    }

    _renderCandidateList(candidates, key)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        const candidatesStartingWithKey = [];
        const candidatesContainingKey = [];
        for (const candidate of candidates) {
            const searchResult = candidate.toLowerCase().indexOf(key.toLowerCase());
            if (key && searchResult < 0)
                continue;
            if (!searchResult)
                candidatesStartingWithKey.push(candidate);
            else
                candidatesContainingKey.push(candidate);
        }
        this._candidateList = candidatesStartingWithKey.concat(candidatesContainingKey).slice(0, this._maxCandidateListLength);
        const candidateElementList = this._candidateList.map((candidate) => {
            const item = link(candidate, () => null);
            // FIXME: We should use 'onlick' callback provided by 'ComponentBase.createLink'. However, in this case,
            // 'blur' event will be triggered before 'onlick', we have to use 'mousedown' instead.
            item.addEventListener('mousedown', () => {
                this.dispatchAction('update', candidate);
                this.enqueueToRender();
            });
            return element('li', item);
        });

        this.renderReplace(this.content('candidate-list'), candidateElementList);
        return candidateElementList;
    }

    _updateCandidateList(selectedCandidateElement, showCandidateList)
    {

        const candidateList = this.content('candidate-list');
        candidateList.style.display = showCandidateList ? null : 'none';

        const previouslySelectedCandidateElement = candidateList.querySelector('.selected');
        if (previouslySelectedCandidateElement)
            previouslySelectedCandidateElement.classList.remove('selected');

        if (selectedCandidateElement) {
            selectedCandidateElement.className = 'selected';
            selectedCandidateElement.scrollIntoViewIfNeeded();
        }
    }

    static htmlTemplate()
    {
        return `<div id='combox-box'>
            <input id='text-field'></input>
            <ul id="candidate-list"></ul>
        </div>`;
    }

    static cssTemplate()
    {
        return `
            div {
                position: relative;
                height: 1.4rem;
                left: 0;
            }

            ul:empty {
                display: none;
            }

            ul {
                transition: background 250ms ease-in;
                margin: 0;
                padding: 0.1rem 0.3rem;
                list-style: none;
                background: rgba(255, 255, 255, 0.95);
                border: solid 1px #ccc;
                top: 1.5rem;
                border-radius: 0.2rem;
                z-index: 10;
                position: absolute;
                min-width: 8.5rem;
                max-height: 10rem;
                overflow: auto;
            }

            li {
                text-align: left;
                margin: 0;
                padding: 0;
            }

            li:hover,
            li.selected {
                background: rgba(204, 153, 51, 0.1);
            }

            li a {
                text-decoration: none;
                font-size: 0.8rem;
                color: inherit;
                display: block;
            }
        `;
    }
}

ComponentBase.defineElement('combo-box', ComboBox);
