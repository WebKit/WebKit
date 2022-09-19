# This is a generic makefile for libyuv for gcc.
# make -f linux.mk CXX=clang++

CC?=gcc
CFLAGS?=-O2 -fomit-frame-pointer
CFLAGS+=-Iinclude/

CXX?=g++
CXXFLAGS?=-O2 -fomit-frame-pointer
CXXFLAGS+=-Iinclude/

LOCAL_OBJ_FILES := \
	source/compare.o           \
	source/compare_common.o    \
	source/compare_gcc.o       \
	source/compare_msa.o       \
	source/compare_neon.o      \
	source/compare_neon64.o    \
	source/compare_win.o       \
	source/convert.o           \
	source/convert_argb.o      \
	source/convert_from.o      \
	source/convert_from_argb.o \
	source/convert_jpeg.o      \
	source/convert_to_argb.o   \
	source/convert_to_i420.o   \
	source/cpu_id.o            \
	source/mjpeg_decoder.o     \
	source/mjpeg_validate.o    \
	source/planar_functions.o  \
	source/rotate.o            \
	source/rotate_any.o        \
	source/rotate_argb.o       \
	source/rotate_common.o     \
	source/rotate_gcc.o        \
	source/rotate_msa.o        \
	source/rotate_neon.o       \
	source/rotate_neon64.o     \
	source/rotate_win.o        \
	source/row_any.o           \
	source/row_common.o        \
	source/row_gcc.o           \
	source/row_msa.o           \
	source/row_neon.o          \
	source/row_neon64.o        \
	source/row_win.o           \
	source/scale.o             \
	source/scale_any.o         \
	source/scale_argb.o        \
	source/scale_common.o      \
	source/scale_gcc.o         \
	source/scale_msa.o         \
	source/scale_neon.o        \
	source/scale_neon64.o      \
	source/scale_rgb.o         \
	source/scale_uv.o          \
	source/scale_win.o         \
	source/video_common.o

.cc.o:
	$(CXX) -c $(CXXFLAGS) $*.cc -o $*.o

.c.o:
	$(CC) -c $(CFLAGS) $*.c -o $*.o

all: libyuv.a i444tonv12_eg yuvconvert yuvconstants cpuid psnr

libyuv.a: $(LOCAL_OBJ_FILES)
	$(AR) $(ARFLAGS) $@ $(LOCAL_OBJ_FILES)

# A C++ test utility that uses libyuv conversion.
yuvconvert: util/yuvconvert.cc libyuv.a
	$(CXX) $(CXXFLAGS) -Iutil/ -o $@ util/yuvconvert.cc libyuv.a

# A C test utility that generates yuvconstants for yuv to rgb.
yuvconstants: util/yuvconstants.c libyuv.a
	$(CXX) $(CXXFLAGS) -Iutil/ -lm -o $@ util/yuvconstants.c libyuv.a

# A standalone test utility
psnr: util/psnr.cc
	$(CXX) $(CXXFLAGS) -Iutil/ -o $@ util/psnr.cc util/psnr_main.cc util/ssim.cc

# A simple conversion example.
i444tonv12_eg: util/i444tonv12_eg.cc libyuv.a
	$(CXX) $(CXXFLAGS) -o $@ util/i444tonv12_eg.cc libyuv.a

# A C test utility that uses libyuv conversion from C.
# gcc 4.4 and older require -fno-exceptions to avoid link error on __gxx_personality_v0
# CC=gcc-4.4 CXXFLAGS=-fno-exceptions CXX=g++-4.4 make -f linux.mk
cpuid: util/cpuid.c libyuv.a
	$(CC) $(CFLAGS) -o $@ util/cpuid.c libyuv.a

clean:
	/bin/rm -f source/*.o *.ii *.s libyuv.a i444tonv12_eg yuvconvert yuvconstants cpuid psnr
