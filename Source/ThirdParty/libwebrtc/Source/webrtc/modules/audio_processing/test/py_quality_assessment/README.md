# APM Quality Assessment tool

Python wrapper of `audioproc_f` with which quality assessment can be
automatized. The tool allows to simulate different noise conditions, input
signals, APM configurations and it computes different scores.
Once the scores are computed, the results can be easily exported to an HTML page
which allows to listen to the APM input and output signals and also the
reference one used for evaluation.

## Dependencies

 - OS: Linux
 - Python 2.7
 - Python libraries: numpy, scipy, pydub (0.17.0+)
 - It is recommended that a dedicated Python environment is used
   - install `virtualenv`
   - `$ sudo apt-get install python-virtualenv`
   - setup a new Python environment (e.g., `my_env`)
   - `$ cd ~ && virtualenv my_env`
   - activate the new Python environment
   - `$ source ~/my_env/bin/activate`
   - add dependcies via `pip`
   - `(my_env)$ pip install numpy pydub scipy`
 - PolqaOem64 (see http://www.polqa.info/)
    - Tested with POLQA Library v1.180 / P863 v2.400
 - Aachen Impulse Response (AIR) Database
    - Download https://www2.iks.rwth-aachen.de/air/air_database_release_1_4.zip
 - Input probing signals and noise tracks (you can make your own dataset - *1)

## Build

 - Compile WebRTC
 - Go to `out/Default/py_quality_assessment` and check that
   `apm_quality_assessment.py` exists

## First time setup

 - Deploy PolqaOem64 and set the `POLQA_PATH` environment variable
   - e.g., `$ export POLQA_PATH=/var/opt/PolqaOem64`
 - Deploy the AIR Database and set the `AECHEN_IR_DATABASE_PATH` environment
 variable
   - e.g., `$ export AECHEN_IR_DATABASE_PATH=/var/opt/AIR_1_4`
 - Deploy probing signal tracks into
   - `out/Default/py_quality_assessment/probing_signals` (*1)
 - Deploy noise tracks into
   - `out/Default/py_quality_assessment/noise_tracks` (*1, *2)

(*1) You can use custom files as long as they are mono tracks sampled at 48kHz
encoded in the 16 bit signed format (it is recommended that the tracks are
converted and exported with Audacity).

(*2) Adapt `EnvironmentalNoiseTestDataGenerator._NOISE_TRACKS` accordingly in
`out/Default/py_quality_assessment/quality_assessment/test_data_generation.py`.

## Usage (scores computation)

 - Go to `out/Default/py_quality_assessment`
 - Check the `apm_quality_assessment.sh` as an example script to parallelize the
   experiments
 - Adjust the script according to your preferences (e.g., output path)
 - Run `apm_quality_assessment.sh`
 - The script will end by opening the browser and showing ALL the computed
   scores

## Usage (export reports)

Showing all the results at once can be confusing. You therefore may want to
export separate reports. In this case, you can use the
`apm_quality_assessment_export.py` script as follows:

 - Set --output_dir to the same value used in `apm_quality_assessment.sh`
 - Use regular expressions to select/filter out scores by
    - APM configurations: `--config_names, -c`
    - probing signals: `--input_names, -i`
    - test data generators: `--test_data_generators, -t`
    - scores: `--eval_scores, -e`
 - Assign a suffix to the report name using `-f <suffix>`

For instance:

```
$ ./apm_quality_assessment-export.py \
  -o ~/data/apm_quality_assessment \
  -e \(polqa\) \
  -n \(echo\) \
  -c "(^default$)|(.*AE.*)" \
  -f echo
```

## Troubleshooting

The input wav file must be:
  - sampled at a sample rate that is a multiple of 100 (required by POLQA)
  - in the 16 bit format (required by `audioproc_f`)
  -  encoded in the Microsoft WAV signed 16 bit PCM format (Audacity default
     when exporting)

Depending on the license, the POLQA tool may take “breaks” as a way to limit the
throughput. When this happens, the APM Quality Assessment tool is slowed down.
For more details about this limitation, check Section 10.9.1 in the POLQA manual
v.1.18.

In case of issues with the POLQA score computation, check
`py_quality_assessment/eval_scores.py` and adapt
`PolqaScore._parse_output_file()`.
The code can be also fixed directly into the build directory (namely,
`out/Default/py_quality_assessment/eval_scores.py`).
