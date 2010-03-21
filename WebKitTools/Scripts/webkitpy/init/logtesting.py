# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Supports the unit-testing of logging code.

Provides support for unit-testing messages logged using the built-in
logging module.

Use the LogTesting class for basic testing needs.  For more advanced
needs (e.g. unit-testing methods that configure logging), see the
TestLogStream class.

"""

import logging


class TestLogStream(object):

    """Represents a file-like object for unit-testing logging.

    This is meant for passing to the logging.StreamHandler constructor.
    Log messages captured by instances of this object can be tested
    using self.assertMessages() below.

    """

    def __init__(self, test_case):
        """Create an instance.

        Args:
          test_case: A unittest.TestCase instance.

        """
        self._test_case = test_case
        self.messages = []
        """A list of log messages written to the stream."""

    # Python documentation says that any object passed to the StreamHandler
    # constructor should support write() and flush():
    #
    # http://docs.python.org/library/logging.html#module-logging.handlers
    def write(self, message):
        self.messages.append(message)

    def flush(self):
        pass

    def assertMessages(self, messages):
        """Assert that the given messages match the logged messages.

        messages: A list of log message strings.

        """
        self._test_case.assertEquals(messages, self.messages)


class LogTesting(object):

    """Supports end-to-end unit-testing of log messages.

        Sample usage:

          class SampleTest(unittest.TestCase):

              def setUp(self):
                  self._log = LogTesting.setUp(self)  # Turn logging on.

              def tearDown(self):
                  self._log.tearDown()  # Turn off and reset logging.

              def test_logging_in_some_method(self):
                  call_some_method()  # Contains calls to _log.info(), etc.

                  # Check the resulting log messages.
                  self._log.assertMessages(["INFO: expected message #1",
                                          "WARNING: expected message #2"])

    """

    def __init__(self, test_stream, handler):
        """Create an instance.

        This method should never be called directly.  Instances should
        instead be created using the static setUp() method.

        Args:
          test_stream: A TestLogStream instance.
          handler: The handler added to the logger.

        """
        self._test_stream = test_stream
        self._handler = handler

    @staticmethod
    def _getLogger():
        """Return the logger being tested."""
        # It is possible we might want to return something other than
        # the root logger in some special situation.  For now, the
        # root logger seems to suffice.
        return logging.getLogger()

    @staticmethod
    def setUp(test_case, logging_level=logging.INFO):
        """Configure logging for unit testing.

        Configures the root logger to log to a testing log stream.
        Only messages logged at or above the given level are logged
        to the stream.  Messages logged to the stream are formatted
        in the following way, for example--

        "INFO: This is a test log message."

        This method should normally be called in the setUp() method
        of a unittest.TestCase.  See the docstring of this class
        for more details.

        Returns:
          A LogTesting instance.

        Args:
          test_case: A unittest.TestCase instance.
          logging_level: An integer logging level that is the minimum level
                         of log messages you would like to test.

        """
        stream = TestLogStream(test_case)
        handler = logging.StreamHandler(stream)
        handler.setLevel(logging_level)
        formatter = logging.Formatter("%(levelname)s: %(message)s")
        handler.setFormatter(formatter)

        # Notice that we only change the root logger by adding a handler
        # to it.  In particular, we do not reset its level using
        # logger.setLevel().  This ensures that we have not interfered
        # with how the code being tested may have configured the root
        # logger.
        logger = LogTesting._getLogger()
        logger.addHandler(handler)

        return LogTesting(stream, handler)

    def tearDown(self):
        """Restore logging to its original state.

        This should normally be called in the tearDown() method of a
        unittest.TestCase.  See the docstring of this class for more
        details.

        """
        logger = LogTesting._getLogger()
        logger.removeHandler(self._handler)

    def assertMessages(self, messages):
        """Assert that the given messages match the logged messages.

        messages: A list of log message strings.

        """
        self._test_stream.assertMessages(messages)
