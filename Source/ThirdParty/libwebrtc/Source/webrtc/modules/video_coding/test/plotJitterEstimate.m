function plotJitterEstimate(filename)

[timestamps, framedata, slopes, randJitters, framestats, timetable, filtjitter, rtt, rttStatsVec] = jitterBufferTraceParser(filename);

x = 1:size(framestats, 1);
%figure(2);
subfigure(3, 2, 1);
hold on;
plot(x, slopes(x, 1).*(framestats(x, 1) - framestats(x, 2)) + 3*sqrt(randJitters(x,2)), 'b'); title('Estimate ms');
plot(x, filtjitter, 'r');
plot(x, slopes(x, 1).*(framestats(x, 1) - framestats(x, 2)), 'g');
subfigure(3, 2, 2);
%subplot(211);
plot(x, slopes(x, 1)); title('Line slope');
%subplot(212);
%plot(x, slopes(x, 2)); title('Line offset');
subfigure(3, 2, 3); hold on;
plot(x, framestats); plot(x, framedata(x, 1)); title('frame size and average frame size');
subfigure(3, 2, 4);
plot(x, framedata(x, 2)); title('Delay');
subfigure(3, 2, 5);
hold on;
plot(x, randJitters(x,1),'r');
plot(x, randJitters(x,2)); title('Random jitter');

subfigure(3, 2, 6);
delays = framedata(:,2);
dL = [0; framedata(2:end, 1) - framedata(1:end-1, 1)];
hold on;
plot(dL, delays, '.');
s = [min(dL) max(dL)];
plot(s, slopes(end, 1)*s + slopes(end, 2), 'g');
plot(s, slopes(end, 1)*s + slopes(end, 2) + 3*sqrt(randJitters(end,2)), 'r');
plot(s, slopes(end, 1)*s + slopes(end, 2) - 3*sqrt(randJitters(end,2)), 'r');
title('theta(1)*x+theta(2), (dT-dTS)/dL');
if sum(size(rttStatsVec)) > 0
    figure; hold on; 
    rttNstdDevsDrift = 3.5;
    rttNstdDevsJump = 2.5;
    rttSamples = rttStatsVec(:, 1);
    rttAvgs = rttStatsVec(:, 2);
    rttStdDevs = sqrt(rttStatsVec(:, 3));
    rttMax = rttStatsVec(:, 4);
    plot(rttSamples, 'ko-');
    plot(rttAvgs, 'g');
    plot(rttAvgs + rttNstdDevsDrift*rttStdDevs, 'b--'); 
    plot(rttAvgs + rttNstdDevsJump*rttStdDevs, 'b'); 
    plot(rttAvgs - rttNstdDevsJump*rttStdDevs, 'b');
    plot(rttMax, 'r');
    %plot(driftRestarts*max(maxRtts), '.');
    %plot(jumpRestarts*max(maxRtts), '.');
end