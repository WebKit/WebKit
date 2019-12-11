import {TestSuite} from '../Test.js';
import {REF, DOM, diff, FP, EventStream} from '../Ref.js';

class DiffTest extends TestSuite {
    testArrayDiff() {
        let removed = [];
        let newArray = [];
        diff([1, 2, 3], [4, 5, 6], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([1, 2, 3]);
        this.expect(newArray).equalToArray([4, 5, 6]);

        removed = [];
        newArray = [];
        diff([1, 2, 3], [2, 3, 4], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([1]);
        this.expect(newArray).equalToArray([2, 3, 4]);

        removed = [];
        newArray = [];
        diff([2], [2, 3, 4], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([]);
        this.expect(newArray).equalToArray([2, 3, 4]);

        removed = [];
        newArray = [];
        diff([], [2, 3, 4], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([]);
        this.expect(newArray).equalToArray([2, 3, 4]);
        
        removed = [];
        newArray = [];
        diff([4, 3, 2], [2, 3, 4], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([]);
        this.expect(newArray).equalToArray([2, 3, 4]);
        
        removed = [];
        newArray = [];
        diff([4, 3, 2, 5, 6], [2, 3, 4], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([5, 6]);
        this.expect(newArray).equalToArray([2, 3, 4]);
        
        removed = [];
        newArray = [];
        diff([], [], item => removed.push(item), item => newArray.push(item));
        this.expect(removed).equalToArray([]);
        this.expect(newArray).equalToArray([]);
    }
}

class DomTest extends TestSuite {
    setup(rootElement) {
        this.rootElement = rootElement;
    }

    testInject() {
        this.rootElement.innerHTML = "";
        const injector = `<div id="${Math.random()}"></div>`;
        DOM.inject(this.rootElement, injector);
        this.expect(this.rootElement.innerHTML).equalToValue(injector);
        this.rootElement.innerHTML = "";
    }

    testBefore() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        let injector = `<div id="${Math.random()}"></div>`;
        DOM.before(this.rootElement.children[0], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`${injector}${initial}`);
        this.rootElement.innerHTML = initial;
        DOM.before(this.rootElement.children[1], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`<div id="1"></div>${injector}<div id="2"></div><div id="3"></div>`);
        this.rootElement.innerHTML = initial;
        DOM.before(this.rootElement.children[2], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`<div id="1"></div><div id="2"></div>${injector}<div id="3"></div>`);
    }

    testAfter() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        let injector = `<div id="${Math.random()}"></div>`;
        DOM.after(this.rootElement.children[0], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`<div id="1"></div>${injector}<div id="2"></div><div id="3"></div>`);
        this.rootElement.innerHTML = initial;
        DOM.after(this.rootElement.children[1], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`<div id="1"></div><div id="2"></div>${injector}<div id="3"></div>`);
        this.rootElement.innerHTML = initial;
        DOM.after(this.rootElement.children[2], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`${initial}${injector}`);
    }

    testPrepend() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        let injector = `<div id="${Math.random()}"></div>`;
        DOM.prepend(this.rootElement, injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`${injector}${initial}`);
    }

    testAppend() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        let injector = `<div id="${Math.random()}"></div>`;
        DOM.append(this.rootElement, injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`${initial}${injector}`);
    }
    
    testReplace() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        let injector = `<div id="${Math.random()}"></div>`;
        DOM.replace(this.rootElement.children[0], injector);
        this.expect(this.rootElement.innerHTML).equalToValue(`${injector}<div id="2"></div><div id="3"></div>`);
    }
    
    testRemove() {
        let initial = '<div id="1"></div><div id="2"></div><div id="3"></div>'; 
        this.rootElement.innerHTML = initial;
        DOM.remove(this.rootElement.children[0]);
        this.expect(this.rootElement.innerHTML).equalToValue(`<div id="2"></div><div id="3"></div>`);
    }
}

class RefTest extends TestSuite {
    setup(rootElement) {
        this.rootElement = rootElement;
    }

    async testOnElementMount() {
        let triggered = false;
        let currentRef = null;
        const creatAComponent = () => {
            let ref = REF.createRef({
                onElementMount: (element) => {
                    triggered = true;
                }
            });
            currentRef = ref;
            return `<div ref="${ref}"></div>`;
        };
            
        const firstComp = creatAComponent();
        DOM.inject(this.rootElement, firstComp);
        await this.waitForRefMounted(currentRef);
        this.expect(triggered).equalToValue(true);
        this.expect(currentRef.element.outerHTML).equalToValue(firstComp);

        triggered = false;
        const secondComp = creatAComponent();
        DOM.replace(this.rootElement.children[0], secondComp);
        await this.waitForRefMounted(currentRef);
        this.expect(triggered).equalToValue(true);
        this.expect(currentRef.element.outerHTML).equalToValue(secondComp);
    }

