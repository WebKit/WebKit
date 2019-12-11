function rtpAnalyze( input_file )
%RTP_ANALYZE Analyze RTP stream(s) from a txt file
%   The function takes the output from the command line tool rtp_analyze
%   and analyzes the stream(s) therein. First, process your rtpdump file
%   through rtp_analyze (from command line):
%   $ out/Debug/rtp_analyze my_file.rtp my_file.txt
%   Then load it with this function (in Matlab):
%   >> rtpAnalyze('my_file.txt')

% Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
%
% Use of this source code is governed by a BSD-style license
% that can be found in the LICENSE file in the root of the source
% tree. An additional intellectual property rights grant can be found
% in the file PATENTS.  All contributing project authors may
% be found in the AUTHORS file in the root of the source tree.

[SeqNo,TimeStamp,ArrTime,Size,PT,M,SSRC] = importfile(input_file);

%% Filter out RTCP packets.
% These appear as RTP packets having payload types 72 through 76.
ix = not(ismember(PT, 72:76));
fprintf('Removing %i RTCP packets\n', length(SeqNo) - sum(ix));
SeqNo = SeqNo(ix);
TimeStamp = TimeStamp(ix);
ArrTime = ArrTime(ix);
Size = Size(ix);
PT = PT(ix);
M = M(ix);
SSRC = SSRC(ix);

%% Find streams.
[uSSRC, ~, uix] = unique(SSRC);

% If there are multiple streams, select one and purge the other
% streams from the data vectors. If there is only one stream, the
% vectors are good to use as they are.
if length(uSSRC) > 1
    for i=1:length(uSSRC)
        uPT = unique(PT(uix == i));
        fprintf('%i: %s (%d packets, pt: %i', i, uSSRC{i}, ...
            length(find(uix==i)), uPT(1));
        if length(uPT) > 1
            fprintf(', %i', uPT(2:end));
        end
        fprintf(')\n');
    end
    sel = input('Select stream number: ');
    if sel < 1 || sel > length(uSSRC)
        error('Out of range');
    end
    ix = find(uix == sel);
    % This is where the data vectors are trimmed.
    SeqNo = SeqNo(ix);
    TimeStamp = TimeStamp(ix);
    ArrTime = ArrTime(ix);
    Size = Size(ix);
    PT = PT(ix);
    M = M(ix);
    SSRC = SSRC(ix);
end

%% Unwrap SeqNo and TimeStamp.
SeqNoUW = maxUnwrap(SeqNo, 65535);
TimeStampUW = maxUnwrap(TimeStamp, 4294967295);

%% Generate some stats for the stream.
fprintf('Statistics:\n');
fprintf('SSRC: %s\n', SSRC{1});
uPT = unique(PT);
if length(uPT) > 1
    warning('This tool cannot yet handle changes in codec sample rate');
end
fprintf('Payload type(s): %i', uPT(1));
if length(uPT) > 1
    fprintf(', %i', uPT(2:end));
end
fprintf('\n');
fprintf('Packets: %i\n', length(SeqNo));
SortSeqNo = sort(SeqNoUW);
fprintf('Missing sequence numbers: %i\n', ...
    length(find(diff(SortSeqNo) > 1)));
fprintf('Duplicated packets: %i\n', length(find(diff(SortSeqNo) == 0)));
reorderIx = findReorderedPackets(SeqNoUW);
fprintf('Reordered packets: %i\n', length(reorderIx));
tsdiff = diff(TimeStampUW);
tsdiff = tsdiff(diff(SeqNoUW) == 1);
[utsdiff, ~, ixtsdiff] = unique(tsdiff);
fprintf('Common packet sizes:\n');
for i = 1:length(utsdiff)
    fprintf('  %i samples (%i%%)\n', ...
        utsdiff(i), ...
        round(100 * length(find(ixtsdiff == i))/length(ixtsdiff)));
end

%% Trying to figure out sample rate.
fs_est = (TimeStampUW(end) - TimeStampUW(1)) / (ArrTime(end) - ArrTime(1));
fs_vec = [8, 16, 32, 48];
fs = 0;
for f = fs_vec
    if abs((fs_est-f)/f) < 0.05  % 5% margin
        fs = f;
        break;
    end
end
if fs == 0
    fprintf('Cannot determine sample rate. I get it to %.2f kHz\n', ...
        fs_est);
    fs = input('Please, input a sample rate (in kHz): ');
else
    fprintf('Sample rate estimated to %i kHz\n', fs);
end

SendTimeMs = (TimeStampUW - TimeStampUW(1)) / fs;

fprintf('Stream duration at sender: %.1f seconds\n', ...
    (SendTimeMs(end) - SendTimeMs(1)) / 1000);

fprintf('Stream duration at receiver: %.1f seconds\n', ...
    (ArrTime(end) - ArrTime(1)) / 1000);

