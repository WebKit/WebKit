function plotTimingTest(filename)
fid=fopen(filename);

%DEBUG     ; ( 9:53:33:859 |    0)        VIDEO:-1         ;      7132; Stochastic test 1
%DEBUG     ; ( 9:53:33:859 |    0) VIDEO CODING:-1         ;      7132; Frame decoded: timeStamp=3000 decTime=10 at 10012
%DEBUG     ; ( 9:53:33:859 |    0)        VIDEO:-1         ;      7132; timeStamp=3000 clock=10037 maxWaitTime=0
%DEBUG     ; ( 9:53:33:859 |    0)        VIDEO:-1         ;      7132; timeStampMs=33 renderTime=54
line = fgetl(fid);
decTime = [];
waitTime = [];
renderTime = [];
foundStart = 0;
testName = 'Stochastic test 1';
while ischar(line)
    if length(line) == 0
        line = fgetl(fid);
        continue;
    end
    lineOrig = line;
    line = line(72:end);
    if ~foundStart
        if strncmp(line, testName, length(testName)) 
            foundStart = 1;
        end
        line = fgetl(fid);
        continue;
    end
    [p, count] = sscanf(line, 'Frame decoded: timeStamp=%lu decTime=%d maxDecTime=%d, at %lu');
    if count == 4
        decTime = [decTime; p'];
        line = fgetl(fid);
        continue;
    end
    [p, count] = sscanf(line, 'timeStamp=%u clock=%u maxWaitTime=%u');
    if count == 3
        waitTime = [waitTime; p'];
        line = fgetl(fid);
        continue;
    end
    [p, count] = sscanf(line, 'timeStamp=%u renderTime=%u');
    if count == 2
        renderTime = [renderTime; p'];
        line = fgetl(fid);
        continue;
    end    
    line = fgetl(fid);
end
fclose(fid);

% Compensate for wrap arounds and start counting from zero.
timeStamps = waitTime(:, 1);
tsDiff = diff(timeStamps);
wrapIdx = find(tsDiff < 0);
timeStamps(wrapIdx+1:end) = hex2dec('ffffffff') + timeStamps(wrapIdx+1:end);
timeStamps = timeStamps - timeStamps(1);

figure;
hold on;
plot(timeStamps, decTime(:, 2), 'r');
plot(timeStamps, waitTime(:, 3), 'g');
plot(timeStamps(2:end), diff(renderTime(:, 2)), 'b');
legend('Decode time', 'Max wait time', 'Render time diff');