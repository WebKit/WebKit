//@ defaultQuickRun

// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Copyright 2014 Apple Inc.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// This is a JavaScript implementation of the Richards
// benchmark from:
//
//    http://www.cl.cam.ac.uk/~mr10/Bench.html
//
// The benchmark was originally implemented in BCPL by
// Martin Richards. It was then ported to JavaScript by the
// V8 project authors, and then subsequently it was modified
// to use getters and setters by WebKit authors.

let __exceptionCounter = 0;
function randomException() {
    __exceptionCounter++;
    if (__exceptionCounter % 2500 === 0)
        throw new Error("blah");
}
noInline(randomException);

/**
 * The Richards benchmark simulates the task dispatcher of an
 * operating system.
 **/
function runRichards() {
    var scheduler = new Scheduler();
    scheduler.addIdleTask(ID_IDLE, 0, null, COUNT);

    var queue = new Packet(null, ID_WORKER, KIND_WORK);
    queue = new Packet(queue,  ID_WORKER, KIND_WORK);
    scheduler.addWorkerTask(ID_WORKER, 1000, queue);

    queue = new Packet(null, ID_DEVICE_A, KIND_DEVICE);
    queue = new Packet(queue,  ID_DEVICE_A, KIND_DEVICE);
    queue = new Packet(queue,  ID_DEVICE_A, KIND_DEVICE);
    scheduler.addHandlerTask(ID_HANDLER_A, 2000, queue);

    queue = new Packet(null, ID_DEVICE_B, KIND_DEVICE);
    queue = new Packet(queue,  ID_DEVICE_B, KIND_DEVICE);
    queue = new Packet(queue,  ID_DEVICE_B, KIND_DEVICE);
    scheduler.addHandlerTask(ID_HANDLER_B, 3000, queue);

    scheduler.addDeviceTask(ID_DEVICE_A, 4000, null);

    scheduler.addDeviceTask(ID_DEVICE_B, 5000, null);

    scheduler.schedule();
    let queueCount;
    try {
        queueCount = scheduler.queueCount;
    } catch(e) {
        queueCount = scheduler.queueCount;
    }
    let holdCount;
    try {
        holdCount = scheduler.holdCount;
    } catch(e) { 
        holdCount = scheduler.holdCount;
    }

    if (queueCount != EXPECTED_QUEUE_COUNT ||
        holdCount != EXPECTED_HOLD_COUNT) {
        var msg =
            "Error during execution: queueCount = " + queueCount +
            ", holdCount = " + holdCount + ".";
        throw new Error(msg);
    }
}

var COUNT = 1000;

/**
 * These two constants specify how many times a packet is queued and
 * how many times a task is put on hold in a correct run of richards.
 * They don't have any meaning a such but are characteristic of a
 * correct run so if the actual queue or hold count is different from
 * the expected there must be a bug in the implementation.
 **/
var EXPECTED_QUEUE_COUNT = 2322;
var EXPECTED_HOLD_COUNT = 928;


/**
 * A scheduler can be used to schedule a set of tasks based on their relative
 * priorities.  Scheduling is done by maintaining a list of task control blocks
 * which holds tasks and the data queue they are processing.
 * @constructor
 */
function Scheduler() {
    this._queueCount = 0;
    this._holdCount = 0;
    this._blocks = new Array(NUMBER_OF_IDS);
    this._list = null;
    this._currentTcb = null;
    this._currentId = null;
}

var ID_IDLE       = 0;
var ID_WORKER     = 1;
var ID_HANDLER_A  = 2;
var ID_HANDLER_B  = 3;
var ID_DEVICE_A   = 4;
var ID_DEVICE_B   = 5;
var NUMBER_OF_IDS = 6;

var KIND_DEVICE   = 0;
var KIND_WORK     = 1;

