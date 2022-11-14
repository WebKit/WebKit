# mypy: allow-untyped-defs

import os
from multiprocessing import Queue, Event
from subprocess import PIPE
from threading import Thread
from mozprocess import ProcessHandlerMixin

from . import chrome_spki_certs
from .base import Browser, ExecutorBrowser
from .base import get_timeout_multiplier   # noqa: F401
from ..executors import executor_kwargs as base_executor_kwargs
from ..executors.executorcontentshell import (ContentShellRefTestExecutor,  # noqa: F401
                                              ContentShellCrashtestExecutor,  # noqa: F401
                                              ContentShellTestharnessExecutor)  # noqa: F401


__wptrunner__ = {"product": "content_shell",
                 "check_args": "check_args",
                 "browser": "ContentShellBrowser",
                 "executor": {"reftest": "ContentShellRefTestExecutor",
                     "crashtest": "ContentShellCrashtestExecutor",
                     "testharness": "ContentShellTestharnessExecutor"},
                 "browser_kwargs": "browser_kwargs",
                 "executor_kwargs": "executor_kwargs",
                 "env_extras": "env_extras",
                 "env_options": "env_options",
                 "update_properties": "update_properties",
                 "timeout_multiplier": "get_timeout_multiplier",}


def check_args(**kwargs):
    pass


def browser_kwargs(logger, test_type, run_info_data, config, **kwargs):
    args = kwargs["binary_args"]

    args.append("--ignore-certificate-errors-spki-list=%s" %
        ','.join(chrome_spki_certs.IGNORE_CERTIFICATE_ERRORS_SPKI_LIST))

    webtranport_h3_port = config.ports.get('webtransport-h3')
    if webtranport_h3_port is not None:
        args.append(
            f"--origin-to-force-quic-on=web-platform.test:{webtranport_h3_port[0]}")

    # These flags are specific to content_shell - they activate web test protocol mode.
    args.append("--run-web-tests")
    args.append("-")

    return {"binary": kwargs["binary"],
            "binary_args": args}


def executor_kwargs(logger, test_type, test_environment, run_info_data,
                    **kwargs):
    executor_kwargs = base_executor_kwargs(test_type, test_environment, run_info_data,
                                           **kwargs)
    return executor_kwargs


def env_extras(**kwargs):
    return []


def env_options():
    return {"server_host": "127.0.0.1",
            "testharnessreport": "testharnessreport-content-shell.js"}


def update_properties():
    return (["debug", "os", "processor"], {"os": ["version"], "processor": ["bits"]})


class ContentShellBrowser(Browser):
    """Class that represents an instance of content_shell.

    Upon startup, the stdout, stderr, and stdin pipes of the underlying content_shell
    process are connected to multiprocessing Queues so that the runner process can
    interact with content_shell through its protocol mode.
    """

    def __init__(self, logger, binary="content_shell", binary_args=[], **kwargs):
        super().__init__(logger)

        self._args = [binary] + binary_args
        self._proc = None

    def start(self, group_metadata, **kwargs):
        self.logger.debug("Starting content shell: %s..." % self._args[0])

        # Unfortunately we need to use the Process class directly because we do not
        # want mozprocess to do any output handling at all.
        self._proc = ProcessHandlerMixin.Process(self._args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        if os.name == "posix":
            self._proc.pgid = ProcessHandlerMixin._getpgid(self._proc.pid)
            self._proc.detached_pid = None

        self._stdout_queue = Queue()
        self._stderr_queue = Queue()
        self._stdin_queue = Queue()
        self._io_stopped = Event()

        self._stdout_reader = self._create_reader_thread(self._proc.stdout, self._stdout_queue)
        self._stderr_reader = self._create_reader_thread(self._proc.stderr, self._stderr_queue)
        self._stdin_writer = self._create_writer_thread(self._proc.stdin, self._stdin_queue)

        # Content shell is likely still in the process of initializing. The actual waiting
        # for the startup to finish is done in the ContentShellProtocol.
        self.logger.debug("Content shell has been started.")

    def stop(self, force=False):
        self.logger.debug("Stopping content shell...")

        if self.is_alive():
            kill_result = self._proc.kill(timeout=5)
            # This makes sure any left-over child processes get killed.
            # See http://bugzilla.mozilla.org/show_bug.cgi?id=1760080
            if force and kill_result != 0:
                self._proc.kill(9, timeout=5)

        # We need to shut down these queues cleanly to avoid broken pipe error spam in the logs.
        self._stdout_reader.join(2)
        self._stderr_reader.join(2)

        self._stdin_queue.put(None)
        self._stdin_writer.join(2)

        for thread in [self._stdout_reader, self._stderr_reader, self._stdin_writer]:
            if thread.is_alive():
                self.logger.warning("Content shell IO threads did not shut down gracefully.")
                return False

        stopped = not self.is_alive()
        if stopped:
            self.logger.debug("Content shell has been stopped.")
        else:
            self.logger.warning("Content shell failed to stop.")

        return stopped

    def is_alive(self):
        return self._proc is not None and self._proc.poll() is None

    def pid(self):
        return self._proc.pid if self._proc else None

    def executor_browser(self):
        """This function returns the `ExecutorBrowser` object that is used by other
        processes to interact with content_shell. In our case, this consists of the three
        multiprocessing Queues as well as an `io_stopped` event to signal when the
        underlying pipes have reached EOF.
        """
        return ExecutorBrowser, {"stdout_queue": self._stdout_queue,
                                 "stderr_queue": self._stderr_queue,
                                 "stdin_queue": self._stdin_queue,
                                 "io_stopped": self._io_stopped}

    def check_crash(self, process, test):
        return not self.is_alive()

    def _create_reader_thread(self, stream, queue):
        """This creates (and starts) a background thread which reads lines from `stream` and
        puts them into `queue` until `stream` reports EOF.
        """
        def reader_thread(stream, queue, stop_event):
            while True:
                line = stream.readline()
                if not line:
                    break

                queue.put(line)

            stop_event.set()
            queue.close()
            queue.join_thread()

        result = Thread(target=reader_thread, args=(stream, queue, self._io_stopped), daemon=True)
        result.start()
        return result

    def _create_writer_thread(self, stream, queue):
        """This creates (and starts) a background thread which gets items from `queue` and
        writes them into `stream` until it encounters a None item in the queue.
        """
        def writer_thread(stream, queue):
            while True:
                line = queue.get()
                if not line:
                    break

                stream.write(line)
                stream.flush()

        result = Thread(target=writer_thread, args=(stream, queue), daemon=True)
        result.start()
        return result
