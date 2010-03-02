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

"""Supports the unit-testing of logging."""

import logging


class UnitTestLogStream(object):

    """Represents a file-like object for unit-testing logging.

    This is meant for passing to the logging.StreamHandler constructor.

    """

    def __init__(self):
        self.messages = []
        """A list of log messages written to the stream."""

    def write(self, message):
        self.messages.append(message)

    def flush(self):
        pass


class UnitTestLog(object):

    """Supports unit-testing logging.

    Sample usage:

    # The following line should go in the setUp() method of a
    # unittest.TestCase.  In particular, self is a TestCase instance.
    self._log = UnitTestLog(self)

    # Call the following in a test method of a unittest.TestCase.
    log = logging.getLogger("webkitpy")
    log.info("message1")
    log.warn("message2")
    self._log.assertMessages(["INFO: message1\n",
                              "WARN: message2\n"])  # Should succeed.

    # The following line should go in the tearDown() method of
    # unittest.TestCase.
    self._log.tearDown()

    """

    def __init__(self, test_case, logging_level=None):
        """Configure unit test logging, and return an instance.

        This method configures the root logger to log to a testing
        log stream.  Only messages logged at or above the given level
        are logged to the stream.  Messages are formatted in the
        following way, for example--

        "INFO: This is a test log message."

        This method should normally be called in the setUp() method
        of a unittest.TestCase.

        Args:
          test_case: A unittest.TestCase instance.
          logging_level: A logging level.  Only messages logged at or
                         above this level are written to the testing
                         log stream.  Defaults to logging.INFO.

        """
        if logging_level is None:
            logging_level = logging.INFO

        stream = UnitTestLogStream()
        handler = logging.StreamHandler(stream)
        formatter = logging.Formatter("%(levelname)s: %(message)s")
        handler.setFormatter(formatter)

        logger = logging.getLogger()
        logger.setLevel(logging_level)
        logger.addHandler(handler)

        self._handler = handler
        self._messages = stream.messages
        self._test_case = test_case

    def _logger(self):
        """Return the root logger used for logging."""
        return logging.getLogger()

    def assertMessages(self, messages):
        """Assert that the given messages match the logged messages."""
        self._test_case.assertEquals(messages, self._messages)

    def tearDown(self):
        """Unconfigure unit test logging.

        This should normally be called in the tearDown method of a
        unittest.TestCase

        """
        logger = self._logger()
        logger.removeHandler(self._handler)