Scheduler.prototype.__defineGetter__("queueCount", function() { randomException(); return this._queueCount; });
Scheduler.prototype.__defineSetter__("queueCount", function(value) { this._queueCount = value; });
Scheduler.prototype.__defineGetter__("holdCount", function() { randomException(); return this._holdCount; });
Scheduler.prototype.__defineSetter__("holdCount", function(value) { this._holdCount = value; });
Scheduler.prototype.__defineGetter__("blocks", function() { randomException(); return this._blocks; });
Scheduler.prototype.__defineSetter__("blocks", function(value) { this._blocks = value; });
Scheduler.prototype.__defineGetter__("list", function() { randomException(); return this._list; });
Scheduler.prototype.__defineSetter__("list", function(value) { this._list = value; });
Scheduler.prototype.__defineGetter__("currentTcb", function() { randomException(); return this._currentTcb; });
Scheduler.prototype.__defineSetter__("currentTcb", function(value) { this._currentTcb = value; });
Scheduler.prototype.__defineGetter__("currentId", function() { randomException(); return this._currentId; });
Scheduler.prototype.__defineSetter__("currentId", function(value) { this._currentId = value; });

/**
 * Add an idle task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {int} count the number of times to schedule the task
 */
Scheduler.prototype.addIdleTask = function (id, priority, queue, count) {
    this.addRunningTask(id, priority, queue, new IdleTask(this, 1, count));
};

/**
 * Add a work task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addWorkerTask = function (id, priority, queue) {
    this.addTask(id, priority, queue, new WorkerTask(this, ID_HANDLER_A, 0));
};

/**
 * Add a handler task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addHandlerTask = function (id, priority, queue) {
    this.addTask(id, priority, queue, new HandlerTask(this));
};

/**
 * Add a handler task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addDeviceTask = function (id, priority, queue) {
    this.addTask(id, priority, queue, new DeviceTask(this))
};

/**
 * Add the specified task and mark it as running.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {Task} task the task to add
 */
Scheduler.prototype.addRunningTask = function (id, priority, queue, task) {
    this.addTask(id, priority, queue, task);
    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    currentTcb.setRunning();
};

/**
 * Add the specified task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {Task} task the task to add
 */
Scheduler.prototype.addTask = function (id, priority, queue, task) {
    let list;
    try {
        list = this.list;
    } catch(e) {
        list = this.list;
    }

    this.currentTcb = new TaskControlBlock(list, id, priority, queue, task);
    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    this.list = currentTcb;
    let blocks;
    try {
        blocks = this.blocks;
    } catch(e) {
        blocks = this.blocks;
    }

    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    blocks[id] = currentTcb;
};

/**
 * Execute the tasks managed by this scheduler.
 */
Scheduler.prototype.schedule = function () {
    let list;
    try {
        list = this.list;
    } catch(e) {
        list = this.list;
    }
    this.currentTcb = list;
    while (true) {
        let currentTcb;

        try {
            currentTcb = this.currentTcb;
        } catch(e) {
            currentTcb = this.currentTcb;
        }

        if (currentTcb == null)
            break;

        try {
            currentTcb = this.currentTcb;
        } catch(e) {
            currentTcb = this.currentTcb;
        }

        if (currentTcb.isHeldOrSuspended()) {
            let link;
            try {
                link = currentTcb.link;
            } catch(e) {
                link = currentTcb.link;
            }
            this.currentTcb = link;
        } else {
            let id;
            try {
                id = currentTcb.id;
            } catch(e) {
                id = currentTcb.id;
            }
            this.currentId = id;
            this.currentTcb = currentTcb.run();
        }
    }
};

/**
 * Release a task that is currently blocked and return the next block to run.
 * @param {int} id the id of the task to suspend
 */
Scheduler.prototype.release = function (id) {
    let blocks;
    try {
        blocks = this.blocks;
    } catch(e) {
        blocks = this.blocks;
    }
    var tcb = blocks[id];
    if (tcb == null) return tcb;
    tcb.markAsNotHeld();

    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }

    if (tcb.priority > currentTcb.priority) {
        return tcb;
    } else {
        let currentTcb;
        try {
            currentTcb = this.currentTcb;
        } catch(e) {
            currentTcb = this.currentTcb;
        }
        return currentTcb;
    }
};

/**
 * Block the currently executing task and return the next task control block
 * to run.  The blocked task will not be made runnable until it is explicitly
 * released, even if new work is added to it.
 */
Scheduler.prototype.holdCurrent = function () {
    let holdCount;
    try {
        holdCount = this.holdCount;
    } catch(e) {
        holdCount = this.holdCount;
    }
    holdCount++;
    this.holdCount = holdCount;
    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    currentTcb.markAsHeld();

    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }

    let link;
    try {
        link = currentTcb.link;
    } catch(e) {
        link = currentTcb.link;
    }
    return link;
};

