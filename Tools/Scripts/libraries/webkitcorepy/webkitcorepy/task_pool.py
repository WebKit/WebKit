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
import os
import logging
import multiprocessing
import signal
import sys

if sys.version_info < (3, 0):
    import Queue
else:
    import queue as Queue

from webkitcorepy import OutputCapture, Timeout, log


class _Message(object):
    def __init__(self, who=None):
        self.who = who or _Process.name

    def __call__(self, caller):
        NotImplemented()


class _Task(_Message):
    def __init__(self, function, id, *args, **kwargs):
        super(_Task, self).__init__()

        self.function = function
        self.id = id
        self.args = tuple(args)
        self.kwargs = kwargs

    def __call__(self, caller):
        return self.function(*self.args, **self.kwargs)


class _Result(_Message):
    def __init__(self, value, id):
        super(_Result, self).__init__()
        self.value = value
        self.id = id

    def __call__(self, caller):
        if caller:
            caller.pending_count -= 1
            caller.callbacks.pop(self.id, lambda value: value)(self.value)
        return self.value


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

    def __init__(self, state):
        super(_State, self).__init__()
        self.state = state

    def __call__(self, caller):
        log.info('{} {}'.format(
            self.who, {
                self.STARTING: 'starting',
                self.STOPPING: 'stopping',
            }.get(self.state, self.state),
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
    def main(cls, name, loglevel, setup, setupargs, setupkwargs, queue, teardown, teardownargs, teardownkwargs):
        from tblib import pickling_support

        cls.name = name
        cls.working = True

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
        queue.send(_State(_State.STARTING))

        with OutputCapture.ReplaceSysStream('stderr', cls.Stream(_Print.stderr, queue)), OutputCapture.ReplaceSysStream('stdout', cls.Stream(_Print.stdout, queue)):
            try:
                pickling_support.install()
                if setup:
                    setup(*setupargs, **setupkwargs)

                while cls.working:
                    task = queue.receive()
                    if not task:
                        break
                    queue.send(_Result(value=task(None), id=task.id))

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
                queue.send(_State(_State.STOPPING))
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
    Process = _Process


    class Exception(RuntimeError):
        pass

    def __init__(
        self, workers=1, name=None, setup=None, teardown=None, grace_period=5, block_size=1000,
        setupargs=None, setupkwargs=None,
        teardownargs=None, teardownkwargs=None,
        force_fork=False,
    ):
        # Ensure tblib is installed before creating child processes
        import tblib

        name = name or 'worker'
        if name == self.Process.name:
            raise ValueError("Parent process is already named {}".format(name))
        self.name = name

        if workers < 1:
            raise ValueError('TaskPool requires positive number of workers')

        self.queue = None
        self.workers = []

        self._setup_args = (setup, setupargs or [], setupkwargs or {})
        self._teardown_args = (teardown, teardownargs or [], teardownkwargs or {})
        self._num_workers = int(workers)

        self._started = 0

        self.callbacks = {}
        self._id_count = 0
        self.pending_count = 0
        self.grace_period = grace_period
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

        from mock import patch

        self.queue = self.BiDirectionalQueue()
        self.workers = [multiprocessing.Process(
            target=self.Process.main,
            args=(
                '{}/{}'.format(self.name, count), logging.getLogger().getEffectiveLevel(),
                self._setup_args[0], self._setup_args[1], self._setup_args[2],
                self.BiDirectionalQueue(outgoing=self.queue.incoming, incoming=self.queue.outgoing),
                self._teardown_args[0], self._teardown_args[1], self._teardown_args[2],
            ),
        ) for count in range(self._num_workers)]

        with Timeout(seconds=60, patch=False, handler=self.Exception('Failed to start all workers')):
            for worker in self.workers:
                worker.start()
            while self._started < len(self.workers):
                self.queue.receive()(self)

        return self

    def do(self, function, *args, **kwargs):
        callback = kwargs.pop('callback', None)
        if not self.queue:
            result = function(*args, **kwargs)
            if callback:
                callback(result)
            return

        if callback:
            self.callbacks[self._id_count] = callback
        self.pending_count += 1
        self.queue.send(self.Task(function, self._id_count, *args, **kwargs))
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

            with Timeout(seconds=self.grace_period):
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

                if sys.version_info >= (3, 7):
                    worker.kill()
                # With python2 killing directly the workers causes the queue to hang on close() <https://webkit.org/b/227715>
                elif hasattr(signal, 'SIGKILL') and sys.version_info.major > 2:
                    try:
                        os.kill(worker.pid, signal.SIGKILL)
                    except OSError as e:
                        log.warn('Failed to terminate worker ' + str(worker.pid) + ' with error ' + str(e))
                else:
                    worker.terminate()

            self.queue.close()
            self.queue = None
            self.workers = []
