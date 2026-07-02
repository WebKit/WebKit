// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Excerpt from `wasm_gc_benchmarks/tools/run_wasm.js` to add own task queue
// implementation, since `setTimeout` and `queueMicrotask` are not always
// available in shells.
// TODO: Now (2025-08-14) that all shells have `setTimeout` available, can we
// remove this? Talk to Dart2wasm folks.
function addTaskQueue(self) {
  "use strict";

    // Task queue as cyclic list queue.
    var taskQueue = new Array(8);  // Length is power of 2.
    var head = 0;
    var tail = 0;
    var mask = taskQueue.length - 1;

    function addTask(elem) {
      taskQueue[head] = elem;
      head = (head + 1) & mask;
      if (head == tail) _growTaskQueue();
    }

    function removeTask() {
      if (head == tail) return;
      var result = taskQueue[tail];
      taskQueue[tail] = undefined;
      tail = (tail + 1) & mask;
      return result;
    }

    function _growTaskQueue() {
      // head == tail.
      var length = taskQueue.length;
      var split = head;
      taskQueue.length = length * 2;
      if (split * 2 < length) {  // split < length / 2
        for (var i = 0; i < split; i++) {
          taskQueue[length + i] = taskQueue[i];
          taskQueue[i] = undefined;
        }
        head += length;
      } else {
        for (var i = split; i < length; i++) {
          taskQueue[length + i] = taskQueue[i];
          taskQueue[i] = undefined;
        }
        tail += length;
      }
      mask = taskQueue.length - 1;
    }

    // Mapping from timer id to timer function.
    // The timer id is written on the function as .$timerId.
    // That field is cleared when the timer is cancelled, but it is not returned
    // from the queue until its time comes.
    var timerIds = {};
    var timerIdCounter = 1;  // Counter used to assign ids.

    // Zero-timer queue as simple array queue using push/shift.
    var zeroTimerQueue = [];

    function addTimer(f, ms) {
      ms = Math.max(0, ms);
      var id = timerIdCounter++;
      // A callback can be scheduled at most once.
      console.assert(f.$timerId === undefined);
      f.$timerId = id;
      timerIds[id] = f;
      if (ms == 0 && !isNextTimerDue()) {
        zeroTimerQueue.push(f);
      } else {
        addDelayedTimer(f, ms);
      }
      return id;
    }

    function nextZeroTimer() {
      while (zeroTimerQueue.length > 0) {
        var action = zeroTimerQueue.shift();
        if (action.$timerId !== undefined) return action;
      }
    }

    function nextEvent() {
      var action = removeTask();
      if (action) {
        return action;
      }
      do {
        action = nextZeroTimer();
        if (action) break;
        var nextList = nextDelayedTimerQueue();
        if (!nextList) {
          return;
        }
        var newTime = nextList.shift();
        advanceTimeTo(newTime);
        zeroTimerQueue = nextList;
      } while (true)
      var id = action.$timerId;
      clearTimerId(action, id);
      return action;
    }

    // Mocking time.
    var timeOffset = 0;
    var now = function() {
      // Install the mock Date object only once.
      // Following calls to "now" will just use the new (mocked) Date.now
      // method directly.
      installMockDate();
      now = Date.now;
      return Date.now();
    };
    var originalDate = Date;
    var originalNow = originalDate.now;

    function advanceTimeTo(time) {
      var now = originalNow();
      if (timeOffset < time - now) {
        timeOffset = time - now;
      }
    }

    function installMockDate() {
      var NewDate = function Date(Y, M, D, h, m, s, ms) {
        if (this instanceof Date) {
          // Assume a construct call.
          switch (arguments.length) {
            case 0:  return new originalDate(originalNow() + timeOffset);
            case 1:  return new originalDate(Y);
            case 2:  return new originalDate(Y, M);
            case 3:  return new originalDate(Y, M, D);
            case 4:  return new originalDate(Y, M, D, h);
            case 5:  return new originalDate(Y, M, D, h, m);
            case 6:  return new originalDate(Y, M, D, h, m, s);
            default: return new originalDate(Y, M, D, h, m, s, ms);
          }
        }
        return new originalDate(originalNow() + timeOffset).toString();
      };
      NewDate.UTC = originalDate.UTC;
      NewDate.parse = originalDate.parse;
      NewDate.now = function now() { return originalNow() + timeOffset; };
      NewDate.prototype = originalDate.prototype;
      originalDate.prototype.constructor = NewDate;
      Date = NewDate;
    }

    // Heap priority queue with key index.
    // Each entry is list of [timeout, callback1 ... callbackn].
    var timerHeap = [];
    var timerIndex = {};

    function addDelayedTimer(f, ms) {
      var timeout = now() + ms;
      var timerList = timerIndex[timeout];
      if (timerList == null) {
        timerList = [timeout, f];
        timerIndex[timeout] = timerList;
        var index = timerHeap.length;
        timerHeap.length += 1;
        bubbleUp(index, timeout, timerList);
      } else {
        timerList.push(f);
      }
    }

    function isNextTimerDue() {
      if (timerHeap.length == 0) return false;
      var head = timerHeap[0];
      return head[0] < originalNow() + timeOffset;
    }

    function nextDelayedTimerQueue() {
      if (timerHeap.length == 0) return null;
      var result = timerHeap[0];
      var last = timerHeap.pop();
      if (timerHeap.length > 0) {
        bubbleDown(0, last[0], last);
      }
      return result;
    }

    function bubbleUp(index, key, value) {
      while (index != 0) {
        var parentIndex = (index - 1) >> 1;
        var parent = timerHeap[parentIndex];
        var parentKey = parent[0];
        if (key > parentKey) break;
        timerHeap[index] = parent;
        index = parentIndex;
      }
      timerHeap[index] = value;
    }

    function bubbleDown(index, key, value) {
      while (true) {
        var leftChildIndex = index * 2 + 1;
        if (leftChildIndex >= timerHeap.length) break;
        var minChildIndex = leftChildIndex;
        var minChild = timerHeap[leftChildIndex];
        var minChildKey = minChild[0];
        var rightChildIndex = leftChildIndex + 1;
        if (rightChildIndex < timerHeap.length) {
          var rightChild = timerHeap[rightChildIndex];
          var rightKey = rightChild[0];
          if (rightKey < minChildKey) {
            minChildIndex = rightChildIndex;
            minChild = rightChild;
            minChildKey = rightKey;
          }
        }
        if (minChildKey > key) break;
        timerHeap[index] = minChild;
        index = minChildIndex;
      }
      timerHeap[index] = value;
    }

    function cancelTimer(id) {
      var f = timerIds[id];
      if (f == null) return;
      clearTimerId(f, id);
    }

    function clearTimerId(f, id) {
      f.$timerId = undefined;
      delete timerIds[id];
    }

    async function eventLoop(action) {
      while (action) {
        try {
          await action();
        } catch (e) {
          if (typeof onerror == "function") {
            onerror(e, null, -1);
          } else {
            throw e;
          }
        }
        action = nextEvent();
      }
    }

    self.setTimeout = addTimer;
    self.clearTimeout = cancelTimer;
    self.queueMicrotask = addTask;
    self.eventLoop = eventLoop;
}