/**
 * Suspend the currently executing task and return the next task control block
 * to run.  If new work is added to the suspended task it will be made runnable.
 */
Scheduler.prototype.suspendCurrent = function () {
    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    currentTcb.markAsSuspended();
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    return currentTcb;
};

/**
 * Add the specified packet to the end of the worklist used by the task
 * associated with the packet and make the task runnable if it is currently
 * suspended.
 * @param {Packet} packet the packet to add
 */
Scheduler.prototype.queue = function (packet) {
    let blocks;
    try {
        blocks = this.blocks;
    } catch(e) {
        blocks = this.blocks;
    }
    let id;
    try {
        id = packet.id;
    } catch(e) {
        id = packet.id;
    }
    var t = blocks[id];
    if (t == null) return t;
    let queueCount;
    try {
        queueCount = this.queueCount;
    } catch(e) {
        queueCount = this.queueCount;
    }
    queueCount++;
    this.queueCount = queueCount;
    try {
        packet.link = null;
    } catch(e) { }
    let currentId;
    try {
        currentId = this.currentId;
    } catch(e) {
        currentId = this.currentId;
    }
    try {
        packet.id = currentId;
    } catch(e) { }
    let currentTcb;
    try {
        currentTcb = this.currentTcb;
    } catch(e) {
        currentTcb = this.currentTcb;
    }
    return t.checkPriorityAdd(currentTcb, packet);
};

/**
 * A task control block manages a task and the queue of work packages associated
 * with it.
 * @param {TaskControlBlock} link the preceding block in the linked block list
 * @param {int} id the id of this block
 * @param {int} priority the priority of this block
 * @param {Packet} queue the queue of packages to be processed by the task
 * @param {Task} task the task
 * @constructor
 */
function TaskControlBlock(link, id, priority, queue, task) {
    this._link = link;
    this._id = id;
    this._priority = priority;
    this._queue = queue;
    this._task = task;
    if (queue == null) {
        try {
            this.state = STATE_SUSPENDED;
        } catch(e) { }
    } else {
        try {
            this.state = STATE_SUSPENDED_RUNNABLE;
        } catch(e) { }
    }
}

/**
 * The task is running and is currently scheduled.
 */
var STATE_RUNNING = 0;

/**
 * The task has packets left to process.
 */
var STATE_RUNNABLE = 1;

/**
 * The task is not currently running.  The task is not blocked as such and may
 * be started by the scheduler.
 */
var STATE_SUSPENDED = 2;

/**
 * The task is blocked and cannot be run until it is explicitly released.
 */
var STATE_HELD = 4;

var STATE_SUSPENDED_RUNNABLE = STATE_SUSPENDED | STATE_RUNNABLE;
var STATE_NOT_HELD = ~STATE_HELD;

TaskControlBlock.prototype.__defineGetter__("link", function() { randomException(); return this._link; });
TaskControlBlock.prototype.__defineGetter__("id", function() { randomException(); return this._id; });
TaskControlBlock.prototype.__defineGetter__("priority", function() { return this._priority; });
TaskControlBlock.prototype.__defineGetter__("queue", function() { randomException(); return this._queue; });
TaskControlBlock.prototype.__defineSetter__("queue", function(value) { this._queue = value; });
TaskControlBlock.prototype.__defineGetter__("task", function() { return this._task; });
TaskControlBlock.prototype.__defineGetter__("state", function() { randomException(); return this._state; });
TaskControlBlock.prototype.__defineSetter__("state", function(value) { this._state = value; randomException(); });

TaskControlBlock.prototype.setRunning = function () {
    try {
        this.state = STATE_RUNNING;
    } catch(e) { }
};

TaskControlBlock.prototype.markAsNotHeld = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    try {
        this.state = state & STATE_NOT_HELD;
    } catch(e){}
};

TaskControlBlock.prototype.markAsHeld = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    try {
        this.state = state | STATE_HELD;
    } catch(e) {}
};

TaskControlBlock.prototype.isHeldOrSuspended = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    return (state & STATE_HELD) != 0 || (state == STATE_SUSPENDED);
};

TaskControlBlock.prototype.markAsSuspended = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    try {
        this.state = state | STATE_SUSPENDED;
    } catch(e) { }
};

TaskControlBlock.prototype.markAsRunnable = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    try {
        this.state = state | STATE_RUNNABLE;
    } catch(e) { }
};