fprintf('Clock drift: %.2f%%\n', ...
    100 * ((ArrTime(end) - ArrTime(1)) / ...
    (SendTimeMs(end) - SendTimeMs(1)) - 1));

fprintf('Sent average bitrate: %i kbps\n', ...
    round(sum(Size) * 8 / (SendTimeMs(end)-SendTimeMs(1))));

fprintf('Received average bitrate: %i kbps\n', ...
    round(sum(Size) * 8 / (ArrTime(end)-ArrTime(1))));

%% Plots.
delay = ArrTime - SendTimeMs;
delay = delay - min(delay);
delayOrdered = delay;
delayOrdered(reorderIx) = nan;  % Set reordered packets to NaN.
delayReordered = delay(reorderIx);  % Pick the reordered packets.
sendTimeMsReordered = SendTimeMs(reorderIx);

% Sort time arrays in packet send order.
[~, sortix] = sort(SeqNoUW);
SendTimeMs = SendTimeMs(sortix);
Size = Size(sortix);
delayOrdered = delayOrdered(sortix);

figure
plot(SendTimeMs / 1000, delayOrdered, ...
    sendTimeMsReordered / 1000, delayReordered, 'r.');
xlabel('Send time [s]');
ylabel('Relative transport delay [ms]');
title(sprintf('SSRC: %s', SSRC{1}));

SendBitrateKbps = 8 * Size(1:end-1) ./ diff(SendTimeMs);
figure
plot(SendTimeMs(1:end-1)/1000, SendBitrateKbps);
xlabel('Send time [s]');
ylabel('Send bitrate [kbps]');
end

%% Subfunctions.

% findReorderedPackets returns the index to all packets that are considered
% old compared with the largest seen sequence number. The input seqNo must
% be unwrapped for this to work.
function reorderIx = findReorderedPackets(seqNo)
largestSeqNo = seqNo(1);
reorderIx = [];
for i = 2:length(seqNo)
    if seqNo(i) < largestSeqNo
        reorderIx = [reorderIx; i]; %#ok<AGROW>
    else
        largestSeqNo = seqNo(i);
    end
end
end

%% Auto-generated subfunction.
function [SeqNo,TimeStamp,SendTime,Size,PT,M,SSRC] = ...
    importfile(filename, startRow, endRow)
%IMPORTFILE Import numeric data from a text file as column vectors.
%   [SEQNO,TIMESTAMP,SENDTIME,SIZE,PT,M,SSRC] = IMPORTFILE(FILENAME) Reads
%   data from text file FILENAME for the default selection.
%
%   [SEQNO,TIMESTAMP,SENDTIME,SIZE,PT,M,SSRC] = IMPORTFILE(FILENAME,
%   STARTROW, ENDROW) Reads data from rows STARTROW through ENDROW of text
%   file FILENAME.
%
% Example:
%   [SeqNo,TimeStamp,SendTime,Size,PT,M,SSRC] =
%   importfile('rtpdump_recv.txt',2, 123);
%
%    See also TEXTSCAN.

% Auto-generated by MATLAB on 2015/05/28 09:55:50

%% Initialize variables.
if nargin<=2
    startRow = 2;
    endRow = inf;
end

%% Format string for each line of text:
%   column1: double (%f)
%   column2: double (%f)
%   column3: double (%f)
%   column4: double (%f)
%   column5: double (%f)
%   column6: double (%f)
%   column7: text (%s)
% For more information, see the TEXTSCAN documentation.
formatSpec = '%5f%11f%11f%6f%6f%3f%s%[^\n\r]';

%% Open the text file.
fileID = fopen(filename,'r');

%% Read columns of data according to format string.
% This call is based on the structure of the file used to generate this
% code. If an error occurs for a different file, try regenerating the code
% from the Import Tool.
dataArray = textscan(fileID, formatSpec, endRow(1)-startRow(1)+1, ...
    'Delimiter', '', 'WhiteSpace', '', 'HeaderLines', startRow(1)-1, ...
    'ReturnOnError', false);
for block=2:length(startRow)
    frewind(fileID);
    dataArrayBlock = textscan(fileID, formatSpec, ...
        endRow(block)-startRow(block)+1, 'Delimiter', '', 'WhiteSpace', ...
        '', 'HeaderLines', startRow(block)-1, 'ReturnOnError', false);
    for col=1:length(dataArray)
        dataArray{col} = [dataArray{col};dataArrayBlock{col}];
    end
end

%% Close the text file.
fclose(fileID);

%% Post processing for unimportable data.
% No unimportable data rules were applied during the import, so no post
% processing code is included. To generate code which works for
% unimportable data, select unimportable cells in a file and regenerate the
% script.

%% Allocate imported array to column variable names
SeqNo = dataArray{:, 1};
TimeStamp = dataArray{:, 2};
SendTime = dataArray{:, 3};
Size = dataArray{:, 4};
PT = dataArray{:, 5};
M = dataArray{:, 6};
SSRC = dataArray{:, 7};
end

