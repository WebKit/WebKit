# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import logging
import math
import multiprocessing
import queue as Queue
import signal
import sys

from webkitcorepy import OutputCapture, Timeout, log


class _Message(object):
    def __init__(self, who=None):
        self.who = who or _Process.name

    def __call__(self, caller):
        raise NotImplementedError()


class _Task(_Message):
    def __init__(self, function, id, *args, **kwargs):
        super(_Task, self).__init__()

        self.function = function
        self.id = id
        self.args = tuple(args)
        self.kwargs = kwargs
        self.repeat = False  # Will be set by TaskPool

    def __call__(self, caller):
        return self.function(*self.args, **self.kwargs)


class _Result(_Message):
    def __init__(self, value, id, repeat=False):
        super(_Result, self).__init__()
        self.value = value
        self.id = id
        self.repeat = repeat

    def __call__(self, caller):
        if caller:
            if getattr(self, 'repeat', False):
                # For repeat tasks, only decrement count when they actually stop
                # (not on each iteration)
                if getattr(self, 'stopped', False):
                    caller.repeat_pending_count -= 1
            else:
                caller.pending_count -= 1
            caller.callbacks.pop(self.id, lambda value: value)(self.value)
        return self.value


class _StopRepeat(_Message):
    def __init__(self):
        super(_StopRepeat, self).__init__()

    def __call__(self, caller):
        # This message signals workers to stop repeat tasks
        if hasattr(caller, 'stop_repeat'):
            caller.stop_repeat = True
        return None


class _Log(_Message):
    def __init__(self, record):
        super(_Log, self).__init__()
        self.record = record

    def __call__(self, caller):
        logging.getLogger(self.record.name).log(self.record.levelno, '{} {}'.format(self.who, self.record.getMessage()))


class _Print(_Message):
    stdout = 1
    stderr = 2

    def __init__(self, lines, stream=stdout):
        super(_Print, self).__init__()
        self.lines = lines
        self.stream = stream

    def __call__(self, caller):
        stream = {
            self.stdout: sys.stdout,
            self.stderr: sys.stderr,
        }.get(self.stream, sys.stderr)
        for line in self.lines:
            stream.write('{}\n'.format(line))


class _State(_Message):
    STARTING, STOPPING = 1, 0
    STATES = [STARTING, STOPPING]

    def __init__(self, state, mutually_exclusive_groups=None):
        super(_State, self).__init__()
        self.state = state
        self.mutually_exclusive_groups = mutually_exclusive_groups or []

    def __call__(self, caller):
        log.info('{} {}{}'.format(
            self.who, {
                self.STARTING: 'starting',
                self.STOPPING: 'stopping',
            }.get(self.state, self.state),
            ' ({})'.format(', '.join(self.mutually_exclusive_groups)) if self.mutually_exclusive_groups else '',
        ))
        if caller:
            caller._started += {
                self.STARTING: 1,
                self.STOPPING: -1,
            }.get(self.state, 0)
        return self.state


class _ChildException(_Message):
    def __init__(self, exc_info=None):
        super(_ChildException, self).__init__()
        self.exc_info = exc_info or sys.exc_info()

    def __call__(self, caller):
        from six import reraise
        _, exception, trace = sys.exc_info()
        if exception:
            log.critical("Exception in flight, '{}' ignored".format(self.exc_info[1]))
            return

        reraise(*self.exc_info)


class _BiDirectionalQueue(object):
    def __init__(self, outgoing=None, incoming=None):
        self.outgoing = outgoing or multiprocessing.Queue()
        self.incoming = incoming or multiprocessing.Queue()

    def send(self, object):
        if self.outgoing._closed:
            sys.stderr.write('Cannot send message to closed queue\n')
            return False
        return self.outgoing.put(object)

    def receive(self, blocking=True):
        with Timeout.DisableAlarm():
            if not blocking:
                return self.incoming.get(block=False)

            difference = Timeout.difference()
            try:
                if difference is not None:
                    return self.incoming.get(timeout=difference)
                return self.incoming.get()
            except Queue.Empty:
                pass

    def close(self):
        with OutputCapture():
            self.outgoing.close()
            self.incoming.close()
            self.outgoing.join_thread()
            self.incoming.join_thread()


class _Queue(object):
    def __init__(self, queue=None):
        self.queue = queue or multiprocessing.Queue()

    def send(self, object):
        if self.queue._closed:
            sys.stderr.write('Cannot send message to closed queue\n')
            return False
        return self.queue.put(object)

    def receive(self, blocking=True):
        with Timeout.DisableAlarm():
            try:
                if not blocking:
                    return self.queue.get(block=False)

                difference = Timeout.difference()
                if difference is not None:
                    return self.queue.get(timeout=difference)
                return self.queue.get()
            except Queue.Empty:
                pass

    def close(self):
        with OutputCapture():
            self.queue.close()
            self.queue.join_thread()


