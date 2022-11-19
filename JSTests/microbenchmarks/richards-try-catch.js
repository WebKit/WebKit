//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"
//@ requireOptions("--useDataICInFTL=true", "--useDataICSharing=true")

// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
// Martin Richards.


let __exceptionCounter = 0;
function randomException() {
    __exceptionCounter++;
    if (__exceptionCounter % 5000 === 0) {
        throw new Error("rando");
    }
}
noInline(randomException);

/**
 * The Richards benchmark simulates the task dispatcher of an
 * operating system.
 **/
function runRichards() {
    try {
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

        if (scheduler.queueCount != EXPECTED_QUEUE_COUNT ||
                scheduler.holdCount != EXPECTED_HOLD_COUNT) {
            var msg =
                "Error during execution: queueCount = " + scheduler.queueCount +
                ", holdCount = " + scheduler.holdCount + ".";
            throw new Error(msg);
        }

        randomException();
    } catch(e) { }
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
    try {
        this.queueCount = 0;
        this.holdCount = 0;
        this.blocks = new Array(NUMBER_OF_IDS);
        this.list = null;
        this.currentTcb = null;
        this.currentId = null;

        randomException();
    } catch(e) { }
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

/**
 * Add an idle task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {int} count the number of times to schedule the task
 */
Scheduler.prototype.addIdleTask = function (id, priority, queue, count) {
    try {
        this.addRunningTask(id, priority, queue, new IdleTask(this, 1, count));
        randomException();
    } catch(e) { }
};

/**
 * Add a work task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addWorkerTask = function (id, priority, queue) {
    try {
        this.addTask(id, priority, queue, new WorkerTask(this, ID_HANDLER_A, 0));
        randomException();
    } catch(e) { }
};

/**
 * Add a handler task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addHandlerTask = function (id, priority, queue) {
    try {
        this.addTask(id, priority, queue, new HandlerTask(this));
        randomException();
    } catch(e) { }
};

/**
 * Add a handler task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 */
Scheduler.prototype.addDeviceTask = function (id, priority, queue) {
    try {
        this.addTask(id, priority, queue, new DeviceTask(this))
        randomException();
    } catch(e) { }
};

/**
 * Add the specified task and mark it as running.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {Task} task the task to add
 */
Scheduler.prototype.addRunningTask = function (id, priority, queue, task) {
    try {
        this.addTask(id, priority, queue, task);
        this.currentTcb.setRunning();
        randomException();
    } catch(e) { }
};

/**
 * Add the specified task to this scheduler.
 * @param {int} id the identity of the task
 * @param {int} priority the task's priority
 * @param {Packet} queue the queue of work to be processed by the task
 * @param {Task} task the task to add
 */
Scheduler.prototype.addTask = function (id, priority, queue, task) {
    try {
        this.currentTcb = new TaskControlBlock(this.list, id, priority, queue, task);
        this.list = this.currentTcb;
        this.blocks[id] = this.currentTcb;
        randomException();
    } catch(e) { }
};

/**
 * Execute the tasks managed by this scheduler.
 */
Scheduler.prototype.schedule = function () {
    this.currentTcb = this.list;
    while (this.currentTcb != null) {
        try {
            if (this.currentTcb.isHeldOrSuspended()) {
                this.currentTcb = this.currentTcb.link;
            } else {
                this.currentId = this.currentTcb.id;
                this.currentTcb = this.currentTcb.run();
            }
            randomException();
        } catch(e) { }
    }
};

/**
 * Release a task that is currently blocked and return the next block to run.
 * @param {int} id the id of the task to suspend
 */
Scheduler.prototype.release = function (id) {
    try {
        var tcb = this.blocks[id];
        if (tcb == null) return tcb;
        tcb.markAsNotHeld();
        randomException();
    } catch(e) { }
    try {
        if (tcb.priority > this.currentTcb.priority) {
            return tcb;
        } else {
            return this.currentTcb;
        }
    } catch(e) { }
};

/**
 * Block the currently executing task and return the next task control block
 * to run.  The blocked task will not be made runnable until it is explicitly
 * released, even if new work is added to it.
 */
Scheduler.prototype.holdCurrent = function () {
    try {
        this.holdCount++;
        this.currentTcb.markAsHeld();
        randomException();
    } catch(e) { }
    return this.currentTcb.link;
};

/**
 * Suspend the currently executing task and return the next task control block
 * to run.  If new work is added to the suspended task it will be made runnable.
 */
Scheduler.prototype.suspendCurrent = function () {
    try {
        this.currentTcb.markAsSuspended();
        randomException();
    } catch(e) { }
    try {
        return this.currentTcb;
    } catch(e) { }
};

/**
 * Add the specified packet to the end of the worklist used by the task
 * associated with the packet and make the task runnable if it is currently
 * suspended.
 * @param {Packet} packet the packet to add
 */
Scheduler.prototype.queue = function (packet) {
    try {
        var t = this.blocks[packet.id];
        if (t == null) return t;
        this.queueCount++;
        packet.link = null;
        randomException();
    } catch(e) { }
    packet.id = this.currentId;
    return t.checkPriorityAdd(this.currentTcb, packet);
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
    try {
        this.link = link;
        this.id = id;
        this.priority = priority;
        this.queue = queue;
        this.task = task;
        randomException();
    } catch(e) { }
    try {
        if (queue == null) {
            this.state = STATE_SUSPENDED;
        } else {
            this.state = STATE_SUSPENDED_RUNNABLE;
        }
        randomException();
    } catch(e) { }
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

TaskControlBlock.prototype.setRunning = function () {
    try {
        this.state = STATE_RUNNING;
        randomException();
    } catch(e){}
};

TaskControlBlock.prototype.markAsNotHeld = function () {
    try {
        this.state = this.state & STATE_NOT_HELD;
        randomException();
    } catch(e) { }
};

TaskControlBlock.prototype.markAsHeld = function () {
    try {
        this.state = this.state | STATE_HELD;
        randomException();
    } catch(e) { }
};

TaskControlBlock.prototype.isHeldOrSuspended = function () {
    try {
        randomException();
        return (this.state & STATE_HELD) != 0 || (this.state == STATE_SUSPENDED);
    } catch(e) { 
        return (this.state & STATE_HELD) != 0 || (this.state == STATE_SUSPENDED);
    }
};

TaskControlBlock.prototype.markAsSuspended = function () {
    try {
        randomException();
        this.state = this.state | STATE_SUSPENDED;
    } catch(e) { 
        this.state = this.state | STATE_SUSPENDED;
    }
};

TaskControlBlock.prototype.markAsRunnable = function () {
    try {
        randomException();
        this.state = this.state | STATE_RUNNABLE;
    } catch(e) {
        this.state = this.state | STATE_RUNNABLE;
    }
};

/**
 * Runs this task, if it is ready to be run, and returns the next task to run.
 */
TaskControlBlock.prototype.run = function () {
    var packet;
    try {
        if (this.state == STATE_SUSPENDED_RUNNABLE) {
            packet = this.queue;
            this.queue = packet.link;
            if (this.queue == null) {
                this.state = STATE_RUNNING;
            } else {
                this.state = STATE_RUNNABLE;
            }
        } else {
            packet = null;
        }
        randomException();
    } catch(e) { }
    return this.task.run(packet);
};

/**
 * Adds a packet to the worklist of this block's task, marks this as runnable if
 * necessary, and returns the next runnable object to run (the one
 * with the highest priority).
 */
TaskControlBlock.prototype.checkPriorityAdd = function (task, packet) {
    try {
        if (this.queue == null) {
            this.queue = packet;
            this.markAsRunnable();
            if (this.priority > task.priority) return this;
        } else {
            this.queue = packet.addTo(this.queue);
        }

        randomException();
        return task;
    } catch(e) { 
        return task;
    }
};

TaskControlBlock.prototype.toString = function () {
    try {
        randomException();
        return "tcb { " + this.task + "@" + this.state + " }";
    } catch(e) {
        return "tcb { " + this.task + "@" + this.state + " }";
    }
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
    try {
        this.scheduler = scheduler;
        this.v1 = v1;
        randomException();
        this.count = count;
    } catch(e) {
        this.count = count;
    }
}

IdleTask.prototype.run = function (packet) {
    try {
        this.count--;
        if (this.count == 0) return this.scheduler.holdCurrent();
        randomException();
    } catch(e) { }

    try {
        if ((this.v1 & 1) == 0) {
            this.v1 = this.v1 >> 1;
            return this.scheduler.release(ID_DEVICE_A);
        } else {
            this.v1 = (this.v1 >> 1) ^ 0xD008;
            return this.scheduler.release(ID_DEVICE_B);
        }
    } catch(e) { }
};

IdleTask.prototype.toString = function () {
    try {
        randomException();
        return "IdleTask"
    } catch(e) {
        return "IdleTask"
    }
};

/**
 * A task that suspends itself after each time it has been run to simulate
 * waiting for data from an external device.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @constructor
 */
function DeviceTask(scheduler) {
    try {
        this.scheduler = scheduler;
        this.v1 = null;
        randomException();
    } catch(e) { }
}

DeviceTask.prototype.run = function (packet) {
    if (packet == null) {
        try {
            if (this.v1 == null) return this.scheduler.suspendCurrent();
            var v = this.v1;
            this.v1 = null;
            randomException();
        } catch(e) { }
        return this.scheduler.queue(v);
    } else {
        try {
            this.v1 = packet;
            randomException();
        } catch(e) { }
        return this.scheduler.holdCurrent();
    }
};

DeviceTask.prototype.toString = function () {
    try {
        randomException();
        return "DeviceTask";
    } catch(e) { }
};

/**
 * A task that manipulates work packets.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @param {int} v1 a seed used to specify how work packets are manipulated
 * @param {int} v2 another seed used to specify how work packets are manipulated
 * @constructor
 */
function WorkerTask(scheduler, v1, v2) {
    try {
        this.scheduler = scheduler;
        this.v1 = v1;
        this.v2 = v2;
        randomException();
    } catch(e) { }
}

WorkerTask.prototype.run = function (packet) {
    if (packet == null) {
        return this.scheduler.suspendCurrent();
    } else {
        try {
            if (this.v1 == ID_HANDLER_A) {
                this.v1 = ID_HANDLER_B;
            } else {
                this.v1 = ID_HANDLER_A;
            }
            packet.id = this.v1;
            packet.a1 = 0;
            randomException();
        } catch(e) { }
        for (var i = 0; i < DATA_SIZE; i++) {
            try {
                this.v2++;
                if (this.v2 > 26) this.v2 = 1;
                packet.a2[i] = this.v2;
                randomException();
            } catch(e) { }
        }
        return this.scheduler.queue(packet);
    }
};

WorkerTask.prototype.toString = function () {
    try {
        return "WorkerTask";
    } catch(e) { }
};

/**
 * A task that manipulates work packets and then suspends itself.
 * @param {Scheduler} scheduler the scheduler that manages this task
 * @constructor
 */
function HandlerTask(scheduler) {
    try {
        this.scheduler = scheduler;
        this.v1 = null;
        this.v2 = null;
        randomException();
    } catch(e) { }
}

HandlerTask.prototype.run = function (packet) {
    try {
        if (packet != null) {
            if (packet.kind == KIND_WORK) {
                this.v1 = packet.addTo(this.v1);
            } else {
                this.v2 = packet.addTo(this.v2);
            }
        }
        randomException();
    } catch(e) { }

    try {
        if (this.v1 != null) {
            var count = this.v1.a1;
            var v;
            if (count < DATA_SIZE) {
                if (this.v2 != null) {
                    v = this.v2;
                    this.v2 = this.v2.link;
                    v.a1 = this.v1.a2[count];
                    this.v1.a1 = count + 1;
                    return this.scheduler.queue(v);
                }
            } else {
                v = this.v1;
                this.v1 = this.v1.link;
                return this.scheduler.queue(v);
            }
        }
        randomException();
    } catch(e) { }
    return this.scheduler.suspendCurrent();
};

HandlerTask.prototype.toString = function () {
    try {
        return "HandlerTask";
    } catch(e) { }
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
    try {
        this.link = link;
        this.id = id;
        this.kind = kind;
        this.a1 = 0;
        this.a2 = new Array(DATA_SIZE);
        randomException();
    } catch(e) { }
}

/**
 * Add this packet to the end of a worklist, and return the worklist.
 * @param {Packet} queue the worklist to add this packet to
 */
Packet.prototype.addTo = function (queue) {
    this.link = null;
    if (queue == null) return this;
    var peek, next = queue;
    while ((peek = next.link) != null) {
        try {
            next = peek;
            randomException();
        } catch(e) { }
    }
    next.link = this;
    return queue;
};

Packet.prototype.toString = function () {
    try {
        return "Packet";
    } catch(e) { }
};

for (let i = 0; i < 350; ++i)
  runRichards();