    async testOnElementUnmount() {
        let triggered = false;
        let currentRef = null;
        const creatAComponent = () => {
            let ref = REF.createRef({
                onElementUnmount: (element) => {
                    triggered = true;
                }
            });
            currentRef = ref;
            return `<div ref="${ref}"></div>`;
        };

        const firstComp = creatAComponent();
        DOM.inject(this.rootElement, firstComp);
        DOM.replace(this.rootElement.children[0], "<div></div>");
        await this.waitForRefUnmounted(currentRef);
        this.expect(triggered).equalToValue(true);
        this.expect(currentRef.element.parentElement).equalToValue(null);

        triggered = false;
        DOM.inject(this.rootElement, firstComp);
        DOM.remove(this.rootElement.children[0]);
        let expectedE = null;
        try {
            await this.waitForRefUnmounted(currentRef);
        } catch (e) {
            // destoried ref won't be triggered
            this.expect(triggered).equalToValue(false);
            expectedE = e;
        }
        this.expect(expectedE).notEqualToValue(null);
        
        triggered = false;
        const secondComp = creatAComponent();
        DOM.inject(this.rootElement, secondComp);
        DOM.remove(this.rootElement.children[0]);
        await this.waitForRefUnmounted(currentRef);
        this.expect(triggered).equalToValue(true);
        this.expect(currentRef.element.parentElement).equalToValue(null);
    }
    
    async testOnComplexStateUpdate() {
        let verifier = null;
        let triggered = false;
        let expectedE = null;
        let initialState = {
            initialState: Math.random()
        };
        let ref = REF.createRef({
            onStateUpdate:(element, stateDiff, state) => {
                if (verifier) verifier(element, stateDiff, state);
            }
        });
        ref.setState(initialState);
        try {
            await this.waitForStateUpdated(ref);
        } catch (e) {
            expectedE = e;
        }
        // before element mount, we don't trigger state update callback, we just save the state
        this.expect(triggered).equalToValue(false);
        this.expect(expectedE).notEqualToValue(null);
        this.expect(ref.state.initialState).equalToValue(initialState.initialState);
        // Each time it will create a new state object
        this.expect(ref.state).notEqualToValue(initialState);
        
        DOM.inject(this.rootElement, `<div ref="${ref}"></div>`);
        verifier = (element, stateDiff, state) => {
            triggered = true;
            this.expect(stateDiff.initialState).equalToValue(initialState.initialState);
            this.expect(state.fakeState).equalToValue(undefined);
        };
        await this.waitForStateUpdated(ref);
        this.expect(triggered).equalToValue(true);
        
        triggered = false;
        let fakeState = {
            fakeState: Math.random()
        };
        verifier = (element, stateDiff, state) => {
            triggered = true;
            this.expect(stateDiff.fakeState).equalToValue(fakeState.fakeState);
            this.expect(state.fakeState).equalToValue(undefined);
            
            this.expect(state.initialState).equalToValue(initialState.initialState);
            this.expect(stateDiff.initialState).equalToValue(undefined);
        };
        ref.setState(fakeState);
        await this.waitForStateUpdated(ref);
        this.expect(ref.state.fakeState).equalToValue(fakeState.fakeState);
        this.expect(ref.state.initialState).equalToValue(initialState.initialState);
        this.expect(triggered).equalToValue(true);
    }
    
    async testOnValueStateUpdate() {
        let triggered = false;
        let verifier = null;
        const initialState = Math.random();
        const ref = REF.createRef({
            state: initialState,
            onStateUpdate:(element, stateDiff, state) => {
                if (verifier) verifier(element, stateDiff, state);
            }
        });
        verifier = (element, stateDiff, state) => {
            triggered = true;
            this.expect(element).notEqualToValue(null);
            this.expect(stateDiff).equalToValue(initialState);
        }
        DOM.inject(this.rootElement, `<div ref="${ref}"></div>`);
        await this.waitForStateUpdated(ref);
        this.expect(triggered).equalToValue(true);

        const newState = null;
        triggered = false;
        verifier = (element, stateDiff, state) => {
            triggered = true;
            this.expect(stateDiff).equalToValue(newState);
            this.expect(state).equalToValue(initialState);
        }
        ref.setState(newState);
        await this.waitForStateUpdated(ref);
        this.expect(triggered).equalToValue(true);
        
        const newState1 = 0;
        triggered = false;
        verifier = (element, stateDiff, state) => {
            triggered = true;
            this.expect(stateDiff).equalToValue(newState1);
            this.expect(state).equalToValue(newState);
        }
        ref.setState(newState1);
        await this.waitForStateUpdated(ref);
        this.expect(triggered).equalToValue(true);
        
        const newState2 = undefined;
        let expectedE = null;
        ref.setState(newState1);
        try {
            await this.waitForStateUpdated(ref);
        } catch(e) {
            expectedE = e;
        }
        this.expect(expectedE).notEqualToValue(null);
    }
}
export {DiffTest, DomTest, RefTest};
