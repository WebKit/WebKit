# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""APM module simulator.
"""

import logging
import os

from . import annotations
from . import data_access
from . import echo_path_simulation
from . import echo_path_simulation_factory
from . import eval_scores
from . import exceptions
from . import input_mixer
from . import input_signal_creator
from . import signal_processing
from . import test_data_generation


class ApmModuleSimulator(object):
    """Audio processing module (APM) simulator class.
  """

    _TEST_DATA_GENERATOR_CLASSES = (
        test_data_generation.TestDataGenerator.REGISTERED_CLASSES)
    _EVAL_SCORE_WORKER_CLASSES = eval_scores.EvaluationScore.REGISTERED_CLASSES

    _PREFIX_APM_CONFIG = 'apmcfg-'
    _PREFIX_CAPTURE = 'capture-'
    _PREFIX_RENDER = 'render-'
    _PREFIX_ECHO_SIMULATOR = 'echosim-'
    _PREFIX_TEST_DATA_GEN = 'datagen-'
    _PREFIX_TEST_DATA_GEN_PARAMS = 'datagen_params-'
    _PREFIX_SCORE = 'score-'

    def __init__(self,
                 test_data_generator_factory,
                 evaluation_score_factory,
                 ap_wrapper,
                 evaluator,
                 external_vads=None):
        if external_vads is None:
            external_vads = {}
        self._test_data_generator_factory = test_data_generator_factory
        self._evaluation_score_factory = evaluation_score_factory
        self._audioproc_wrapper = ap_wrapper
        self._evaluator = evaluator
        self._annotator = annotations.AudioAnnotationsExtractor(
            annotations.AudioAnnotationsExtractor.VadType.ENERGY_THRESHOLD
            | annotations.AudioAnnotationsExtractor.VadType.WEBRTC_COMMON_AUDIO
            | annotations.AudioAnnotationsExtractor.VadType.WEBRTC_APM,
            external_vads)

        # Init.
        self._test_data_generator_factory.SetOutputDirectoryPrefix(
            self._PREFIX_TEST_DATA_GEN_PARAMS)
        self._evaluation_score_factory.SetScoreFilenamePrefix(
            self._PREFIX_SCORE)

        # Properties for each run.
        self._base_output_path = None
        self._output_cache_path = None
        self._test_data_generators = None
        self._evaluation_score_workers = None
        self._config_filepaths = None
        self._capture_input_filepaths = None
        self._render_input_filepaths = None
        self._echo_path_simulator_class = None

    @classmethod
    def GetPrefixApmConfig(cls):
        return cls._PREFIX_APM_CONFIG

    @classmethod
    def GetPrefixCapture(cls):
        return cls._PREFIX_CAPTURE

    @classmethod
    def GetPrefixRender(cls):
        return cls._PREFIX_RENDER

    @classmethod
    def GetPrefixEchoSimulator(cls):
        return cls._PREFIX_ECHO_SIMULATOR

    @classmethod
    def GetPrefixTestDataGenerator(cls):
        return cls._PREFIX_TEST_DATA_GEN

    @classmethod
    def GetPrefixTestDataGeneratorParameters(cls):
        return cls._PREFIX_TEST_DATA_GEN_PARAMS

    @classmethod
    def GetPrefixScore(cls):
        return cls._PREFIX_SCORE

    def Run(self,
            config_filepaths,
            capture_input_filepaths,
            test_data_generator_names,
            eval_score_names,
            output_dir,
            render_input_filepaths=None,
            echo_path_simulator_name=(
                echo_path_simulation.NoEchoPathSimulator.NAME)):
        """Runs the APM simulation.

    Initializes paths and required instances, then runs all the simulations.
    The render input can be optionally added. If added, the number of capture
    input audio tracks and the number of render input audio tracks have to be
    equal. The two lists are used to form pairs of capture and render input.

    Args:
      config_filepaths: set of APM configuration files to test.
      capture_input_filepaths: set of capture input audio track files to test.
      test_data_generator_names: set of test data generator names to test.
      eval_score_names: set of evaluation score names to test.
      output_dir: base path to the output directory for wav files and outcomes.
      render_input_filepaths: set of render input audio track files to test.
      echo_path_simulator_name: name of the echo path simulator to use when
                                render input is provided.
    """
        assert render_input_filepaths is None or (
            len(capture_input_filepaths) == len(render_input_filepaths)), (
                'render input set size not matching input set size')
        assert render_input_filepaths is None or echo_path_simulator_name in (
            echo_path_simulation.EchoPathSimulator.REGISTERED_CLASSES), (
                'invalid echo path simulator')
        self._base_output_path = os.path.abspath(output_dir)

        # Output path used to cache the data shared across simulations.
        self._output_cache_path = os.path.join(self._base_output_path,
                                               '_cache')

        # Instance test data generators.
        self._test_data_generators = [
            self._test_data_generator_factory.GetInstance(
                test_data_generators_class=(
                    self._TEST_DATA_GENERATOR_CLASSES[name]))
            for name in (test_data_generator_names)
        ]

        # Instance evaluation score workers.
        self._evaluation_score_workers = [
            self._evaluation_score_factory.GetInstance(
                evaluation_score_class=self._EVAL_SCORE_WORKER_CLASSES[name])
            for (name) in eval_score_names
        ]

        # Set APM configuration file paths.
        self._config_filepaths = self._CreatePathsCollection(config_filepaths)

        # Set probing signal file paths.
        if render_input_filepaths is None:
            # Capture input only.
            self._capture_input_filepaths = self._CreatePathsCollection(
                capture_input_filepaths)
            self._render_input_filepaths = None
        else:
            # Set both capture and render input signals.
            self._SetTestInputSignalFilePaths(capture_input_filepaths,
                                              render_input_filepaths)

        # Set the echo path simulator class.
        self._echo_path_simulator_class = (
            echo_path_simulation.EchoPathSimulator.
            REGISTERED_CLASSES[echo_path_simulator_name])

        self._SimulateAll()

    def _SimulateAll(self):
        """Runs all the simulations.

    Iterates over the combinations of APM configurations, probing signals, and
    test data generators. This method is mainly responsible for the creation of
    the cache and output directories required in order to call _Simulate().
    """
        without_render_input = self._render_input_filepaths is None

        # Try different APM config files.
        for config_name in self._config_filepaths:
            config_filepath = self._config_filepaths[config_name]

            # Try different capture-render pairs.
            for capture_input_name in self._capture_input_filepaths:
                # Output path for the capture signal annotations.
                capture_annotations_cache_path = os.path.join(
                    self._output_cache_path,
                    self._PREFIX_CAPTURE + capture_input_name)
                data_access.MakeDirectory(capture_annotations_cache_path)

                # Capture.
                capture_input_filepath = self._capture_input_filepaths[
                    capture_input_name]
                if not os.path.exists(capture_input_filepath):
                    # If the input signal file does not exist, try to create using the
                    # available input signal creators.
                    self._CreateInputSignal(capture_input_filepath)
                assert os.path.exists(capture_input_filepath)
                self._ExtractCaptureAnnotations(
                    capture_input_filepath, capture_annotations_cache_path)

                # Render and simulated echo path (optional).
                render_input_filepath = None if without_render_input else (
                    self._render_input_filepaths[capture_input_name])
                render_input_name = '(none)' if without_render_input else (
                    self._ExtractFileName(render_input_filepath))
                echo_path_simulator = (echo_path_simulation_factory.
                                       EchoPathSimulatorFactory.GetInstance(
                                           self._echo_path_simulator_class,
                                           render_input_filepath))

                # Try different test data generators.
                for test_data_generators in self._test_data_generators:
                    logging.info(
                        'APM config preset: <%s>, capture: <%s>, render: <%s>,'
                        'test data generator: <%s>,  echo simulator: <%s>',
                        config_name, capture_input_name, render_input_name,
                        test_data_generators.NAME, echo_path_simulator.NAME)

                    # Output path for the generated test data.
                    test_data_cache_path = os.path.join(
                        capture_annotations_cache_path,
                        self._PREFIX_TEST_DATA_GEN + test_data_generators.NAME)
                    data_access.MakeDirectory(test_data_cache_path)
                    logging.debug('test data cache path: <%s>',
                                  test_data_cache_path)

                    # Output path for the echo simulator and APM input mixer output.
                    echo_test_data_cache_path = os.path.join(
                        test_data_cache_path,
                        'echosim-{}'.format(echo_path_simulator.NAME))
                    data_access.MakeDirectory(echo_test_data_cache_path)
                    logging.debug('echo test data cache path: <%s>',
                                  echo_test_data_cache_path)

                    # Full output path.
                    output_path = os.path.join(
                        self._base_output_path,
                        self._PREFIX_APM_CONFIG + config_name,
                        self._PREFIX_CAPTURE + capture_input_name,
                        self._PREFIX_RENDER + render_input_name,
                        self._PREFIX_ECHO_SIMULATOR + echo_path_simulator.NAME,
                        self._PREFIX_TEST_DATA_GEN + test_data_generators.NAME)
                    data_access.MakeDirectory(output_path)
                    logging.debug('output path: <%s>', output_path)

                    self._Simulate(test_data_generators,
                                   capture_input_filepath,
                                   render_input_filepath, test_data_cache_path,
                                   echo_test_data_cache_path, output_path,
                                   config_filepath, echo_path_simulator)

    @staticmethod
    def _CreateInputSignal(input_signal_filepath):
        """Creates a missing input signal file.

    The file name is parsed to extract input signal creator and params. If a
    creator is matched and the parameters are valid, a new signal is generated
    and written in |input_signal_filepath|.

    Args:
      input_signal_filepath: Path to the input signal audio file to write.

    Raises:
      InputSignalCreatorException
    """
        filename = os.path.splitext(
            os.path.split(input_signal_filepath)[-1])[0]
        filename_parts = filename.split('-')

        if len(filename_parts) < 2:
            raise exceptions.InputSignalCreatorException(
                'Cannot parse input signal file name')

        signal, metadata = input_signal_creator.InputSignalCreator.Create(
            filename_parts[0], filename_parts[1].split('_'))

        signal_processing.SignalProcessingUtils.SaveWav(
            input_signal_filepath, signal)
        data_access.Metadata.SaveFileMetadata(input_signal_filepath, metadata)

    def _ExtractCaptureAnnotations(self,
                                   input_filepath,
                                   output_path,
                                   annotation_name=""):
        self._annotator.Extract(input_filepath)
        self._annotator.Save(output_path, annotation_name)

    def _Simulate(self, test_data_generators, clean_capture_input_filepath,
                  render_input_filepath, test_data_cache_path,
                  echo_test_data_cache_path, output_path, config_filepath,
                  echo_path_simulator):
        """Runs a single set of simulation.

    Simulates a given combination of APM configuration, probing signal, and
    test data generator. It iterates over the test data generator
    internal configurations.

    Args:
      test_data_generators: TestDataGenerator instance.
      clean_capture_input_filepath: capture input audio track file to be
                                    processed by a test data generator and
                                    not affected by echo.
      render_input_filepath: render input audio track file to test.
      test_data_cache_path: path for the generated test audio track files.
      echo_test_data_cache_path: path for the echo simulator.
      output_path: base output path for the test data generator.
      config_filepath: APM configuration file to test.
      echo_path_simulator: EchoPathSimulator instance.
    """
        # Generate pairs of noisy input and reference signal files.
        test_data_generators.Generate(
            input_signal_filepath=clean_capture_input_filepath,
            test_data_cache_path=test_data_cache_path,
            base_output_path=output_path)

        # Extract metadata linked to the clean input file (if any).
        apm_input_metadata = None
        try:
            apm_input_metadata = data_access.Metadata.LoadFileMetadata(
                clean_capture_input_filepath)
        except IOError as e:
            apm_input_metadata = {}
        apm_input_metadata['test_data_gen_name'] = test_data_generators.NAME
        apm_input_metadata['test_data_gen_config'] = None

        # For each test data pair, simulate a call and evaluate.
        for config_name in test_data_generators.config_names:
            logging.info(' - test data generator config: <%s>', config_name)
            apm_input_metadata['test_data_gen_config'] = config_name

            # Paths to the test data generator output.
            # Note that the reference signal does not depend on the render input
            # which is optional.
            noisy_capture_input_filepath = (
                test_data_generators.noisy_signal_filepaths[config_name])
            reference_signal_filepath = (
                test_data_generators.reference_signal_filepaths[config_name])

            # Output path for the evaluation (e.g., APM output file).
            evaluation_output_path = test_data_generators.apm_output_paths[
                config_name]

            # Paths to the APM input signals.
            echo_path_filepath = echo_path_simulator.Simulate(
                echo_test_data_cache_path)
            apm_input_filepath = input_mixer.ApmInputMixer.Mix(
                echo_test_data_cache_path, noisy_capture_input_filepath,
                echo_path_filepath)

            # Extract annotations for the APM input mix.
            apm_input_basepath, apm_input_filename = os.path.split(
                apm_input_filepath)
            self._ExtractCaptureAnnotations(
                apm_input_filepath, apm_input_basepath,
                os.path.splitext(apm_input_filename)[0] + '-')

            # Simulate a call using APM.
            self._audioproc_wrapper.Run(
                config_filepath=config_filepath,
                capture_input_filepath=apm_input_filepath,
                render_input_filepath=render_input_filepath,
                output_path=evaluation_output_path)

            try:
                # Evaluate.
                self._evaluator.Run(
                    evaluation_score_workers=self._evaluation_score_workers,
                    apm_input_metadata=apm_input_metadata,
                    apm_output_filepath=self._audioproc_wrapper.
                    output_filepath,
                    reference_input_filepath=reference_signal_filepath,
                    render_input_filepath=render_input_filepath,
                    output_path=evaluation_output_path,
                )

                # Save simulation metadata.
                data_access.Metadata.SaveAudioTestDataPaths(
                    output_path=evaluation_output_path,
                    clean_capture_input_filepath=clean_capture_input_filepath,
                    echo_free_capture_filepath=noisy_capture_input_filepath,
                    echo_filepath=echo_path_filepath,
                    render_filepath=render_input_filepath,
                    capture_filepath=apm_input_filepath,
                    apm_output_filepath=self._audioproc_wrapper.
                    output_filepath,
                    apm_reference_filepath=reference_signal_filepath,
                    apm_config_filepath=config_filepath,
                )
            except exceptions.EvaluationScoreException as e:
                logging.warning('the evaluation failed: %s', e.message)
                continue

    def _SetTestInputSignalFilePaths(self, capture_input_filepaths,
                                     render_input_filepaths):
        """Sets input and render input file paths collections.

    Pairs the input and render input files by storing the file paths into two
    collections. The key is the file name of the input file.

    Args:
      capture_input_filepaths: list of file paths.
      render_input_filepaths: list of file paths.
    """
        self._capture_input_filepaths = {}
        self._render_input_filepaths = {}
        assert len(capture_input_filepaths) == len(render_input_filepaths)
        for capture_input_filepath, render_input_filepath in zip(
                capture_input_filepaths, render_input_filepaths):
            name = self._ExtractFileName(capture_input_filepath)
            self._capture_input_filepaths[name] = os.path.abspath(
                capture_input_filepath)
            self._render_input_filepaths[name] = os.path.abspath(
                render_input_filepath)

    @classmethod
    def _CreatePathsCollection(cls, filepaths):
        """Creates a collection of file paths.

    Given a list of file paths, makes a collection with one item for each file
    path. The value is absolute path, the key is the file name without
    extenstion.

    Args:
      filepaths: list of file paths.

    Returns:
      A dict.
    """
        filepaths_collection = {}
        for filepath in filepaths:
            name = cls._ExtractFileName(filepath)
            filepaths_collection[name] = os.path.abspath(filepath)
        return filepaths_collection

    @classmethod
    def _ExtractFileName(cls, filepath):
        return os.path.splitext(os.path.split(filepath)[-1])[0]
