# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Unit tests for the annotations module.
"""

from __future__ import division
import logging
import os
import shutil
import tempfile
import unittest

import numpy as np

from . import annotations
from . import external_vad
from . import input_signal_creator
from . import signal_processing


class TestAnnotationsExtraction(unittest.TestCase):
    """Unit tests for the annotations module.
  """

    _CLEAN_TMP_OUTPUT = True
    _DEBUG_PLOT_VAD = False
    _VAD_TYPE_CLASS = annotations.AudioAnnotationsExtractor.VadType
    _ALL_VAD_TYPES = (_VAD_TYPE_CLASS.ENERGY_THRESHOLD
                      | _VAD_TYPE_CLASS.WEBRTC_COMMON_AUDIO
                      | _VAD_TYPE_CLASS.WEBRTC_APM)

    def setUp(self):
        """Create temporary folder."""
        self._tmp_path = tempfile.mkdtemp()
        self._wav_file_path = os.path.join(self._tmp_path, 'tone.wav')
        pure_tone, _ = input_signal_creator.InputSignalCreator.Create(
            'pure_tone', [440, 1000])
        signal_processing.SignalProcessingUtils.SaveWav(
            self._wav_file_path, pure_tone)
        self._sample_rate = pure_tone.frame_rate

    def tearDown(self):
        """Recursively delete temporary folder."""
        if self._CLEAN_TMP_OUTPUT:
            shutil.rmtree(self._tmp_path)
        else:
            logging.warning(self.id() + ' did not clean the temporary path ' +
                            (self._tmp_path))

    def testFrameSizes(self):
        e = annotations.AudioAnnotationsExtractor(self._ALL_VAD_TYPES)
        e.Extract(self._wav_file_path)
        samples_to_ms = lambda n, sr: 1000 * n // sr
        self.assertEqual(
            samples_to_ms(e.GetLevelFrameSize(), self._sample_rate),
            e.GetLevelFrameSizeMs())
        self.assertEqual(samples_to_ms(e.GetVadFrameSize(), self._sample_rate),
                         e.GetVadFrameSizeMs())

    def testVoiceActivityDetectors(self):
        for vad_type_value in range(0, self._ALL_VAD_TYPES + 1):
            vad_type = self._VAD_TYPE_CLASS(vad_type_value)
            e = annotations.AudioAnnotationsExtractor(vad_type=vad_type_value)
            e.Extract(self._wav_file_path)
            if vad_type.Contains(self._VAD_TYPE_CLASS.ENERGY_THRESHOLD):
                # pylint: disable=unpacking-non-sequence
                vad_output = e.GetVadOutput(
                    self._VAD_TYPE_CLASS.ENERGY_THRESHOLD)
                self.assertGreater(len(vad_output), 0)
                self.assertGreaterEqual(
                    float(np.sum(vad_output)) / len(vad_output), 0.95)

            if vad_type.Contains(self._VAD_TYPE_CLASS.WEBRTC_COMMON_AUDIO):
                # pylint: disable=unpacking-non-sequence
                vad_output = e.GetVadOutput(
                    self._VAD_TYPE_CLASS.WEBRTC_COMMON_AUDIO)
                self.assertGreater(len(vad_output), 0)
                self.assertGreaterEqual(
                    float(np.sum(vad_output)) / len(vad_output), 0.95)

            if vad_type.Contains(self._VAD_TYPE_CLASS.WEBRTC_APM):
                # pylint: disable=unpacking-non-sequence
                (vad_probs,
                 vad_rms) = e.GetVadOutput(self._VAD_TYPE_CLASS.WEBRTC_APM)
                self.assertGreater(len(vad_probs), 0)
                self.assertGreater(len(vad_rms), 0)
                self.assertGreaterEqual(
                    float(np.sum(vad_probs)) / len(vad_probs), 0.5)
                self.assertGreaterEqual(
                    float(np.sum(vad_rms)) / len(vad_rms), 20000)

            if self._DEBUG_PLOT_VAD:
                frame_times_s = lambda num_frames, frame_size_ms: np.arange(
                    num_frames).astype(np.float32) * frame_size_ms / 1000.0
                level = e.GetLevel()
                t_level = frame_times_s(num_frames=len(level),
                                        frame_size_ms=e.GetLevelFrameSizeMs())
                t_vad = frame_times_s(num_frames=len(vad_output),
                                      frame_size_ms=e.GetVadFrameSizeMs())
                import matplotlib.pyplot as plt
                plt.figure()
                plt.hold(True)
                plt.plot(t_level, level)
                plt.plot(t_vad, vad_output * np.max(level), '.')
                plt.show()

    def testSaveLoad(self):
        e = annotations.AudioAnnotationsExtractor(self._ALL_VAD_TYPES)
        e.Extract(self._wav_file_path)
        e.Save(self._tmp_path, "fake-annotation")

        data = np.load(
            os.path.join(
                self._tmp_path,
                e.GetOutputFileNameTemplate().format("fake-annotation")))
        np.testing.assert_array_equal(e.GetLevel(), data['level'])
        self.assertEqual(np.float32, data['level'].dtype)
        np.testing.assert_array_equal(
            e.GetVadOutput(self._VAD_TYPE_CLASS.ENERGY_THRESHOLD),
            data['vad_energy_output'])
        np.testing.assert_array_equal(
            e.GetVadOutput(self._VAD_TYPE_CLASS.WEBRTC_COMMON_AUDIO),
            data['vad_output'])
        np.testing.assert_array_equal(
            e.GetVadOutput(self._VAD_TYPE_CLASS.WEBRTC_APM)[0],
            data['vad_probs'])
        np.testing.assert_array_equal(
            e.GetVadOutput(self._VAD_TYPE_CLASS.WEBRTC_APM)[1],
            data['vad_rms'])
        self.assertEqual(np.uint8, data['vad_energy_output'].dtype)
        self.assertEqual(np.float64, data['vad_probs'].dtype)
        self.assertEqual(np.float64, data['vad_rms'].dtype)

    def testEmptyExternalShouldNotCrash(self):
        for vad_type_value in range(0, self._ALL_VAD_TYPES + 1):
            annotations.AudioAnnotationsExtractor(vad_type_value, {})

    def testFakeExternalSaveLoad(self):
        def FakeExternalFactory():
            return external_vad.ExternalVad(
                os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             'fake_external_vad.py'), 'fake')

        for vad_type_value in range(0, self._ALL_VAD_TYPES + 1):
            e = annotations.AudioAnnotationsExtractor(
                vad_type_value, {'fake': FakeExternalFactory()})
            e.Extract(self._wav_file_path)
            e.Save(self._tmp_path, annotation_name="fake-annotation")
            data = np.load(
                os.path.join(
                    self._tmp_path,
                    e.GetOutputFileNameTemplate().format("fake-annotation")))
            self.assertEqual(np.float32, data['extvad_conf-fake'].dtype)
            np.testing.assert_almost_equal(np.arange(100, dtype=np.float32),
                                           data['extvad_conf-fake'])
