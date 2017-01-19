% Create the color enhancement look-up table and write it to
% file colorEnhancementTable.cpp. Copy contents of that file into
% the source file for the color enhancement function.

clear
close all


% First, define the color enhancement in a normalized domain

% Compander function is defined in three radial zones.
% 1. From 0 to radius r0, the compander function
% is a second-order polynomial intersecting the points (0,0)
% and (r0, r0), and with a slope B in (0,0).
% 2. From r0 to r1, the compander is a third-order polynomial
% intersecting the points (r0, r0) and (r1, r1), and with the
% same slope as the first part in the point (r0, r0) and slope
% equal to 1 in (r1, r1).
% 3. For radii larger than r1, the compander function is the
% unity scale function (no scaling at all).

r0=0.07; % Dead zone radius (must be > 0)
r1=0.6; % Enhancement zone radius (must be > r0 and < 1)
B=0.2; % initial slope of compander function (between 0 and 1)

x0=linspace(0,r0).'; % zone 1
x1=linspace(r0,r1).'; % zone 2
x2=linspace(r1,1).'; % zone 3

A=(1-B)/r0;
f0=A*x0.^2+B*x0; % compander function in zone 1

% equation system for finding second zone parameters
M=[r0^3 r0^2 r0 1;
    3*r0^2 2*r0 1 0;
    3*r1^2 2*r1 1 0;
    r1^3 r1^2 r1 1];
m=[A*r0^2+B*r0; 2*A*r0+B; 1; r1];
% solve equations
theta=M\m;

% compander function in zone 1
f1=[x1.^3 x1.^2 x1 ones(size(x1))]*theta;

x=[x0; x1; x2];
f=[f0; f1; x2];

% plot it
figure(1)
plot(x,f,x,x,':')
xlabel('Normalized radius')
ylabel('Modified radius')


% Now, create the look-up table in the integer color space
[U,V]=meshgrid(0:255, 0:255); % U-V space
U0=U;
V0=V;

% Conversion matrix from normalized YUV to RGB
T=[1 0 1.13983; 1 -0.39465 -0.58060; 1 2.03211 0];
Ylum=0.5;

figure(2)
Z(:,:,1)=Ylum + (U-127)/256*T(1,2) + (V-127)/256*T(1,3);
Z(:,:,2)=Ylum + (U-127)/256*T(2,2) + (V-127)/256*T(2,3);
Z(:,:,3)=Ylum + (U-127)/256*T(3,2) + (V-127)/256*T(3,3);
Z=max(Z,0);
Z=min(Z,1);
subplot(121)
image(Z);
axis square
axis off
set(gcf,'color','k')

R = sqrt((U-127).^2 + (V-127).^2);
Rnorm = R/127;
RnormMod = Rnorm;
RnormMod(RnormMod==0)=1; % avoid division with zero

% find indices to pixels in dead-zone (zone 1)
ix=find(Rnorm<=r0);
scaleMatrix = (A*Rnorm(ix).^2 + B*Rnorm(ix))./RnormMod(ix);
U(ix)=(U(ix)-127).*scaleMatrix+127;
V(ix)=(V(ix)-127).*scaleMatrix+127;

% find indices to pixels in zone 2
ix=find(Rnorm>r0 & Rnorm<=r1);
scaleMatrix = (theta(1)*Rnorm(ix).^3 + theta(2)*Rnorm(ix).^2 + ...
    theta(3)*Rnorm(ix) + theta(4)) ./ RnormMod(ix);
U(ix)=(U(ix)-127).*scaleMatrix + 127;
V(ix)=(V(ix)-127).*scaleMatrix + 127;

% round to integer values and saturate
U=round(U);
V=round(V);
U=max(min(U,255),0);
V=max(min(V,255),0);

Z(:,:,1)=Ylum + (U-127)/256*T(1,2) + (V-127)/256*T(1,3);
Z(:,:,2)=Ylum + (U-127)/256*T(2,2) + (V-127)/256*T(2,3);
Z(:,:,3)=Ylum + (U-127)/256*T(3,2) + (V-127)/256*T(3,3);
Z=max(Z,0);
Z=min(Z,1);
subplot(122)
image(Z);
axis square
axis off

figure(3)
subplot(121)
mesh(U-U0)
subplot(122)
mesh(V-V0)



% Last, write to file
% Write only one matrix, since U=V'

fid = fopen('../out/Debug/colorEnhancementTable.h','wt');
if fid==-1
    error('Cannot open file colorEnhancementTable.cpp');
end

fprintf(fid,'//colorEnhancementTable.h\n\n');
fprintf(fid,'//Copy the constant table to the appropriate header file.\n\n');

fprintf(fid,'//Table created with Matlab script createTable.m\n\n');
fprintf(fid,'//Usage:\n');
fprintf(fid,'//    Umod=colorTable[U][V]\n');
fprintf(fid,'//    Vmod=colorTable[V][U]\n');

fprintf(fid,'static unsigned char colorTable[%i][%i] = {\n', size(U,1), size(U,2));

for u=1:size(U,2)
    fprintf(fid,'    {%i', U(1,u));
    for v=2:size(U,1)
        fprintf(fid,', %i', U(v,u));
    end
    fprintf(fid,'}');
    if u<size(U,2)
        fprintf(fid,',');
    end
    fprintf(fid,'\n');
end
fprintf(fid,'};\n\n');
fclose(fid);
fprintf('done');


answ=input('Create test vector (takes some time...)? y/n : ','s');
if answ ~= 'y'
    return
end

% Also, create test vectors

% Read test file foreman.yuv
fprintf('Reading test file...')
[y,u,v]=readYUV420file('../out/Debug/testFiles/foreman_cif.yuv',352,288);
fprintf(' done\n');
unew=uint8(zeros(size(u)));
vnew=uint8(zeros(size(v)));

% traverse all frames
for k=1:size(y,3)
    fprintf('Frame %i\n', k);
    for r=1:size(u,1)
        for c=1:size(u,2)
            unew(r,c,k) = uint8(U(double(v(r,c,k))+1, double(u(r,c,k))+1));
            vnew(r,c,k) = uint8(V(double(v(r,c,k))+1, double(u(r,c,k))+1));
        end
    end
end

fprintf('\nWriting modified test file...')
writeYUV420file('../out/Debug/foremanColorEnhanced.yuv',y,unew,vnew);
fprintf(' done\n');
