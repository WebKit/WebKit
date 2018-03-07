# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit tests for the export module.
"""

import logging
import os
import shutil
import tempfile
import unittest

import pyquery as pq

from . import audioproc_wrapper
from . import collect_data
from . import eval_scores_factory
from . import evaluation
from . import export
from . import simulation
from . import test_data_generation_factory


class TestExport(unittest.TestCase):
  """Unit tests for the export module.
  """

  _CLEAN_TMP_OUTPUT = True

  def setUp(self):
    """Creates temporary data to export."""
    self._tmp_path = tempfile.mkdtemp()

    # Run a fake experiment to produce data to export.
    simulator = simulation.ApmModuleSimulator(
        test_data_generator_factory=(
            test_data_generation_factory.TestDataGeneratorFactory(
                aechen_ir_database_path='',
                noise_tracks_path='',
                copy_with_identity=False)),
        evaluation_score_factory=(
          eval_scores_factory.EvaluationScoreWorkerFactory(
              polqa_tool_bin_path=os.path.join(
                  os.path.dirname(os.path.abspath(__file__)), 'fake_polqa'))),
        ap_wrapper=audioproc_wrapper.AudioProcWrapper(
            audioproc_wrapper.AudioProcWrapper.DEFAULT_APM_SIMULATOR_BIN_PATH),
        evaluator=evaluation.ApmModuleEvaluator())
    simulator.Run(
        config_filepaths=['apm_configs/default.json'],
        capture_input_filepaths=[
            os.path.join(self._tmp_path, 'pure_tone-440_1000.wav'),
            os.path.join(self._tmp_path, 'pure_tone-880_1000.wav'),
        ],
        test_data_generator_names=['identity', 'white_noise'],
        eval_score_names=['audio_level_peak', 'audio_level_mean'],
        output_dir=self._tmp_path)

    # Export results.
    p = collect_data.InstanceArgumentsParser()
    args = p.parse_args(['--output_dir', self._tmp_path])
    src_path = collect_data.ConstructSrcPath(args)
    self._data_to_export = collect_data.FindScores(src_path, args)

  def tearDown(self):
    """Recursively deletes temporary folders."""
    if self._CLEAN_TMP_OUTPUT:
      shutil.rmtree(self._tmp_path)
    else:
      logging.warning(self.id() + ' did not clean the temporary path ' + (
          self._tmp_path))

  def testCreateHtmlReport(self):
    fn_out = os.path.join(self._tmp_path, 'results.html')
    exporter = export.HtmlExport(fn_out)
    exporter.Export(self._data_to_export)

    document = pq.PyQuery(filename=fn_out)
    self.assertIsInstance(document, pq.PyQuery)
    # TODO(alessiob): Use PyQuery API to check the HTML file.
