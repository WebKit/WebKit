function writeYUV420file(filename, Y, U, V)
% writeYUV420file(filename, Y, U, V)

fid = fopen(filename,'wb');
if fid==-1
    error(['Cannot open file ' filename]);
end

numFrames=size(Y,3);

for k=1:numFrames
   % Write luminance
   fwrite(fid,uint8(Y(:,:,k).'), 'uchar');

   % Write U channel
   fwrite(fid,uint8(U(:,:,k).'), 'uchar');

   % Write V channel
   fwrite(fid,uint8(V(:,:,k).'), 'uchar');
end

fclose(fid);
