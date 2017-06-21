#!/bin/bash
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Path to the POLQA tool.
if [ -z ${POLQA_PATH} ]; then  # Check if defined.
  # Default location.
  export POLQA_PATH='/var/opt/PolqaOem64'
fi
if [ -d "${POLQA_PATH}" ]; then
  echo "POLQA found in ${POLQA_PATH}"
else
  echo "POLQA not found in ${POLQA_PATH}"
  exit 1
fi

# Path to the Aechen IR database.
if [ -z ${AECHEN_IR_DATABASE_PATH} ]; then  # Check if defined.
  # Default location.
  export AECHEN_IR_DATABASE_PATH='/var/opt/AIR_1_4'
fi
if [ -d "${AECHEN_IR_DATABASE_PATH}" ]; then
  echo "AIR database found in ${AECHEN_IR_DATABASE_PATH}"
else
  echo "AIR database not found in ${AECHEN_IR_DATABASE_PATH}"
  exit 1
fi

# Customize probing signals, test data generators and scores if needed.
PROBING_SIGNALS=(probing_signals/*.wav)
TEST_DATA_GENERATORS=( \
    "identity" \
    "white_noise" \
    "environmental_noise" \
    "reverberation" \
)
SCORES=( \
    "polqa" \
    "audio_level" \
)
OUTPUT_PATH=output

# Generate standard APM config files.
chmod +x apm_quality_assessment_gencfgs.py
./apm_quality_assessment_gencfgs.py

# Customize APM configurations if needed.
APM_CONFIGS=(apm_configs/*.json)

# Add output path if missing.
if [ ! -d ${OUTPUT_PATH} ]; then
  mkdir ${OUTPUT_PATH}
fi

# Start one process for each "probing signal"-"test data source" pair.
chmod +x apm_quality_assessment.py
for probing_signal_filepath in "${PROBING_SIGNALS[@]}" ; do
  probing_signal_name="$(basename $probing_signal_filepath)"
  probing_signal_name="${probing_signal_name%.*}"
  for test_data_gen_name in "${TEST_DATA_GENERATORS[@]}" ; do
    LOG_FILE="${OUTPUT_PATH}/apm_qa-${probing_signal_name}-"`
             `"${test_data_gen_name}.log"
    echo "Starting ${probing_signal_name} ${test_data_gen_name} "`
         `"(see ${LOG_FILE})"
    ./apm_quality_assessment.py \
        --polqa_path ${POLQA_PATH}\
        --air_db_path ${AECHEN_IR_DATABASE_PATH}\
        -i ${probing_signal_filepath} \
        -o ${OUTPUT_PATH} \
        -t ${test_data_gen_name} \
        -c "${APM_CONFIGS[@]}" \
        -e "${SCORES[@]}" > $LOG_FILE 2>&1 &
  done
done

# Join.
wait

# Export results.
chmod +x ./apm_quality_assessment_export.py
./apm_quality_assessment_export.py -o ${OUTPUT_PATH}

# Show results in the browser.
RESULTS_FILE="$(realpath ${OUTPUT_PATH}/results.html)"
sensible-browser "file://${RESULTS_FILE}" > /dev/null 2>&1 &
