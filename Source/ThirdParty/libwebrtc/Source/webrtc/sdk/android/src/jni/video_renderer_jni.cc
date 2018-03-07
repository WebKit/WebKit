/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "api/video/video_frame.h"
#include "media/base/videosinkinterface.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videoframe.h"

namespace webrtc {
namespace jni {

// Wrapper dispatching rtc::VideoSinkInterface to a Java VideoRenderer
// instance.
class JavaVideoRendererWrapper : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  JavaVideoRendererWrapper(JNIEnv* jni, jobject j_callbacks)
      : j_callbacks_(jni, j_callbacks),
        j_render_frame_id_(
            GetMethodID(jni,
                        GetObjectClass(jni, j_callbacks),
                        "renderFrame",
                        "(Lorg/webrtc/VideoRenderer$I420Frame;)V")),
        j_frame_class_(jni,
                       FindClass(jni, "org/webrtc/VideoRenderer$I420Frame")),
        j_i420_frame_ctor_id_(GetMethodID(jni,
                                          *j_frame_class_,
                                          "<init>",
                                          "(III[I[Ljava/nio/ByteBuffer;J)V")),
        j_texture_frame_ctor_id_(
            GetMethodID(jni, *j_frame_class_, "<init>", "(IIII[FJ)V")),
        j_byte_buffer_class_(jni, FindClass(jni, "java/nio/ByteBuffer")) {
    CHECK_EXCEPTION(jni);
  }

  virtual ~JavaVideoRendererWrapper() {}

  void OnFrame(const VideoFrame& video_frame) override {
    ScopedLocalRefFrame local_ref_frame(jni());

    jobject j_frame;
    if (video_frame.video_frame_buffer()->type() ==
        VideoFrameBuffer::Type::kNative) {
      AndroidVideoFrameBuffer* android_buffer =
          static_cast<AndroidVideoFrameBuffer*>(
              video_frame.video_frame_buffer().get());
      switch (android_buffer->android_type()) {
        case AndroidVideoFrameBuffer::AndroidType::kTextureBuffer:
          j_frame = ToJavaTextureFrame(&video_frame);
          break;
        case AndroidVideoFrameBuffer::AndroidType::kJavaBuffer:
          j_frame = static_cast<AndroidVideoBuffer*>(android_buffer)
                        ->ToJavaI420Frame(jni(), video_frame.rotation());
          break;
        default:
          RTC_NOTREACHED();
      }
    } else {
      j_frame = ToJavaI420Frame(&video_frame);
    }
    // |j_callbacks_| is responsible for releasing |j_frame| with
    // VideoRenderer.renderFrameDone().
    jni()->CallVoidMethod(*j_callbacks_, j_render_frame_id_, j_frame);
    CHECK_EXCEPTION(jni());
  }

 private:
  // Make a shallow copy of |frame| to be used with Java. The callee has
  // ownership of the frame, and the frame should be released with
  // VideoRenderer.releaseNativeFrame().
  static jlong javaShallowCopy(const VideoFrame* frame) {
    return jlongFromPointer(new VideoFrame(*frame));
  }

  // Return a VideoRenderer.I420Frame referring to the data in |frame|.
  jobject ToJavaI420Frame(const VideoFrame* frame) {
    jintArray strides = jni()->NewIntArray(3);
    jint* strides_array = jni()->GetIntArrayElements(strides, NULL);
    rtc::scoped_refptr<I420BufferInterface> i420_buffer =
        frame->video_frame_buffer()->ToI420();
    strides_array[0] = i420_buffer->StrideY();
    strides_array[1] = i420_buffer->StrideU();
    strides_array[2] = i420_buffer->StrideV();
    jni()->ReleaseIntArrayElements(strides, strides_array, 0);
    jobjectArray planes = jni()->NewObjectArray(3, *j_byte_buffer_class_, NULL);
    jobject y_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8_t*>(i420_buffer->DataY()),
        i420_buffer->StrideY() * i420_buffer->height());
    size_t chroma_height = i420_buffer->ChromaHeight();
    jobject u_buffer =
        jni()->NewDirectByteBuffer(const_cast<uint8_t*>(i420_buffer->DataU()),
                                   i420_buffer->StrideU() * chroma_height);
    jobject v_buffer =
        jni()->NewDirectByteBuffer(const_cast<uint8_t*>(i420_buffer->DataV()),
                                   i420_buffer->StrideV() * chroma_height);

