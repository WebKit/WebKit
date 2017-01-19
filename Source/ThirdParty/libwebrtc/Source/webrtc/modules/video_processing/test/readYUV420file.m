function [Y,U,V] = readYUV420file(filename, width, height)
% [Y,U,V] = readYUVfile(filename, width, height)

fid = fopen(filename,'rb');
if fid==-1
    error(['Cannot open file ' filename]);
end

% Number of pixels per image
nPx=width*height;

% nPx bytes luminance, nPx/4 bytes U, nPx/4 bytes V
frameSizeBytes = nPx*1.5;

% calculate number of frames
fseek(fid,0,'eof'); % move to end of file
fileLen=ftell(fid); % number of bytes
fseek(fid,0,'bof'); % rewind to start

% calculate number of frames
numFrames = floor(fileLen/frameSizeBytes);

Y=uint8(zeros(height,width,numFrames));
U=uint8(zeros(height/2,width/2,numFrames));
V=uint8(zeros(height/2,width/2,numFrames));

[X,nBytes]=fread(fid, frameSizeBytes, 'uchar');

for k=1:numFrames

    % Store luminance
    Y(:,:,k)=uint8(reshape(X(1:nPx), width, height).');

    % Store U channel
    U(:,:,k)=uint8(reshape(X(nPx + (1:nPx/4)), width/2, height/2).');

    % Store V channel
    V(:,:,k)=uint8(reshape(X(nPx + nPx/4 + (1:nPx/4)), width/2, height/2).');

    % Read next frame
    [X,nBytes]=fread(fid, frameSizeBytes, 'uchar');
end


fclose(fid);
