# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import logging
import os
import re


class HtmlExport(object):
  """HTML exporter class for APM quality scores.
  """

  # Path to CSS and JS files.
  _PATH = os.path.dirname(os.path.realpath(__file__))

  # CSS file parameters.
  _CSS_FILEPATH = os.path.join(_PATH, 'results.css')
  _INLINE_CSS = False

  # JS file parameters.
  _JS_FILEPATH = os.path.join(_PATH, 'results.js')
  _INLINE_JS = False

  _NEW_LINE = '\n'

  def __init__(self, output_filepath):
    self._test_data_generator_names = None
    self._test_data_generator_params = None
    self._output_filepath = output_filepath

  def Export(self, scores):
    """Exports the scores into an HTML file.

    Args:
      scores: nested dictionary containing the scores.
    """
    # Generate one table for each evaluation score.
    tables = []
    for score_name in sorted(scores.keys()):
      tables.append(self._BuildScoreTable(score_name, scores[score_name]))

    # Create the html file.
    html = (
        '<html>' +
        self._BuildHeader() +
        '<body onload="initialize()">' +
        '<h1>Results from {}</h1>'.format(self._output_filepath) +
        self._NEW_LINE.join(tables) +
        '</body>' +
        '</html>')

    self._Save(self._output_filepath, html)

  def _BuildHeader(self):
    """Builds the <head> section of the HTML file.

    The header contains the page title and either embedded or linked CSS and JS
    files.

    Returns:
      A string with <head>...</head> HTML.
    """
    html = ['<head>', '<title>Results</title>']

    # Function to append the lines of a text file to html.
    def EmbedFile(filepath):
      with open(filepath) as f:
        for l in f:
          html.append(l.strip())

    # CSS.
    if self._INLINE_CSS:
      # Embed.
      html.append('<style>')
      EmbedFile(self._CSS_FILEPATH)
      html.append('</style>')
    else:
      # Link.
      html.append('<link rel="stylesheet" type="text/css" '
                  'href="file://{}?">'.format(self._CSS_FILEPATH))

    # Javascript.
    if self._INLINE_JS:
      # Embed.
      html.append('<script>')
      EmbedFile(self._JS_FILEPATH)
      html.append('</script>')
    else:
      # Link.
      html.append('<script src="file://{}?"></script>'.format(
          self._JS_FILEPATH))

    html.append('</head>')

    return self._NEW_LINE.join(html)

  def _BuildScoreTable(self, score_name, scores):
    """Builds a table for a specific evaluation score (e.g., POLQA).

    Args:
      score_name: name of the score.
      scores: nested dictionary of scores.

    Returns:
      A string with <table>...</table> HTML.
    """
    config_names = sorted(scores.keys())
    input_names = sorted(scores[config_names[0]].keys())
    rows = [self._BuildTableRow(
        score_name, config_name, scores[config_name], input_names) for (
            config_name) in config_names]

    html = (
        '<table celpadding="0" cellspacing="0">' +
        '<thead><tr>{}</tr></thead>'.format(
            self._BuildTableHeader(score_name, input_names)) +
        '<tbody>' +
        '<tr>' + '</tr><tr>'.join(rows) + '</tr>' +
        '</tbody>' +
        '</table>' + self._BuildLegend())

    return html

  def _BuildTableHeader(self, score_name, input_names):
    """Builds the cells of a table header.

    A table header starts with a cell containing the name of the evaluation
    score, and then it includes one column for each probing signal.

    Args:
      score_name: name of the score.
      input_names: list of probing signal names.

    Returns:
      A string with a list of <th>...</th> HTML elements.
    """
    html = (
        '<th>{}</th>'.format(self._FormatName(score_name)) +
        '<th>' + '</th><th>'.join(
          [self._FormatName(name) for name in input_names]) + '</th>')
    return html

  def _BuildTableRow(self, score_name, config_name, scores, input_names):
    """Builds the cells of a table row.

    A table row starts with the name of the APM configuration file, and then it
    includes one column for each probing singal.

    Args:
      score_name: name of the score.
      config_name: name of the APM configuration.
      scores: nested dictionary of scores.
      input_names: list of probing signal names.

    Returns:
      A string with a list of <td>...</td> HTML elements.
    """
    cells = [self._BuildTableCell(
        scores[input_name], score_name, config_name, input_name) for (
            input_name) in input_names]
    html = ('<td>{}</td>'.format(self._FormatName(config_name)) +
            '<td>' + '</td><td>'.join(cells) + '</td>')
    return html

  def _BuildTableCell(self, scores, score_name, config_name, input_name):
    """Builds the inner content of a table cell.

    A table cell includes all the scores computed for a specific evaluation
    score (e.g., POLQA), APM configuration (e.g., default), and probing signal.

    Args:
      scores: dictionary of score data.
      score_name: name of the score.
      config_name: name of the APM configuration.
      input_name: name of the probing signal.

    Returns:
      A string with the HTML of a table body cell.
    """
    # Init test data generator names and parameters cache (if not done).
    if self._test_data_generator_names is None:
      self._test_data_generator_names = sorted(scores.keys())
      self._test_data_generator_params = {test_data_generator_name: sorted(
          scores[test_data_generator_name].keys()) for (
              test_data_generator_name) in self._test_data_generator_names}

    # For each noisy input (that is a pair of test data generator and
    # generator parameters), add an item with the score and its metadata.
    items = []
    for name_index, test_data_generator_name in enumerate(
        self._test_data_generator_names):
      for params_index, test_data_generator_params in enumerate(
          self._test_data_generator_params[test_data_generator_name]):

        # Init.
        score_value = '?'
        metadata = ''

        # Extract score value and its metadata.
        try:
          data = scores[test_data_generator_name][test_data_generator_params]
          score_value = '{0:f}'.format(data['score'])
          metadata = (
              '<input type="hidden" name="gen_name" value="{}"/>'
              '<input type="hidden" name="gen_params" value="{}"/>'
              '<input type="hidden" name="audio_in" value="file://{}"/>'
              '<input type="hidden" name="audio_out" value="file://{}"/>'
              '<input type="hidden" name="audio_ref" value="file://{}"/>'
          ).format(
              test_data_generator_name,
              test_data_generator_params,
              data['audio_in_filepath'],
              data['audio_out_filepath'],
              data['audio_ref_filepath'])
        except TypeError:
          logging.warning(
              'missing score found: <score:%s> <config:%s> <input:%s> '
              '<generator:%s> <params:%s>', score_name, config_name, input_name,
              test_data_generator_name, test_data_generator_params)

        # Add the score.
        items.append(
            '<div class="test-data-gen-desc">[{0:d}, {1:d}]{2}</div>'
            '<div class="value">{3}</div>'.format(
                name_index, params_index, metadata, score_value))

    html = (
        '<div class="score">' +
        '</div><div class="score">'.join(items) +
        '</div>')

    return html

  def _BuildLegend(self):
    """Builds the legend.

    The legend details test data generator name and parameter pairs.

    Returns:
      A string with a <div class="legend">...</div> HTML element.
    """
    items = []
    for name_index, test_data_generator_name in enumerate(
        self._test_data_generator_names):
      for params_index, test_data_generator_params in enumerate(
          self._test_data_generator_params[test_data_generator_name]):
        items.append(
            '<div class="test-data-gen-desc">[{0:d}, {1:d}]</div>: {2}, '
            '{3}'.format(name_index, params_index, test_data_generator_name,
                         test_data_generator_params))
    html = (
        '<div class="legend"><div>' +
        '</div><div>'.join(items) + '</div></div>')

    return html

  @classmethod
  def _Save(cls, output_filepath, html):
    """Writes the HTML file.

    Args:
      output_filepath: output file path.
      html: string with the HTML content.
    """
    with open(output_filepath, 'w') as f:
      f.write(html)

  @classmethod
  def _FormatName(cls, name):
    """Formats a name.

    Args:
      name: a string.

    Returns:
      A copy of name in which underscores and dashes are replaced with a space.
    """
    return re.sub(r'[_\-]', ' ', name)