/**
 * Runs this task, if it is ready to be run, and returns the next task to run.
 */
TaskControlBlock.prototype.run = function () {
    var packet;
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    if (state == STATE_SUSPENDED_RUNNABLE) {
        let queue;
        try {
            queue = this.queue;
        } catch(e) {
            queue = this.queue;
        }
        packet = queue;
        let link;
        try {
            link = packet.link;
        } catch(e) {
            link = packet.link;
        }
        this.queue = link;
        try {
            queue = this.queue;
        } catch(e) {
            queue = this.queue;
        }
        if (queue == null) {
            try {
                this.state = STATE_RUNNING;
            } catch(e) { }
        } else {
            try {
                this.state = STATE_RUNNABLE;
            } catch(e) { }
        }
    } else {
        packet = null;
    }
    return this.task.run(packet);
};

/**
 * Adds a packet to the worklist of this block's task, marks this as runnable if
 * necessary, and returns the next runnable object to run (the one
 * with the highest priority).
 */
TaskControlBlock.prototype.checkPriorityAdd = function (task, packet) {
    let queue;
    try {
        queue = this.queue;
    } catch(e) {
        queue = this.queue;
    }
    if (queue == null) {
        this.queue = packet;
        this.markAsRunnable();
        if (this.priority > task.priority) return this;
    } else {
        let queue;
        try {
            queue = this.queue;
        } catch(e) {
            queue = this.queue;
        }
        this.queue = packet.addTo(queue);
    }
    return task;
};

TaskControlBlock.prototype.toString = function () {
    let state;
    try {
        state = this.state;
    } catch(e) {
        state = this.state;
    }
    return "tcb { " + this.task + "@" + state + " }";
};

/**
 * An idle task doesn't do any work itself but cycles control between the two
 * device tasks.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @param {int} v1 a seed value that controls how the device tasks are scheduled
 * @param {int} count the number of times this task should be scheduled
 * @constructor
 */
function IdleTask(scheduler, v1, count) {
    this._scheduler = scheduler;
    this._v1 = v1;
    this._count = count;
}

IdleTask.prototype.__defineGetter__("scheduler", function() { randomException(); return this._scheduler; });
IdleTask.prototype.__defineGetter__("v1", function() { randomException(); return this._v1; });
IdleTask.prototype.__defineSetter__("v1", function(value) { this._v1 = value; randomException(); });
IdleTask.prototype.__defineGetter__("count", function() { randomException(); return this._count; });
IdleTask.prototype.__defineSetter__("count", function(value) { this._count = value; randomException(); });

IdleTask.prototype.run = function (packet) {
    let count;
    try {
        count = this.count;
    } catch(e) {
        count = this.count;
    }
    try {
        this.count = count - 1;
    } catch(e) { }
    let scheduler;
    try {
        scheduler = this.scheduler;
    } catch(e) {
        scheduler = this.scheduler;
    }

    try {
        count = this.count;
    } catch(e) {
        count = this.count;
    }
    if (count == 0) return scheduler.holdCurrent();
    let v1;
    try {
        v1 = this.v1;
    } catch(e) {
        v1 = this.v1;
    }
    if ((v1 & 1) == 0) {
        let v1;
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        try {
            this.v1 = v1 >> 1;
        } catch(e) { }
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        return scheduler.release(ID_DEVICE_A);
    } else {
        let v1;
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        try {
            this.v1 = (v1 >> 1) ^ 0xD008;
        } catch(e) { }
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        return scheduler.release(ID_DEVICE_B);
    }
};

IdleTask.prototype.toString = function () {
    return "IdleTask"
};

/**
 * A task that suspends itself after each time it has been run to simulate
 * waiting for data from an external device.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @constructor
 */
function DeviceTask(scheduler) {
    this._scheduler = scheduler;
    this._v1 = null;
}

DeviceTask.prototype.__defineGetter__("scheduler", function() { randomException(); return this._scheduler; });
DeviceTask.prototype.__defineGetter__("v1", function() { randomException(); return this._v1; });
DeviceTask.prototype.__defineSetter__("v1", function(value) { this._v1 = value; randomException(); });

