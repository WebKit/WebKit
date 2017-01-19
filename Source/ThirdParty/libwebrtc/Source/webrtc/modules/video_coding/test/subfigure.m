function H = subfigure(m, n, p)
%
% H = SUBFIGURE(m, n, p)
%
% Create a new figure window and adjust position and size such that it will
% become the p-th tile in an m-by-n matrix of windows. (The interpretation of
% m, n, and p is the same as for SUBPLOT.
%
% Henrik Lundin, 2009-01-19
%


h = figure;

[j, i] = ind2sub([n m], p);
scrsz = get(0,'ScreenSize'); % get screen size
%scrsz = [1, 1, 1600, 1200];

taskbarSize = 58;
windowbarSize = 68;
windowBorder = 4;

scrsz(2) = scrsz(2) + taskbarSize;
scrsz(4) = scrsz(4) - taskbarSize;

set(h, 'position', [(j-1)/n * scrsz(3) + scrsz(1) + windowBorder,...
        (m-i)/m * scrsz(4) + scrsz(2) + windowBorder, ...
        scrsz(3)/n - (windowBorder + windowBorder),...
        scrsz(4)/m - (windowbarSize + windowBorder + windowBorder)]);

