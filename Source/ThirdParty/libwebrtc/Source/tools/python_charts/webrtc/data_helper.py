#!/usr/bin/env python
#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

class DataHelper(object):
  """
  Helper class for managing table data.
  This class does not verify the consistency of the data tables sent into it.
  """

  def __init__(self, data_list, table_description, names_list, messages):
    """ Initializes the DataHelper with data.

    Args:
      data_list: List of one or more data lists in the format that the
        Google Visualization Python API expects (list of dictionaries, one
        per row of data). See the gviz_api.DataTable documentation for more
        info.
      table_description: dictionary describing the data types of all
        columns in the data lists, as defined in the gviz_api.DataTable
        documentation.
      names_list: List of strings of what we're going to name the data
        columns after. Usually different runs of data collection.
      messages: List of strings we might append error messages to.
    """
    self.data_list = data_list
    self.table_description = table_description
    self.names_list = names_list
    self.messages = messages
    self.number_of_datasets = len(data_list)
    self.number_of_frames = len(data_list[0])

  def CreateData(self, field_name, start_frame=0, end_frame=0):
    """ Creates a data structure for a specified data field.

    Creates a data structure (data type description dictionary and a list
    of data dictionaries) to be used with the Google Visualization Python
    API. The frame_number column is always present and one column per data
    set is added and its field name is suffixed by _N where N is the number
    of the data set (0, 1, 2...)

    Args:
      field_name: String name of the field, must be present in the data
        structure this DataHelper was created with.
      start_frame: Frame number to start at (zero indexed). Default: 0.
      end_frame: Frame number to be the last frame. If zero all frames
        will be included. Default: 0.

    Returns:
      A tuple containing:
      - a dictionary describing the columns in the data result_data_table below.
        This description uses the name for each data set specified by
        names_list.

        Example with two data sets named 'Foreman' and 'Crew':
        {
         'frame_number': ('number', 'Frame number'),
         'ssim_0': ('number', 'Foreman'),
         'ssim_1': ('number', 'Crew'),
         }
      - a list containing dictionaries (one per row) with the frame_number
        column and one column of the specified field_name column per data
        set.

        Example with two data sets named 'Foreman' and 'Crew':
        [
         {'frame_number': 0, 'ssim_0': 0.98, 'ssim_1': 0.77 },
         {'frame_number': 1, 'ssim_0': 0.81, 'ssim_1': 0.53 },
        ]
    """

    # Build dictionary that describes the data types
    result_table_description = {'frame_number': ('string', 'Frame number')}
    for dataset_index in range(self.number_of_datasets):
      column_name = '%s_%s' % (field_name, dataset_index)
      column_type = self.table_description[field_name][0]
      column_description = self.names_list[dataset_index]
      result_table_description[column_name] = (column_type, column_description)

    # Build data table of all the data
    result_data_table = []
    # We're going to have one dictionary per row.
    # Create that and copy frame_number values from the first data set
    for source_row in self.data_list[0]:
      row_dict = {'frame_number': source_row['frame_number']}
      result_data_table.append(row_dict)

    # Pick target field data points from the all data tables
    if end_frame == 0:  # Default to all frames
      end_frame = self.number_of_frames

    for dataset_index in range(self.number_of_datasets):
      for row_number in range(start_frame, end_frame):
        column_name = '%s_%s' % (field_name, dataset_index)
        # Stop if any of the data sets are missing the frame
        try:
          result_data_table[row_number][column_name] = \
          self.data_list[dataset_index][row_number][field_name]
        except IndexError:
          self.messages.append("Couldn't find frame data for row %d "
          "for %s" % (row_number, self.names_list[dataset_index]))
          break
    return result_table_description, result_data_table

  def GetOrdering(self, table_description):  # pylint: disable=R0201
    """ Creates a list of column names, ordered alphabetically except for the
      frame_number column which always will be the first column.

      Args:
        table_description: A dictionary of column definitions as defined by the
          gviz_api.DataTable documentation.
      Returns:
        A list of column names, where frame_number is the first and the
        remaining columns are sorted alphabetically.
    """
    # The JSON data representation generated from gviz_api.DataTable.ToJSon()
    # must have frame_number as its first column in order for the chart to
    # use it as it's X-axis value series.
    # gviz_api.DataTable orders the columns by name by default, which will
    # be incorrect if we have column names that are sorted before frame_number
    # in our data table.
    columns_ordering = ['frame_number']
    # add all other columns:
    for column in sorted(table_description.keys()):
      if column != 'frame_number':
        columns_ordering.append(column)
    return columns_ordering

  def CreateConfigurationTable(self, configurations):  # pylint: disable=R0201
    """ Combines multiple test data configurations for display.

    Args:
      configurations: List of one ore more configurations. Each configuration
      is required to be a list of dictionaries with two keys: 'name' and
      'value'.
      Example of a single configuration:
      [
        {'name': 'name', 'value': 'VP8 software'},
        {'name': 'test_number', 'value': '0'},
        {'name': 'input_filename', 'value': 'foreman_cif.yuv'},
      ]
    Returns:
      A tuple containing:
      - a dictionary describing the columns in the configuration table to be
        displayed. All columns will have string as data type.
        Example:
        {
          'name': 'string',
          'test_number': 'string',
          'input_filename': 'string',
         }
      - a list containing dictionaries (one per configuration) with the
        configuration column names mapped to the value for each test run:

        Example matching the columns above:
        [
         {'name': 'VP8 software',
          'test_number': '12',
          'input_filename': 'foreman_cif.yuv' },
         {'name': 'VP8 hardware',
          'test_number': '5',
          'input_filename': 'foreman_cif.yuv' },
        ]
    """
    result_description = {}
    result_data = []

    for configuration in configurations:
      data = {}
      result_data.append(data)
      for values in configuration:
        name = values['name']
        value = values['value']
        result_description[name] = 'string'
        data[name] = value
    return result_description, result_data
