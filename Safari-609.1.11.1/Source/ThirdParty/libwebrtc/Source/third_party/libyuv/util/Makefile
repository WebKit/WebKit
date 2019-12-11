psnr: psnr.cc ssim.cc psnr_main.cc
ifeq ($(CXX),icl)
	$(CXX) /arch:SSE2 /Ox /openmp psnr.cc ssim.cc psnr_main.cc
else
	$(CXX) -msse2 -O3 -fopenmp -static -o psnr psnr.cc ssim.cc psnr_main.cc -Wl,--strip-all
endif

# for MacOS
# /usr/local/bin/g++-7 -msse2 -O3 -fopenmp -Bstatic -o psnr psnr.cc ssim.cc psnr_main.cc
