// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function uuidv4() {
    return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
        (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
    )
}

const TRACE_STATE = false;

class Signal {
    constructor(handlers) {
        this.handlers = handlers ? handlers : [];
        this.maxinumRunTime = 1 / 30 * 1000;
    }
    addListener(fn) {
        if (!fn) return this;
        this.handlers.push(fn);
        return this;
    }
    removeListener(fn) {
        let removeIndex = 0;
        this.handlers.forEach((handler, index) => {
            if (handler === fn)
                removeIndex = index;
        });
        this.handlers.splice(removeIndex, 1);
        return this;
    }
    removeAllListener() {
        this.handlers = [];
        return this;
    }
    emit(...args) {
        if (this.handlers) {
            this.requestEmit(0, ...args);
        }
        return this;
    }
    requestEmit(from, ...args) {
        return new Promise(resolve => {
            setTimeout(() => {
                const startedTime = new Date().getTime();
                // Run as many handlers as posible
                let i = from;
                for (; i < this.handlers.length; i++) {
                    this.handlers[i](...args);
                    const endTime = new Date().getTime();
                    if (endTime - startedTime >= this.maxinumRunTime) {
                        this.requestEmit(i+1, ...args).then(() => resolve());
                        break;
                    }
                }
                if (i === this.handlers.length) {
                    resolve();
                }
            });
        });
    }
}

class EventStream {
    constructor(signal) {
        this.signal = signal ? signal : new Signal();
        this.errorSignal = new Signal();
        this.stopSignal = new Signal();
        this.lastValue = null;
    }
    action(fn) {
        this.signal.addListener(fn);
        return this;
    }
    stopAction(fn) {
        this.signal.removeListener(fn);
        return this;
    }
    error(fn) {
        this.errorSignal.addListener(fn);
        return this;
    }
    stop() {
        this.signal.removeAllListener();
        this.errorSignal.removeAllListener();
        this.stopSignal.emit();
        return this;
    }
    onStop(fn) {
        this.stopSignal.addListener(fn);
        return this;
    }
    add(...args) {
        this.lastValue = args;
        this.signal.emit(...args);
        return this;
    }
    getLastValue() {
        return this.lastValue;
    }
    replayLast() {
        const lastValue = this.getLastValue();
        if (lastValue)
            this.add(...lastValue);
    }
    addPromise(p) {
        let that = this;
        p.then((...args) => {
            that.add(...args);
        }).catch((...args) => {
            that.addError(...args);
        });
    }
    addError(...args) {
        this.errorSignal.emit(...args);
    }
    addWithDelay(delay, ...args) {
        setTimeout(() => {
            this.add(...args);
        }, delay);
        return this;
    }
    filter(filterFn) {
        const res = new EventStream();
        this.action((...args) => {
            if (filterFn(...args))
                res.add(...args);
        });
        return res;
    }
    remains(filterFn) {
        const res = new EventStream();
        this.action((...args) => {
            if (!filterFn(...args))
                res.add(...args);
        });
        return res;
    }
    map(fn) {
        const res = new EventStream();
        this.action((...args) => {
            res.add(fn(...args));
        });
        return res;
    }
    scan(fn) {
        const res = new EventStream();
        let accumulator = null;
        this.action((...args) => {
            accumulator = fn(accumulator, ...args);
            res.add(accumulator);
        });
        return res;
    }
    flatMap(fn) {
        const res = new EventStream();
        this.action((...args) => {
            fn(res)(...args);
        });
        return res;
    }
    inject(es, ...args) {
        return this.action(() => {
            es.add(...args);
        });
    }
    static merge(...args) {
        const res = new EventStream();
        Array.prototype.forEach.call(args, ((stream) => {
            stream.action(() => {
                res.add(...args);
            });
        }));
        return res;
    }
    static fromMonitor(fn, interval, initial) {
        const res = new EventStream();
        let handler = null;
        let result = initial;
        res.onStop(() => {
            clearTimeout(handler);
        });
        const poll = () => {
            result = fn(result);
            if (result instanceof Promise)
                res.addPromise(result);
            else
                res.add(result);
            handler = setTimeout(poll, interval);
        }
        poll();
        return res;
    }
}

function isMergeableState(state) {
    return Object.prototype.toString.call(state) === "[object Object]";
}

function applyStateDiff(state, diff) {
    if (diff === undefined)
        return state;
    if (!isMergeableState(diff))
        return diff;
    const res = {};
    if (state) {
        Object.keys(state).forEach((key) => {
            res[key] = state[key];
        });
    } else {
        return state;
    }
    if (diff) {
        Object.keys(diff).forEach((key) => {
            res[key] = diff[key];
        });
    }
    return res;
}

class Ref {
    constructor(config) {
        this.key = uuidv4();
        this.element = null;
        this.lastStateDiff = null;
        config = config ? config : {};
        this.stateTracer = config.state !== undefined ? [config.state] : [];
        this.state = config.state !== undefined ? config.state : {};
        this.onStateUpdate = new Signal().addListener(config.onStateUpdate);
        this.onElementMount = new Signal().addListener(config.onElementMount);
        this.onElementUnmount = new Signal().addListener(config.onElementUnmount);
    }
    bind(element) {
        this.apply(element.querySelector(`[ref="${this.key}"]`));
    }

