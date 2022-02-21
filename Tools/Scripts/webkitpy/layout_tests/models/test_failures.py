# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.layout_tests.models import test_expectations

_log = logging.getLogger(__name__)


def is_reftest_failure(failure_list):
    failure_types = [type(f) for f in failure_list]
    return set((FailureReftestMismatch, FailureReftestMismatchDidNotOccur, FailureReftestNoImagesGenerated)).intersection(failure_types)


# FIXME: This is backwards.  Each TestFailure subclass should know what
# test_expectation type it corresponds too.  Then this method just
# collects them all from the failure list and returns the worst one.
def determine_result_type(failure_list):
    """Takes a set of test_failures and returns which result type best fits
    the list of failures. "Best fits" means we use the worst type of failure.

    Returns:
      one of the test_expectations result types - PASS, FAIL, CRASH, etc."""

    if not failure_list or len(failure_list) == 0:
        return test_expectations.PASS

    failure_types = [type(f) for f in failure_list]
    if FailureCrash in failure_types:
        return test_expectations.CRASH
    elif FailureTimeout in failure_types:
        return test_expectations.TIMEOUT
    elif FailureEarlyExit in failure_types:
        return test_expectations.SKIP
    elif FailureDocumentLeak in failure_types:
        return test_expectations.LEAK
    elif (FailureMissingResult in failure_types or
          FailureMissingImage in failure_types or
          FailureMissingImageHash in failure_types or
          FailureMissingAudio in failure_types or
          FailureNoOutput in failure_types):
        return test_expectations.MISSING
    else:
        is_text_failure = FailureTextMismatch in failure_types
        is_image_failure = (FailureImageHashIncorrect in failure_types or
                            FailureImageHashMismatch in failure_types)
        is_audio_failure = (FailureAudioMismatch in failure_types)
        if is_text_failure and is_image_failure:
            return test_expectations.IMAGE_PLUS_TEXT
        elif is_text_failure:
            return test_expectations.TEXT
        elif is_image_failure or is_reftest_failure(failure_list):
            return test_expectations.IMAGE
        elif is_audio_failure:
            return test_expectations.AUDIO
        else:
            raise ValueError("unclassifiable set of failures: "
                             + str(failure_types))


class TestFailure(object):
    """Abstract base class that defines the failure interface."""

    def message(self):
        """Returns a string describing the failure in more detail."""
        raise NotImplementedError

    def __eq__(self, other):
        return self.__class__.__name__ == other.__class__.__name__

    def __ne__(self, other):
        return self.__class__.__name__ != other.__class__.__name__

    def __hash__(self):
        return hash(self.__class__.__name__)

    def driver_needs_restart(self):
        """Returns True if we should kill DumpRenderTree/WebKitTestRunner before the next test."""
        return False

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        assert isinstance(self, (FailureTimeout, FailureReftestNoImagesGenerated))


class FailureText(TestFailure):
    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_text_files(driver_output.text, expected_driver_output.text)
        writer.create_text_diff_and_write_result(driver_output.text, expected_driver_output.text)


class FailureAudio(TestFailure):
    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_audio_files(driver_output.audio, expected_driver_output.audio)
        writer.create_audio_diff_and_write_result(driver_output.audio, expected_driver_output.audio)


class FailureTimeout(TestFailure):
    def __init__(self, is_reftest=False):
        super(FailureTimeout, self).__init__()
        self.is_reftest = is_reftest

    def message(self):
        return "test timed out"

    def driver_needs_restart(self):
        return True


class FailureCrash(TestFailure):
    def __init__(self, is_reftest=False, process_name='DumpRenderTree', pid=None):
        super(FailureCrash, self).__init__()
        self.process_name = process_name
        self.pid = pid
        self.is_reftest = is_reftest

    def message(self):
        if self.pid:
            return "%s crashed [pid=%d]" % (self.process_name, self.pid)
        return self.process_name + " crashed"

    def driver_needs_restart(self):
        return True

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        crashed_driver_output = expected_driver_output if self.is_reftest else driver_output
        writer.write_crash_log(crashed_driver_output.crash_log)


class FailureLeak(TestFailure):
    def __init__(self):
        super(FailureLeak, self).__init__()

    def message(self):
        return "leak"


class FailureDocumentLeak(FailureLeak):
    def __init__(self, leaked_document_urls=None):
        super(FailureDocumentLeak, self).__init__()
        self.leaked_document_urls = leaked_document_urls

    def message(self):
        return "test leaked document%s %s" % ("s" if len(self.leaked_document_urls) else "", ', '.join(self.leaked_document_urls))


class FailureMissingResult(FailureText):
    def message(self):
        return "-expected.txt was missing"