DeviceTask.prototype.run = function (packet) {
    if (packet == null) {
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        let v1;
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        if (v1 == null) return scheduler.suspendCurrent();
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        var v = v1;
        try {
            this.v1 = null;
        } catch(e) { }

        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        let queue;
        try {
            queue = scheduler.queue;
        } catch(e) {
            queue = scheduler.queue;
        }
        return queue.bind(scheduler)(v);
    } else {
        try {
            this.v1 = packet;
        } catch(e) { }
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        return scheduler.holdCurrent();
    }
};

DeviceTask.prototype.toString = function () {
    return "DeviceTask";
};

/**
 * A task that manipulates work packets.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @param {int} v1 a seed used to specify how work packets are manipulated
 * @param {int} v2 another seed used to specify how work packets are manipulated
 * @constructor
 */
function WorkerTask(scheduler, v1, v2) {
    this._scheduler = scheduler;
    this._v1 = v1;
    this._v2 = v2;
}

WorkerTask.prototype.__defineGetter__("scheduler", function() { randomException(); return this._scheduler; });
WorkerTask.prototype.__defineGetter__("v1", function() { randomException(); return this._v1; });
WorkerTask.prototype.__defineSetter__("v1", function(value) { this._v1 = value; randomException(); });
WorkerTask.prototype.__defineGetter__("v2", function() { randomException(); return this._v2; });
WorkerTask.prototype.__defineSetter__("v2", function(value) { this._v2 = value; randomException(); });

WorkerTask.prototype.run = function (packet) {
    if (packet == null) {
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        return scheduler.suspendCurrent();
    } else {
        let v1;
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        if (v1 == ID_HANDLER_A) {
            try {
                this.v1 = ID_HANDLER_B;
            } catch(e){}
        } else {
            try {
                this.v1 = ID_HANDLER_A;
            } catch(e) { }
        }
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        try {
            packet.id = v1;
        } catch(e) { }
        try {
            packet.a1 = 0;
        } catch(e) { }
        for (var i = 0; i < DATA_SIZE; i++) {
            let v2;
            try {
                v2 = this.v2;
            } catch(e) {
                v2 = this.v2;
            }
            v2++;
            try {
                this.v2 = v2;
            } catch(e) { }
            try {
                v2 = this.v2;
            } catch(e) {
                v2 = this.v2;
            }
            if (v2 > 26) {
                try {
                    this.v2 = 1;
                } catch(e) { }
            } 
            try {
                v2 = this.v2;
            } catch(e) {
                v2 = this.v2;
            }
            packet.a2[i] = v2;
        }
        let scheduler;
        try {
            scheduler = this.scheduler;
        } catch(e) {
            scheduler = this.scheduler;
        }
        let queue;
        try {
            queue = scheduler.queue;
        } catch(e) {
            queue = scheduler.queue;
        }
        return queue.bind(scheduler)(packet);
    }
};

WorkerTask.prototype.toString = function () {
    return "WorkerTask";
};

/**
 * A task that manipulates work packets and then suspends itself.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @constructor
 */
function HandlerTask(scheduler) {
    this._scheduler = scheduler;
    this._v1 = null;
    this._v2 = null;
}

HandlerTask.prototype.__defineGetter__("scheduler", function() { randomException(); return this._scheduler; });
HandlerTask.prototype.__defineGetter__("v1", function() { randomException(); return this._v1; });
HandlerTask.prototype.__defineSetter__("v1", function(value) { this._v1 = value; randomException(); });
HandlerTask.prototype.__defineGetter__("v2", function() { randomException(); return this._v2; });
HandlerTask.prototype.__defineSetter__("v2", function(value) { this._v2 = value; randomException(); });

