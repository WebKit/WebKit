# This is the Android makefile for libyuv for NDK.
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
    source/compare.cc           \
    source/compare_common.cc    \
    source/compare_gcc.cc       \
    source/compare_msa.cc       \
    source/compare_neon.cc      \
    source/compare_neon64.cc    \
    source/compare_win.cc       \
    source/convert.cc           \
    source/convert_argb.cc      \
    source/convert_from.cc      \
    source/convert_from_argb.cc \
    source/convert_to_argb.cc   \
    source/convert_to_i420.cc   \
    source/cpu_id.cc            \
    source/planar_functions.cc  \
    source/rotate.cc            \
    source/rotate_any.cc        \
    source/rotate_argb.cc       \
    source/rotate_common.cc     \
    source/rotate_gcc.cc        \
    source/rotate_msa.cc        \
    source/rotate_neon.cc       \
    source/rotate_neon64.cc     \
    source/rotate_win.cc        \
    source/row_any.cc           \
    source/row_common.cc        \
    source/row_gcc.cc           \
    source/row_msa.cc           \
    source/row_neon.cc          \
    source/row_neon64.cc        \
    source/row_win.cc           \
    source/scale.cc             \
    source/scale_any.cc         \
    source/scale_argb.cc        \
    source/scale_common.cc      \
    source/scale_gcc.cc         \
    source/scale_msa.cc         \
    source/scale_neon.cc        \
    source/scale_neon64.cc      \
    source/scale_rgb.cc         \
    source/scale_uv.cc          \
    source/scale_win.cc         \
    source/video_common.cc

common_CFLAGS := -Wall -fexceptions
ifneq ($(LIBYUV_DISABLE_JPEG), "yes")
LOCAL_SRC_FILES += \
    source/convert_jpeg.cc      \
    source/mjpeg_decoder.cc     \
    source/mjpeg_validate.cc
common_CFLAGS += -DHAVE_JPEG
LOCAL_SHARED_LIBRARIES := libjpeg
endif

LOCAL_CFLAGS += $(common_CFLAGS)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE := libyuv_static
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := libyuv_static
LOCAL_MODULE := libyuv
ifneq ($(LIBYUV_DISABLE_JPEG), "yes")
LOCAL_SHARED_LIBRARIES := libjpeg
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_STATIC_LIBRARIES := libyuv_static
LOCAL_SHARED_LIBRARIES := libjpeg
LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_SRC_FILES := \
    unit_test/basictypes_test.cc    \
    unit_test/color_test.cc         \
    unit_test/compare_test.cc       \
    unit_test/convert_argb_test.cc  \
    unit_test/convert_test.cc       \
    unit_test/cpu_test.cc           \
    unit_test/cpu_thread_test.cc    \
    unit_test/math_test.cc          \
    unit_test/planar_test.cc        \
    unit_test/rotate_argb_test.cc   \
    unit_test/rotate_test.cc        \
    unit_test/scale_argb_test.cc    \
    unit_test/scale_plane_test.cc   \
    unit_test/scale_rgb_test.cc     \
    unit_test/scale_test.cc         \
    unit_test/scale_uv_test.cc      \
    unit_test/unit_test.cc          \
    unit_test/video_common_test.cc

LOCAL_MODULE := libyuv_unittest
include $(BUILD_NATIVE_TEST)