class _DummyQueue(object):
    def send(self, object):
        if isinstance(object, _Message):
            object(None)
        return True

    def receive(self, blocking=True):
        pass

    def close(self):
        pass


class _Process(object):
    name = None
    working = False
    queue = None
    stop_repeat = False
    name = None
    working = False
    queue = None
    stop_repeat = False
    repeat_task_queue = None

    class LogHandler(logging.Handler):
        def __init__(self, queue, **kwargs):
            self._queue = queue
            super(_Process.LogHandler, self).__init__(**kwargs)

        def emit(self, record):
            self._queue.send(_Log(record))

    class Stream(io.IOBase):
        def __init__(self, handle, queue):
            if not handle:
                raise ValueError('No target streams provided')
            self.handle = handle
            self.cache = None
            self._queue = queue

        def flush(self):
            if self.cache is not None:
                self._queue.send(_Print(lines=[self.cache], stream=self.handle))
                self.cache = None

        def writelines(self, lines):
            for line in lines:
                self.write(line)

        def write(self, data):
            to_be_printed = []
            for c in data:
                if c == '\n':
                    to_be_printed.append(self.cache or '')
                    self.cache = None
                elif c not in ['\r', '\0']:
                    self.cache = c if self.cache is None else (self.cache + c)
            if to_be_printed:
                self._queue.send(_Print(lines=to_be_printed, stream=self.handle))
            return len(data)

        @property
        def closed(self):
            return False

        def close(self):
            self.flush()

        def fileno(self):
            return self.handle

        def isatty(self):
            return False

        def readable(self):
            return False

        def readline(self, size=-1):
            raise NotImplementedError()

        def readlines(self, hint=-1):
            raise NotImplementedError()

        def seek(self, offset, whence=io.SEEK_SET):
            raise NotImplementedError()

        def seekable(self):
            return False

        def tell(self):
            raise NotImplementedError()

        def truncate(self, size=None):
            raise NotImplementedError()

        def writable(self):
            return True

    @classmethod
    def handler(cls, value, _):
        if value in (getattr(signal, 'SIGTERM'), getattr(signal, 'SIGINT')):
            cls.working = False

    @classmethod
    def main(cls, name, loglevel, setup, setupargs, setupkwargs, queue, teardown, teardownargs, teardownkwargs, mutually_exclusive_group_queues):
        from tblib import pickling_support

        cls.name = name
        cls.working = True
        cls.name = name
        cls.working = True
        cls.repeat_task_queue = []

        setupargs = setupargs or []
        setupkwargs = setupkwargs or {}

        teardownargs = teardownargs or []
        teardownkwargs = teardownkwargs or {}

        if getattr(signal, 'SIGTERM'):
            signal.signal(signal.SIGTERM, cls.handler)
        if getattr(signal, 'SIGINT'):
            signal.signal(signal.SIGINT, cls.handler)

        logger = logging.getLogger()
        for handler in logger.handlers:
            logger.removeHandler(handler)
        logger.addHandler(cls.LogHandler(queue))
        logger.setLevel(loglevel)

        cls.queue = queue
        group_queues = list(mutually_exclusive_group_queues.values())
        queue.send(_State(_State.STARTING, list(mutually_exclusive_group_queues.keys())))

        with OutputCapture.ReplaceSysStream('stderr', cls.Stream(_Print.stderr, queue)), OutputCapture.ReplaceSysStream('stdout', cls.Stream(_Print.stdout, queue)):
            try:
                pickling_support.install()
                if setup:
                    setup(*setupargs, **setupkwargs)

                while cls.working:
                    task = None

                    # Check group queues first (including for control messages)
                    for i in range(len(group_queues)):
                        group_queue = group_queues[i]
                        task = group_queue.receive(blocking=False)
                        if task:
                            del group_queues[i]
                            group_queues.append(group_queue)
                            break

                    # Handle _StopRepeat message early (before checking repeat queue)
                    if isinstance(task, _StopRepeat):
                        task(cls)  # This sets cls.stop_repeat = True
                        # Send final results for any queued repeat tasks
                        while cls.repeat_task_queue:
                            stopped_task = cls.repeat_task_queue.pop(0)
                            result_msg = _Result(value=None, id=stopped_task.id, repeat=True)
                            result_msg.stopped = True
                            queue.send(result_msg)
                        # Exit worker loop immediately to proceed to teardown
                        break

                    # If no task from group queues, check for repeat tasks
                    if not task and cls.repeat_task_queue:
                        task = cls.repeat_task_queue.pop(0)

                    # If still no task, handle main queue access
                    if not task:
                        if not group_queues:
                            # Workers NOT assigned to any groups can pull regular tasks from main queue
                            task = queue.receive()
                            # Handle shutdown signal (None)
                            if task is None:
                                break
                        else:
                            # Grouped workers check main queue for shutdown signals (non-blocking)
                            try:
                                main_task = queue.receive(blocking=False)
                                if main_task is None:
                                    break  # Shutdown signal received
                                elif main_task is not None:
                                    task = main_task  # Got a task from main queue
                            except Queue.Empty:
                                pass  # No task available, continue looping
                            if not task:
                                continue  # Continue to next iteration of worker loop

                    # Handle _StopRepeat message (applies to all workers from any queue)
                    if isinstance(task, _StopRepeat):
                        task(cls)  # This sets cls.stop_repeat = True
                        # Send final "stopped" results for any queued repeat tasks
                        while cls.repeat_task_queue:
                            stopped_task = cls.repeat_task_queue.pop(0)
                            result_msg = _Result(value=None, id=stopped_task.id, repeat=True)
                            result_msg.stopped = True
                            queue.send(result_msg)
                        # Only grouped workers exit here; regular workers continue to wait for None
                        if group_queues:
                            break
                        continue

                    # For repeat tasks, check stop condition
                    if getattr(task, 'repeat', False):
                        if not cls.stop_repeat:
                            result = task(None)
                            queue.send(_Result(value=result, id=task.id, repeat=True))
                            # Re-queue the repeat task to run again
                            cls.repeat_task_queue.append(task)
                        else:
                            # Task is stopping - send a final result with stopped=True
                            result_msg = _Result(value=None, id=task.id, repeat=True)
                            result_msg.stopped = True
                            queue.send(result_msg)
                    else:
                        result = task(None)
                        queue.send(_Result(value=result, id=task.id, repeat=False))

            except BaseException:
                typ, exception, traceback = sys.exc_info()
                queue.send(_ChildException(exc_info=(
                    typ, typ('{} (from {})'.format(str(exception), name)), traceback,
                )))

            finally:
                if teardown:
                    teardown(*teardownargs, **teardownkwargs)
                sys.stdout.flush()
                sys.stderr.flush()
                queue.send(_State(_State.STOPPING, list(mutually_exclusive_group_queues.keys())))
                cls.queue.close()
                cls.queue = None


