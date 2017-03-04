#!/bin/bash
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Customize probing signals, noise sources and scores if needed.
PROBING_SIGNALS=(probing_signals/*.wav)
NOISE_SOURCES=( \
    "identity" \
    "white" \
    "environmental" \
    "echo" \
)
SCORES=( \
    "polqa" \
    "audio_level" \
)
OUTPUT_PATH=output

# Generate standard APM config files.
chmod +x apm_quality_assessment-gencfgs.py
./apm_quality_assessment-gencfgs.py

# Customize APM configurations if needed.
APM_CONFIGS=(apm_configs/*.json)

# Add output path if missing.
if [ ! -d ${OUTPUT_PATH} ]; then
  mkdir ${OUTPUT_PATH}
fi

# Start one process for each "probing signal"-"noise source" pair.
chmod +x apm_quality_assessment.py
for probing_signal_filepath in "${PROBING_SIGNALS[@]}" ; do
  probing_signal_name="$(basename $probing_signal_filepath)"
  probing_signal_name="${probing_signal_name%.*}"
  for noise_source_name in "${NOISE_SOURCES[@]}" ; do
    LOG_FILE="${OUTPUT_PATH}/apm_qa-${probing_signal_name}-"`
             `"${noise_source_name}.log"
    echo "Starting ${probing_signal_name} ${noise_source_name} "`
         `"(see ${LOG_FILE})"
    ./apm_quality_assessment.py \
        -i ${probing_signal_filepath}\
        -o ${OUTPUT_PATH} \
        -n ${noise_source_name} \
        -c "${APM_CONFIGS[@]}" \
        -e "${SCORES[@]}" > $LOG_FILE 2>&1 &
  done
done

# Join.
wait

# Export results.
chmod +x ./apm_quality_assessment-export.py
./apm_quality_assessment-export.py -o ${OUTPUT_PATH}

# Show results in the browser.
RESULTS_FILE="$(realpath ${OUTPUT_PATH}/results.html)"
sensible-browser "file://${RESULTS_FILE}" > /dev/null 2>&1 &