    jni()->SetObjectArrayElement(planes, 0, y_buffer);
    jni()->SetObjectArrayElement(planes, 1, u_buffer);
    jni()->SetObjectArrayElement(planes, 2, v_buffer);
    return jni()->NewObject(*j_frame_class_, j_i420_frame_ctor_id_,
                            frame->width(), frame->height(),
                            static_cast<int>(frame->rotation()), strides,
                            planes, javaShallowCopy(frame));
  }

  // Return a VideoRenderer.I420Frame referring texture object in |frame|.
  jobject ToJavaTextureFrame(const VideoFrame* frame) {
    NativeHandleImpl handle =
        static_cast<AndroidTextureBuffer*>(frame->video_frame_buffer().get())
            ->native_handle_impl();
    jfloatArray sampling_matrix = handle.sampling_matrix.ToJava(jni());

    return jni()->NewObject(
        *j_frame_class_, j_texture_frame_ctor_id_, frame->width(),
        frame->height(), static_cast<int>(frame->rotation()),
        handle.oes_texture_id, sampling_matrix, javaShallowCopy(frame));
  }

  JNIEnv* jni() { return AttachCurrentThreadIfNeeded(); }

  ScopedGlobalRef<jobject> j_callbacks_;
  jmethodID j_render_frame_id_;
  ScopedGlobalRef<jclass> j_frame_class_;
  jmethodID j_i420_frame_ctor_id_;
  jmethodID j_texture_frame_ctor_id_;
  ScopedGlobalRef<jclass> j_byte_buffer_class_;
};

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_freeWrappedVideoRenderer,
                         JNIEnv*,
                         jclass,
                         jlong j_p) {
  delete reinterpret_cast<JavaVideoRendererWrapper*>(j_p);
}

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_releaseNativeFrame,
                         JNIEnv* jni,
                         jclass,
                         jlong j_frame_ptr) {
  delete reinterpret_cast<const VideoFrame*>(j_frame_ptr);
}

JNI_FUNCTION_DECLARATION(jlong,
                         VideoRenderer_nativeWrapVideoRenderer,
                         JNIEnv* jni,
                         jclass,
                         jobject j_callbacks) {
  std::unique_ptr<JavaVideoRendererWrapper> renderer(
      new JavaVideoRendererWrapper(jni, j_callbacks));
  return (jlong)renderer.release();
}

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_nativeCopyPlane,
                         JNIEnv* jni,
                         jclass,
                         jobject j_src_buffer,
                         jint width,
                         jint height,
                         jint src_stride,
                         jobject j_dst_buffer,
                         jint dst_stride) {
  size_t src_size = jni->GetDirectBufferCapacity(j_src_buffer);
  size_t dst_size = jni->GetDirectBufferCapacity(j_dst_buffer);
  RTC_CHECK(src_stride >= width) << "Wrong source stride " << src_stride;
  RTC_CHECK(dst_stride >= width) << "Wrong destination stride " << dst_stride;
  RTC_CHECK(src_size >= src_stride * height)
      << "Insufficient source buffer capacity " << src_size;
  RTC_CHECK(dst_size >= dst_stride * height)
      << "Insufficient destination buffer capacity " << dst_size;
  uint8_t* src =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer));
  uint8_t* dst =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_buffer));
  if (src_stride == dst_stride) {
    memcpy(dst, src, src_stride * height);
  } else {
    for (int i = 0; i < height; i++) {
      memcpy(dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

}  // namespace jni
}  // namespace webrtc