    apply(element) {
        if (!element)
            return;
        if (this.element === element)
            return;
        if (this.element && this.element !== element)
            this.onElementUnmount.emit(this.element);
        this.element = element;
        //Initial state
        if (this.state !== undefined && this.state !== null)
            this.onStateUpdate.emit(this.element, this.state, this.state);
        this.onElementMount.emit(this.element);
    }
    destory(element) {
        if (!element || element !== this.element)
            return;
        this.onElementUnmount.emit(this.element);
    }
    toString() {
        return this.key;
    }
    setState(stateDiff) {
        if (stateDiff === undefined)
            return;
        if (TRACE_STATE)
            this.stateTracer.push(stateDiff);
        else
            this.stateTracer = [stateDiff];
        this.lastStateDiff = stateDiff;
        //If element haven't been mount, we just update the state, so that we will only render the finial stat when element mount
        if (this.element)
            this.onStateUpdate.emit(this.element, stateDiff, this.state);
        this.state = applyStateDiff(this.state, stateDiff);
    }
    fromEvent(event, ...args) {
        const res = new EventStream();
        this.onElementMount.addListener((element) => element.addEventListener(event, (e) => {
            res.add(e, ...args);
        }));
        return res;
    }
}

class RefManger {
    constructor() {
        this.refs = {};
    }
    createRef(config) {
        const ref = new Ref(config);
        this.refs[ref.key] = ref;
        return ref;
    }
    bindAll(dom) {
        const nodeIterator = document.createNodeIterator(
            dom,
            NodeFilter.SHOW_ELEMENT,
            (node) => {
                    const refKey = node.getAttribute('ref');
                    const ref = this.refs[refKey];
                    if (ref)
                        ref.apply.call(ref,node);
                    return ref ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
            }
        );
        let currentNode = null;
        while (currentNode = nodeIterator.nextNode());
    }
    unbindAll(dom) {
        const nodeIterator = document.createNodeIterator(
            dom,
            NodeFilter.SHOW_ELEMENT,
            (node) => {
                    const refKey = node.getAttribute('ref');
                    const ref = this.refs[refKey];
                    if (ref) {
                        ref.destory.call(ref,node);
                        delete this.refs[refKey];
                    }
                    return ref ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
            }
        );
        let currentNode = null;
        while (currentNode = nodeIterator.nextNode());
    }
}

const REF = new RefManger();

function createDomElementsFromStr(string) {
    const fakeElement = document.createElement('div');
    fakeElement.innerHTML = string;
    const elements = [];
    Array.prototype.forEach.call(fakeElement.children, (element) => {
        element.remove();
        elements.push(element);
    });
    return elements;
}

const DOM = {
    inject: (element, string) => {
        REF.unbindAll(element);
        element.innerHTML = string;
        REF.bindAll(element);
    },
    prepend: (element, string) => {
        if (element.children.length === 0) {
            DOM.inject(element, string);
            return;
        }
        DOM.before(element.firstElementChild, string);
    },
    append: (element, string) => {
        if (element.children.length === 0) {
            DOM.inject(element, string);
            return;
        }
        DOM.after(element.lastElementChild, string);
    },
    before: (element, string) => {
        const elements = createDomElementsFromStr(string);
        element.before(...elements);
        elements.forEach((elm) => {
            REF.bindAll(elm);
        });
    },
    after: (element, string) => {
        const elements = createDomElementsFromStr(string);
        element.after(...elements);
        elements.forEach((elm) => {
            REF.bindAll(elm);
        });
    },
    replace: (element, string) => {
        DOM.before(element, string);
        REF.unbindAll(element);
        element.remove();
    },
    remove: (element) => {
        REF.unbindAll(element);
        element.remove();
    },
};
function diff(oldValue, newValue, removeCallback, addCallback, getKey = item => item.toString()) {
    if (!newValue) return;
    if (oldValue === newValue) return;
    if (Array.isArray(oldValue) && Array.isArray(newValue)) {
        const newArrayMap = new Map();
        newValue.forEach((item, index) => {
             newArrayMap.set(getKey(item), {
                 item: item,
                 index: index,
                 oldIndex: -1
             });
        });
        oldValue.forEach((oldItem, index) => {
            const oldItemKey = getKey(oldItem);
            if (!newArrayMap.has(oldItemKey))
                removeCallback(oldItem, index);
            else {
                const newEntry = newArrayMap.get(oldItemKey);
                if (newEntry) {
                    newEntry.oldIndex = index;
                    newEntry.item = oldItem;
                }
            }
        });
        for (let entry of newArrayMap.entries()) {
            addCallback(entry[1].item, entry[1].index, entry[1].oldIndex);
        }
    }
}

const FP = {
    composer: (fn) => {
        // Input function must be a currying function
        // Composer will compose that currying function call
        // Making a currying function fn = (a) => (b) => {....} can be called
        // composer = FP.composer(fn);
        // composer(a); composer(b);
        // instead of calling it fn(a)(b);
        console.assert(typeof fn === "function" && fn.length === 1);
        let result = fn;
        return (...args) => {
            args.forEach(arg => {
                result = result(arg);
            });
        }
    },
    currying: (fn) => {
        // Currying input function
        // (a, b) => {...}
        // to
        // (a) => (b) => {...}
        const args = [];
        const returnFn = (x) => {
            args.push(x);
            if (args.length === fn.length)
                return fn(...args);
            else
                return returnFn;
        }
        return returnFn;
    }
}
export {REF, DOM, diff, FP, EventStream, uuidv4};
