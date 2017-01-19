function parsed = parseLog(filename)
%
% parsed = parseLog(filename)
% Parses a DataLog text file, with the filename specified in the string
% filename, into a struct with each column name as a field, and with the
% column data stored as a vector in that field.
%
% Arguments
%
% filename: A string with the name of the file to parse.
%
% Return value
%
% parsed: A struct containing each column parsed from the input file
%         as a field and with the column data stored as a vector in that 
%         field.
%

% Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
%
% Use of this source code is governed by a BSD-style license
% that can be found in the LICENSE file in the root of the source
% tree. An additional intellectual property rights grant can be found
% in the file PATENTS.  All contributing project authors may
% be found in the AUTHORS file in the root of the source tree.

table = importdata(filename, ',', 1);
if ~isstruct(table)
  error('Malformed file, possibly empty or lacking data entries')
end

colheaders = table.textdata;
if length(colheaders) == 1
  colheaders = regexp(table.textdata{1}, ',', 'split');
end

parsed = struct;
i = 1;
while i <= length(colheaders)
  % Checking for a multi-value column.
  m = regexp(colheaders{i}, '([\w\t]+)\[(\d+)\]', 'tokens');
  if ~isempty(m)
    % Parse a multi-value column
    n = str2double(m{1}{2}) - 1;
    parsed.(strrep(m{1}{1}, ' ', '_')) = table.data(:, i:i+n);
    i = i + n + 1;
  elseif ~isempty(colheaders{i})
    % Parse a single-value column
    parsed.(strrep(colheaders{i}, ' ', '_')) = table.data(:, i);
    i = i + 1;
  else
    error('Empty column');
  end
end
