# This is a generic makefile for libyuv for Windows Arm.
# call "c:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\x86_arm\vcvarsx86_arm.bat"
# nmake /f winarm.mk
# make -f winarm.mk
# nmake /f winarm.mk clean
# consider /arch:ARMv7VE
CC=cl
CCFLAGS=/Ox /nologo /Iinclude /DWINAPI_FAMILY=WINAPI_FAMILY_PHONE_APP
AR=lib
ARFLAGS=/MACHINE:ARM /NOLOGO /SUBSYSTEM:NATIVE
RM=cmd /c del

LOCAL_OBJ_FILES = \
	source/compare.o\
	source/compare_common.o\
	source/convert.o\
	source/convert_argb.o\
	source/convert_from.o\
	source/convert_from_argb.o\
	source/convert_to_argb.o\
	source/convert_to_i420.o\
	source/cpu_id.o\
	source/planar_functions.o\
	source/rotate.o\
	source/rotate_any.o\
	source/rotate_argb.o\
	source/rotate_common.o\
	source/row_any.o\
	source/row_common.o\
	source/scale.o\
	source/scale_any.o\
	source/scale_argb.o\
	source/scale_common.o\
	source/video_common.o

.cc.o:
	$(CC) /c $(CCFLAGS) $*.cc /Fo$@

all: libyuv_arm.lib winarm.mk

libyuv_arm.lib: $(LOCAL_OBJ_FILES) winarm.mk
	$(AR) $(ARFLAGS) /OUT:$@ $(LOCAL_OBJ_FILES)

clean:
	$(RM) "source\*.o" libyuv_arm.lib

