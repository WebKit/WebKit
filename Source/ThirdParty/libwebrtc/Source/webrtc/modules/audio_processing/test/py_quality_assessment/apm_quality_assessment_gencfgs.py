#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Generate .json files with which the APM module can be tested using the
   apm_quality_assessment.py script.
"""

import logging
import os

import quality_assessment.data_access as data_access

OUTPUT_PATH = os.path.abspath('apm_configs')


def _GenerateDefaultOverridden(config_override):
  """Generates one or more APM overriden configurations.

  For each item in config_override, it overrides the default configuration and
  writes a new APM configuration file.

  The default settings are loaded via "-all_default".
  Check "src/webrtc/modules/audio_processing/test/audioproc_float.cc" and search
  for "if (FLAGS_all_default) {".

  For instance, in 55eb6d621489730084927868fed195d3645a9ec9 the default is this:
  settings.use_aec = rtc::Optional<bool>(true);
  settings.use_aecm = rtc::Optional<bool>(false);
  settings.use_agc = rtc::Optional<bool>(true);
  settings.use_bf = rtc::Optional<bool>(false);
  settings.use_ed = rtc::Optional<bool>(false);
  settings.use_hpf = rtc::Optional<bool>(true);
  settings.use_ie = rtc::Optional<bool>(false);
  settings.use_le = rtc::Optional<bool>(true);
  settings.use_ns = rtc::Optional<bool>(true);
  settings.use_ts = rtc::Optional<bool>(true);
  settings.use_vad = rtc::Optional<bool>(true);

  Args:
    config_override: dict of APM configuration file names as keys; the values
                     are dict instances encoding the audioproc_f flags.
  """
  for config_filename in config_override:
    config = config_override[config_filename]
    config['-all_default'] = None

    config_filepath = os.path.join(OUTPUT_PATH, 'default-{}.json'.format(
        config_filename))
    logging.debug('config file <%s> | %s', config_filepath, config)

    data_access.AudioProcConfigFile.Save(config_filepath, config)
    logging.info('config file created: <%s>', config_filepath)


def _GenerateAllDefaultButOne():
  """Disables the flags enabled by default one-by-one.
  """
  config_sets = {
      'no_AEC': {'-aec': 0,},
      'no_AGC': {'-agc': 0,},
      'no_HP_filter': {'-hpf': 0,},
      'no_level_estimator': {'-le': 0,},
      'no_noise_suppressor': {'-ns': 0,},
      'no_transient_suppressor': {'-ts': 0,},
      'no_vad': {'-vad': 0,},
  }
  _GenerateDefaultOverridden(config_sets)


def _GenerateAllDefaultPlusOne():
  """Enables the flags disabled by default one-by-one.
  """
  config_sets = {
      'with_AECM': {'-aec': 0, '-aecm': 1,},  # AEC and AECM are exclusive.
      'with_AGC_limiter': {'-agc_limiter': 1,},
      'with_AEC_delay_agnostic': {'-delay_agnostic': 1,},
      'with_drift_compensation': {'-drift_compensation': 1,},
      'with_residual_echo_detector': {'-ed': 1,},
      'with_AEC_extended_filter': {'-extended_filter': 1,},
      'with_intelligibility_enhancer': {'-ie': 1,},
      'with_LC': {'-lc': 1,},
      'with_refined_adaptive_filter': {'-refined_adaptive_filter': 1,},
  }
  _GenerateDefaultOverridden(config_sets)


def main():
  logging.basicConfig(level=logging.INFO)
  _GenerateAllDefaultPlusOne()
  _GenerateAllDefaultButOne()


if __name__ == '__main__':
  main()