class FailureNoOutput(FailureText):
    def message(self):
        return 'no output from test'


class FailureTextMismatch(FailureText):
    def message(self):
        return "text diff"


class FailureMissingImageHash(TestFailure):
    def message(self):
        return "-expected.png was missing an embedded checksum"

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_image_files(driver_output.image, expected_driver_output.image)


class FailureMissingImage(TestFailure):
    def message(self):
        return "-expected.png was missing"

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_image_files(driver_output.image, expected_image=None)


class FailureImageHashMismatch(TestFailure):
    def __init__(self, image_diff_result=None):
        super(FailureImageHashMismatch, self).__init__()
        self.image_diff_result = image_diff_result

    def message(self):
        return "image diff"

    def formatted_diff_percent(self):
        return '{:.2f}%'.format(max(self.image_diff_result.diff_percent, 0.01))

    def formatted_fuzzy_data(self):
        fuzzy_data = self.image_diff_result.fuzzy_data if self.image_diff_result else None
        if fuzzy_data:
            return 'maxDifference={}; totalPixels={}'.format(fuzzy_data['max_difference'], fuzzy_data['total_pixels'])
        return None

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_image_files(driver_output.image, expected_driver_output.image)
        writer.write_image_diff_files(driver_output.image_diff, self.formatted_diff_percent(), self.formatted_fuzzy_data())


class FailureImageHashIncorrect(TestFailure):
    def message(self):
        return "-expected.png embedded checksum is incorrect"


class FailureReftestMismatch(TestFailure):
    def __init__(self, reference_filename=None, image_diff_result=None):
        super(FailureReftestMismatch, self).__init__()
        self.reference_filename = reference_filename
        self.image_diff_result = image_diff_result

    def message(self):
        fuzzy_data = self.image_diff_result.fuzzy_data if self.image_diff_result else None
        if fuzzy_data:
            return "reference mismatch maxDifference={}; totalPixels={}".format(fuzzy_data['max_difference'], fuzzy_data['total_pixels'])
        return "reference mismatch"

    def formatted_diff_percent(self):
        return '{:.2f}%'.format(max(self.image_diff_result.diff_percent, 0.01))

    def formatted_fuzzy_data(self):
        fuzzy_data = self.image_diff_result.fuzzy_data if self.image_diff_result else None
        if fuzzy_data:
            return 'maxDifference={}; totalPixels={}'.format(fuzzy_data['max_difference'], fuzzy_data['total_pixels'])
        return None

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_image_files(driver_output.image, expected_driver_output.image)
        if self.image_diff_result:
            # If the ref test was run with non-zero tolerance, generate the image diff again with zero tolerance.
            if self.image_diff_result.tolerance != 0:
                diff_image = port.diff_image(expected_driver_output.image, driver_output.image, tolerance=0).diff_image
            else:
                diff_image = self.image_diff_result.diff_image

            writer.write_image_diff_files(diff_image, self.formatted_diff_percent(), self.formatted_fuzzy_data())
        else:
            _log.warn('ref test mismatch did not produce an image diff.')
        writer.write_reftest(self.reference_filename)


class FailureReftestMismatchDidNotOccur(TestFailure):
    def __init__(self, reference_filename=None):
        super(FailureReftestMismatchDidNotOccur, self).__init__()
        self.reference_filename = reference_filename

    def message(self):
        return "reference mismatch didn't happen"

    def write_failure(self, writer, driver_output, expected_driver_output, port):
        writer.write_image_files(driver_output.image, expected_image=None)
        writer.write_reftest(self.reference_filename)


class FailureReftestNoImagesGenerated(TestFailure):
    def __init__(self, reference_filename=None):
        super(FailureReftestNoImagesGenerated, self).__init__()
        self.reference_filename = reference_filename

    def message(self):
        return "reference didn't generate pixel results."


class FailureMissingAudio(FailureAudio):
    def message(self):
        return "expected audio result was missing"


class FailureAudioMismatch(FailureAudio):
    def message(self):
        return "audio mismatch"


class FailureEarlyExit(TestFailure):
    def message(self):
        return "skipped due to early exit"


# Convenient collection of all failure classes for anything that might
# need to enumerate over them all.
ALL_FAILURE_CLASSES = (FailureTimeout, FailureCrash, FailureMissingResult, FailureNoOutput,
                       FailureTextMismatch, FailureMissingImageHash,
                       FailureMissingImage, FailureImageHashMismatch,
                       FailureImageHashIncorrect, FailureReftestMismatch,
                       FailureReftestMismatchDidNotOccur, FailureReftestNoImagesGenerated,
                       FailureMissingAudio, FailureAudioMismatch, FailureDocumentLeak,
                       FailureEarlyExit)