HandlerTask.prototype.run = function (packet) {
    if (packet != null) {
        let kind;
        try {
            kind = packet.kind;
        } catch(e) {
            kind = packet.kind;
        }
        if (kind == KIND_WORK) {
            let v1;
            try {
                v1 = this.v1;
            } catch(e) {
                v1 = this.v1;
            }
            try {
                this.v1 = packet.addTo(v1);
            } catch(e) { }
        } else {
            let v2;
            try {
                v2 = this.v2;
            } catch(e) {
                v2 = this.v2;
            }
            try {
                this.v2 = packet.addTo(v2);
            } catch(e) { }
        }
    }
    let v1;
    try {
        v1 = this.v1;
    } catch(e) {
        v1 = this.v1;
    }
    if (v1 != null) {
        let v1;
        try {
            v1 = this.v1;
        } catch(e) {
            v1 = this.v1;
        }
        var count = v1.a1;
        var v;
        if (count < DATA_SIZE) {
            let v2;
            try {
                v2 = this.v2;
            } catch(e) {
                v2 = this.v2;
            }
            if (v2 != null) {
                let v2;
                try {
                    v2 = this.v2;
                } catch(e) {
                    v2 = this.v2;
                }
                v = v2;
                let link;
                try {
                    v2 = this.v2;
                } catch(e) {
                    v2 = this.v2;
                }
                try {
                    link = v2.link;
                } catch(e) {
                    link = v2.link;
                }
                try {
                    this.v2 = link;
                } catch(e) { }
                let v1;
                try {
                    v1 = this.v1;
                } catch(e) {
                    v1 = this.v1;
                }
                try {
                    v.a1 = v1.a2[count];
                } catch(e) { }
                try {
                    v1 = this.v1;
                } catch(e) {
                    v1 = this.v1;
                }
                try {
                    v1.a1 = count + 1;
                } catch(e) { }
                let scheduler;
                try {
                    scheduler = this.scheduler;
                } catch(e) {
                    scheduler = this.scheduler;
                }
                let queue;
                try {
                    queue = scheduler.queue;
                } catch(e) {
                    queue = scheduler.queue;
                }
                return queue.bind(scheduler)(v);
            }
        } else {
            let v1;
            try {
                v1 = this.v1;
            } catch(e) {
                v1 = this.v1;
            }
            v = v1;
            try {
                v1 = this.v1;
            } catch(e) {
                v1 = this.v1;
            }
            let link;
            try {
                link = v1.link;
            } catch(e) {
                link = v1.link;
            }
            try {
                this.v1 = link;
            } catch(e) { }
            let scheduler;
            try {
                scheduler = this.scheduler;
            } catch(e) {
                scheduler = this.scheduler;
            }
            let queue;
            try {
                queue = scheduler.queue;
            } catch(e) {
                queue = scheduler.queue;
            }
            return queue.bind(scheduler)(v);
        }
    }
    let scheduler;
    try {
        scheduler = this.scheduler;
    } catch(e) {
        scheduler = this.scheduler;
    }
    return scheduler.suspendCurrent();
};

HandlerTask.prototype.toString = function () {
    return "HandlerTask";
};

/* --- *
 * P a c k e t
 * --- */

var DATA_SIZE = 4;

/**
 * A simple package of data that is manipulated by the tasks.  The exact layout
 * of the payload data carried by a packet is not importaint, and neither is the
 * nature of the work performed on packets by the tasks.
 *
 * Besides carrying data, packets form linked lists and are hence used both as
 * data and worklists.
 * @param {Packet} link the tail of the linked list of packets
 * @param {int} id an ID for this packet
 * @param {int} kind the type of this packet
 * @constructor
 */
function Packet(link, id, kind) {
    this._link = link;
    this._id = id;
    this._kind = kind;
    this._a1 = 0;
    this._a2 = new Array(DATA_SIZE);
}

Packet.prototype.__defineGetter__("link", function() { randomException(); return this._link; });
Packet.prototype.__defineSetter__("link", function(value) { this._link = value; randomException(); });
Packet.prototype.__defineGetter__("id", function() { return this._id; });
Packet.prototype.__defineSetter__("id", function(value) { this._id = value; randomException(); });
Packet.prototype.__defineGetter__("kind", function() { randomException(); return this._kind; });
Packet.prototype.__defineGetter__("a1", function() { return this._a1; });
Packet.prototype.__defineSetter__("a1", function(value) { this._a1 = value; randomException(); });
Packet.prototype.__defineGetter__("a2", function() { return this._a2; });

/**
 * Add this packet to the end of a worklist, and return the worklist.
 * @param {Packet} queue the worklist to add this packet to
 */
Packet.prototype.addTo = function (queue) {
    try {
        this.link = null;
    } catch(e) { }
    if (queue == null) return this;
    var peek, next = queue;
    while (true) {
        let link;
        try {
            link = next.link;
        } catch(e) {
            link = next.link;
        }
        if (link == null)
            break;
        peek = link;
        next = peek;
    }
    try {
        next.link = this;
    } catch(e) { }
    return queue;
};

Packet.prototype.toString = function () {
    return "Packet";
};

for (var i = 0; i < 30; ++i)
    runRichards();