function dartPrint(...args) { print(args); }
addTaskQueue(globalThis);
globalThis.window ??= globalThis;

class Benchmark {
  dart2wasmJsModule;
  compiledApp;

  async init() {
    // The generated JavaScript code from dart2wasm is an ES module, which we
    // can only load with a dynamic import (since this file is not a module.)

    Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
    this.dart2wasmJsModule = await JetStream.dynamicImport(JetStream.preload.jsModule);
  }

  async runIteration() {
    // Compile once in the first iteration.
    if (!this.compiledApp) {
      this.compiledApp = await this.dart2wasmJsModule.compile(Module.wasmBinary);
    }

    // Instantiate each iteration, since we can only `invokeMain()` with a
    // freshly instantiated module.
    const additionalImports = {};
    const instantiatedApp = await this.compiledApp.instantiate(additionalImports);

    const startTimeSinceEpochSeconds = new Date().getTime() / 1000;
    // Reduce workload size for a single iteration.
    // The default is 1000 frames, but that takes too long (>2s per iteration).
    const framesToDraw = 100;
    const initialFramesToSkip = 0;
    const dartArgs = [
      startTimeSinceEpochSeconds.toString(),
      framesToDraw.toString(),
      initialFramesToSkip.toString()
    ];

    await eventLoop(async () => {
      await instantiatedApp.invokeMain(...dartArgs)
    });
  }
}
