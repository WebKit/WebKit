#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Export the scores computed by the apm_quality_assessment.py script into an
   HTML file.
"""

import logging
import os
import sys

import quality_assessment.collect_data as collect_data
import quality_assessment.export as export


def _BuildOutputFilename(filename_suffix):
    """Builds the filename for the exported file.

  Args:
    filename_suffix: suffix for the output file name.

  Returns:
    A string.
  """
    if filename_suffix is None:
        return 'results.html'
    return 'results-{}.html'.format(filename_suffix)


def main():
    # Init.
    logging.basicConfig(
        level=logging.DEBUG)  # TODO(alessio): INFO once debugged.
    parser = collect_data.InstanceArgumentsParser()
    parser.add_argument('-f',
                        '--filename_suffix',
                        help=('suffix of the exported file'))
    parser.description = ('Exports pre-computed APM module quality assessment '
                          'results into HTML tables')
    args = parser.parse_args()

    # Get the scores.
    src_path = collect_data.ConstructSrcPath(args)
    logging.debug(src_path)
    scores_data_frame = collect_data.FindScores(src_path, args)

    # Export.
    output_filepath = os.path.join(args.output_dir,
                                   _BuildOutputFilename(args.filename_suffix))
    exporter = export.HtmlExport(output_filepath)
    exporter.Export(scores_data_frame)

    logging.info('output file successfully written in %s', output_filepath)
    sys.exit(0)


if __name__ == '__main__':
    main()
