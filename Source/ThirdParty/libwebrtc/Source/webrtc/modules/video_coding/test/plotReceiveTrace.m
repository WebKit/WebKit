function [t, TS] = plotReceiveTrace(filename, flat)
fid=fopen(filename);
%DEBUG     ; ( 8:32:33:375 |    0)        VIDEO:1          ;      5260; First packet of frame 1869537938
%DEBUG     ; ( 8:32:33:375 |    0) VIDEO CODING:1          ;      5260; Decoding timestamp 1869534934
%DEBUG     ; ( 8:32:33:375 |    0)        VIDEO:1          ;      5260; Render frame 1869534934 at 20772610
%DEBUG     ; ( 8:32:33:375 |    0) VIDEO CODING:-1         ;      5260; Frame decoded: timeStamp=1870511259 decTime=0 maxDecTime=0, at 19965
%DEBUG     ; ( 7:59:42:500 |    0)        VIDEO:-1         ;      2500; Received complete frame timestamp 1870514263 frame type 1 frame size 7862 at time 19965, jitter estimate was 130
%DEBUG     ; ( 8: 5:51:774 |    0)        VIDEO:-1         ;      3968; ExtrapolateLocalTime(1870967878)=24971 ms

if nargin == 1
    flat = 0;
end
line = fgetl(fid);
estimatedArrivalTime = [];
packetTime = [];
firstPacketTime = [];
decodeTime = [];
decodeCompleteTime = [];
renderTime = [];
completeTime = [];
while ischar(line)%line ~= -1
    if length(line) == 0
        line = fgetl(fid);
        continue;
    end
    % Parse the trace line header
    [tempres, count] = sscanf(line, 'DEBUG     ; (%u:%u:%u:%u |%*lu)%13c:');
    if count < 5
        line = fgetl(fid);
        continue;
    end
    hr=tempres(1);
    mn=tempres(2);
    sec=tempres(3);
    ms=tempres(4);
    timeInMs=hr*60*60*1000 + mn*60*1000 + sec*1000 + ms;
    label = tempres(5:end);
    I = find(label ~= 32); 
    label = label(I(1):end); % remove white spaces
    if ~strncmp(char(label), 'VIDEO', 5) & ~strncmp(char(label), 'VIDEO CODING', 12)
        line = fgetl(fid);
        continue;
    end
    message = line(72:end);
    
    % Parse message
    [p, count] = sscanf(message, 'ExtrapolateLocalTime(%lu)=%lu ms');
    if count == 2
        estimatedArrivalTime = [estimatedArrivalTime; p'];
        line = fgetl(fid);
        continue;
    end
    
    [p, count] = sscanf(message, 'Packet seqNo %u of frame %lu at %lu');
    if count == 3
        packetTime = [packetTime; p'];
        line = fgetl(fid);
        continue;
    end
    
    [p, count] = sscanf(message, 'First packet of frame %lu at %lu');
    if count == 2
        firstPacketTime = [firstPacketTime; p'];
        line = fgetl(fid);
        continue;
    end
    
    [p, count] = sscanf(message, 'Decoding timestamp %lu at %lu');
    if count == 2
        decodeTime = [decodeTime; p'];
        line = fgetl(fid);
        continue;        
    end
    
    [p, count] = sscanf(message, 'Render frame %lu at %lu. Render delay %lu, required delay %lu, max decode time %lu, min total delay %lu');
    if count == 6
        renderTime = [renderTime; p'];
        line = fgetl(fid);
        continue;
    end

    [p, count] = sscanf(message, 'Frame decoded: timeStamp=%lu decTime=%d maxDecTime=%lu, at %lu');
    if count == 4
        decodeCompleteTime = [decodeCompleteTime; p'];
        line = fgetl(fid);
        continue;
    end

    [p, count] = sscanf(message, 'Received complete frame timestamp %lu frame type %u frame size %*u at time %lu, jitter estimate was %lu');
    if count == 4
        completeTime = [completeTime; p'];
        line = fgetl(fid);
        continue;
    end
    
    line = fgetl(fid);
end
fclose(fid);

t = completeTime(:,3);
TS = completeTime(:,1);

figure;
subplot(211);
hold on;
slope = 0;

if sum(size(packetTime)) > 0
    % Plot the time when each packet arrives
    firstTimeStamp = packetTime(1,2);
    x = (packetTime(:,2) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    firstTime = packetTime(1,3);
    plot(x, packetTime(:,3) - firstTime - slope, 'b.');
else
    % Plot the time when the first packet of a frame arrives
    firstTimeStamp = firstPacketTime(1,1);
    x = (firstPacketTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    firstTime = firstPacketTime(1,2);
    plot(x, firstPacketTime(:,2) - firstTime - slope, 'b.');
end

% Plot the frame complete time
if prod(size(completeTime)) > 0
    x = (completeTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, completeTime(:,3) - firstTime - slope, 'ks');
end

% Plot the time the decode starts
if prod(size(decodeTime)) > 0
    x = (decodeTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, decodeTime(:,2) - firstTime - slope, 'r.');
end

% Plot the decode complete time
if prod(size(decodeCompleteTime)) > 0
    x = (decodeCompleteTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, decodeCompleteTime(:,4) - firstTime - slope, 'g.');
end

if prod(size(renderTime)) > 0
    % Plot the wanted render time in ms
    x = (renderTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, renderTime(:,2) - firstTime - slope, 'c-');
    
    % Plot the render time if there were no render delay or decoding delay.
    x = (renderTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, renderTime(:,2) - firstTime - slope - renderTime(:, 3) - renderTime(:, 5), 'c--');
    
    % Plot the render time if there were no render delay.
    x = (renderTime(:,1) - firstTimeStamp)/90;
    if flat
        slope = x;
    end
    plot(x, renderTime(:,2) - firstTime - slope - renderTime(:, 3) - renderTime(:, 5), 'b-');
end

%plot(x, 90*x, 'r-');

xlabel('RTP timestamp (in ms)');
ylabel('Time (ms)');
legend('Packet arrives', 'Frame complete', 'Decode', 'Decode complete', 'Time to render', 'Only jitter', 'Must decode');

% subplot(312);
% hold on;
% completeTs = completeTime(:, 1);
% arrivalTs = estimatedArrivalTime(:, 1);
% [c, completeIdx, arrivalIdx] = intersect(completeTs, arrivalTs);
% %plot(completeTs(completeIdx), completeTime(completeIdx, 3) - estimatedArrivalTime(arrivalIdx, 2));
% timeUntilComplete = completeTime(completeIdx, 3) - estimatedArrivalTime(arrivalIdx, 2);
% devFromAvgCompleteTime = timeUntilComplete - mean(timeUntilComplete);
% plot(completeTs(completeIdx) - completeTs(completeIdx(1)), devFromAvgCompleteTime);
% plot(completeTime(:, 1) - completeTime(1, 1), completeTime(:, 4), 'r');
% plot(decodeCompleteTime(:, 1) - decodeCompleteTime(1, 1), decodeCompleteTime(:, 2), 'g');
% plot(decodeCompleteTime(:, 1) - decodeCompleteTime(1, 1), decodeCompleteTime(:, 3), 'k');
% xlabel('RTP timestamp');
% ylabel('Time (ms)');
% legend('Complete time - Estimated arrival time', 'Desired jitter buffer level', 'Actual decode time', 'Max decode time', 0);

if prod(size(renderTime)) > 0
    subplot(212);
    hold on;
    firstTime = renderTime(1, 1);
    targetDelay = max(renderTime(:, 3) + renderTime(:, 4) + renderTime(:, 5), renderTime(:, 6));
    plot(renderTime(:, 1) - firstTime, renderTime(:, 3), 'r-');
    plot(renderTime(:, 1) - firstTime, renderTime(:, 4), 'b-');
    plot(renderTime(:, 1) - firstTime, renderTime(:, 5), 'g-');
    plot(renderTime(:, 1) - firstTime, renderTime(:, 6), 'k-');
    plot(renderTime(:, 1) - firstTime, targetDelay, 'c-');
    xlabel('RTP timestamp');
    ylabel('Time (ms)');
    legend('Render delay', 'Jitter delay', 'Decode delay', 'Extra delay', 'Min total delay');
end