class TaskPool(object):
    Message = _Message
    Task = _Task
    Result = _Result
    Log = _Log
    Print = _Print
    State = _State
    ChildException = _ChildException
    BiDirectionalQueue = _BiDirectionalQueue
    Queue = _Queue
    Process = _Process

    class Exception(RuntimeError):
        pass

    def __init__(
        self, workers=1, name=None, setup=None, teardown=None, enter_grace_period=60, exit_grace_period=5, block_size=1000,
        setupargs=None, setupkwargs=None,
        teardownargs=None, teardownkwargs=None,
        force_fork=False,
        mutually_exclusive_groups=None,
    ):
        # Ensure tblib is installed before creating child processes
        import tblib  # noqa: F401

        name = name or 'worker'
        if name == self.Process.name:
            raise ValueError("Parent process is already named {}".format(name))
        self.name = name

        if workers < 1:
            raise ValueError('TaskPool requires positive number of workers')

        self.queue = None
        self.mutually_exclusive_groups = set(mutually_exclusive_groups or [])
        self._group_queues = dict()
        self.workers = []

        self._setup_args = (setup, setupargs or [], setupkwargs or {})
        self._teardown_args = (teardown, teardownargs or [], teardownkwargs or {})
        self._num_workers = int(workers)

        self._started = 0

        self.callbacks = {}
        self._id_count = 0
        self.pending_count = 0
        self.repeat_pending_count = 0
        self.enter_grace_period = enter_grace_period
        self._has_repeat_tasks = False
        self.exit_grace_period = exit_grace_period
        self.block_size = block_size
        self.force_fork = force_fork

        if not self.force_fork and self._num_workers == 1 and TaskPool.Process.queue:
            raise ValueError('Illegal single-process TaskPool nesting')

    def __enter__(self):
        if not self.force_fork and self._num_workers == 1:
            TaskPool.Process.queue = _DummyQueue()
            TaskPool.Process.name = TaskPool.Process.name or '{}/0'.format(self.name)
            if self._setup_args[0]:
                self._setup_args[0](*self._setup_args[1], **self._setup_args[2])
            TaskPool.Process.working = True
            return self

        self.queue = self.BiDirectionalQueue()
        self._group_queues = {
            name: self.Queue() for name in self.mutually_exclusive_groups
        }

        mutually_exclusive_groups = sorted(self.mutually_exclusive_groups)
        self.workers = []
        for count in range(self._num_workers):
            groups_for_worker = mutually_exclusive_groups[:int(math.ceil(float(len(mutually_exclusive_groups)) / (self._num_workers - count)))]
            mutually_exclusive_groups = mutually_exclusive_groups[len(groups_for_worker):]
            self.workers.append(multiprocessing.Process(
                target=self.Process.main,
                args=(
                    '{}/{}'.format(self.name, count), logging.getLogger().getEffectiveLevel(),
                    self._setup_args[0], self._setup_args[1], self._setup_args[2],
                    self.BiDirectionalQueue(outgoing=self.queue.incoming, incoming=self.queue.outgoing),
                    self._teardown_args[0], self._teardown_args[1], self._teardown_args[2],
                    {group: self._group_queues[group] for group in groups_for_worker},
                ),
            ))

        with Timeout(seconds=self.enter_grace_period, patch=False, handler=self.Exception('Failed to start all workers')):
            for worker in self.workers:
                worker.start()
            while self._started < len(self.workers):
                self.queue.receive()(self)

        return self

    def do(self, function, *args, **kwargs):
        callback = kwargs.pop('callback', None)
        group = kwargs.pop('group', None)
        repeat = kwargs.pop('repeat', False)
        if group and group not in self.mutually_exclusive_groups:
            raise ValueError("'{}' is not a recognized group".format(group))

        if not self.queue:
            result = function(*args, **kwargs)
            if callback:
                callback(result)
            return

        queue = self._group_queues.get(group, self.queue)

        if callback:
            self.callbacks[self._id_count] = callback

        task = self.Task(function, self._id_count, *args, **kwargs)
        task.repeat = repeat

        if repeat:
            self._has_repeat_tasks = True
            self.repeat_pending_count += 1
        else:
            self.pending_count += 1

        queue.send(task)
        self._id_count += 1

        # For every block of tasks passed to our workers, we need consume messages so we don't get deadlocked
        if not self._id_count % self.block_size:
            while self.pending_count > 2 * self._num_workers:
                try:
                    self.queue.receive(blocking=False)(self)
                except Queue.Empty:
                    break

    def wait(self):
        if not self.queue:
            return

        # Wait for regular tasks to complete first
        while self.pending_count > 0:
            self.queue.receive()(self)

        # If we have repeat tasks, signal them to stop
        if self._has_repeat_tasks:
            log.info('Regular tasks completed, signaling repeat tasks to stop')
            # Send _StopRepeat message to main queue and all group queues
            for _ in self.workers:
                self.queue.send(_StopRepeat())
            # Also send to all group queues so workers assigned to groups receive the signal
            for group_queue in self._group_queues.values():
                group_queue.send(_StopRepeat())

            # Wait for repeat tasks to be stopped (not "complete" - they're repeat!)
            while self.repeat_pending_count > 0:
                message = self.queue.receive()
                if message is not None:
                    message(self)

        # Now do normal shutdown - send shutdown signals to main queue only
        for _ in self.workers:
            self.queue.send(None)

        while self._started:
            self.queue.receive()(self)

    def __exit__(self, *args, **kwargs):
        if not self.queue:
            TaskPool.Process.working = False
            try:
                if self._teardown_args[0]:
                    self._teardown_args[0](*self._teardown_args[1], **self._teardown_args[2])
            finally:
                TaskPool.Process.queue = None
                TaskPool.Process.name = None
            return

        from six import reraise
        try:
            inflight = sys.exc_info()

            if inflight[1]:
                # We're about to terminate all the workers. Ignore any unprocessed jobs.
                self.queue.outgoing.cancel_join_thread()

            for worker in self.workers:
                if worker.is_alive():
                    worker.terminate()

            with Timeout(seconds=self.exit_grace_period):
                try:
                    while self._started:
                        self.queue.receive()(self)
                except Exception:
                    if inflight[1]:
                        log.critical('Some workers failed to gracefully shut down, but in-flight exception taking precedence')
                        reraise(*inflight)
                    raise self.Exception('Some workers failed to gracefully shut down')

        finally:
            for worker in self.workers:
                if not worker.is_alive():
                    continue

                worker.kill()

            self.queue.close()
            self.queue = None
            self.workers = []
