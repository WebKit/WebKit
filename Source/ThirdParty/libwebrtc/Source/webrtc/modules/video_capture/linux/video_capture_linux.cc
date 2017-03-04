/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <new>

#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/video_capture/linux/video_capture_linux.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace videocapturemodule {
rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const char* deviceUniqueId) {
    rtc::scoped_refptr<VideoCaptureModuleV4L2> implementation(
        new rtc::RefCountedObject<VideoCaptureModuleV4L2>());

    if (implementation->Init(deviceUniqueId) != 0)
        return nullptr;

    return implementation;
}

VideoCaptureModuleV4L2::VideoCaptureModuleV4L2()
    : VideoCaptureImpl(),
      _captureCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _deviceId(-1),
      _deviceFd(-1),
      _buffersAllocatedByDevice(-1),
      _currentWidth(-1),
      _currentHeight(-1),
      _currentFrameRate(-1),
      _captureStarted(false),
      _captureVideoType(kVideoI420),
      _pool(NULL)
{
}

int32_t VideoCaptureModuleV4L2::Init(const char* deviceUniqueIdUTF8)
{
    int len = strlen((const char*) deviceUniqueIdUTF8);
    _deviceUniqueId = new (std::nothrow) char[len + 1];
    if (_deviceUniqueId)
    {
        memcpy(_deviceUniqueId, deviceUniqueIdUTF8, len + 1);
    }

    int fd;
    char device[32];
    bool found = false;

    /* detect /dev/video [0-63] entries */
    int n;
    for (n = 0; n < 64; n++)
    {
        sprintf(device, "/dev/video%d", n);
        if ((fd = open(device, O_RDONLY)) != -1)
        {
            // query device capabilities
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
            {
                if (cap.bus_info[0] != 0)
                {
                    if (strncmp((const char*) cap.bus_info,
                                (const char*) deviceUniqueIdUTF8,
                                strlen((const char*) deviceUniqueIdUTF8)) == 0) //match with device id
                    {
                        close(fd);
                        found = true;
                        break; // fd matches with device unique id supplied
                    }
                }
            }
            close(fd); // close since this is not the matching device
        }
    }
    if (!found)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                     0, "no matching device found");
        return -1;
    }
    _deviceId = n; //store the device id
    return 0;
}

VideoCaptureModuleV4L2::~VideoCaptureModuleV4L2()
{
    StopCapture();
    if (_captureCritSect)
    {
        delete _captureCritSect;
    }
    if (_deviceFd != -1)
      close(_deviceFd);
}

int32_t VideoCaptureModuleV4L2::StartCapture(
    const VideoCaptureCapability& capability)
{
    if (_captureStarted)
    {
        if (capability.width == _currentWidth &&
            capability.height == _currentHeight &&
            _captureVideoType == capability.rawType)
        {
            return 0;
        }
        else
        {
            StopCapture();
        }
    }

    CriticalSectionScoped cs(_captureCritSect);
    //first open /dev/video device
    char device[20];
    sprintf(device, "/dev/video%d", (int) _deviceId);

    if ((_deviceFd = open(device, O_RDWR | O_NONBLOCK, 0)) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "error in opening %s errono = %d", device, errno);
        return -1;
    }

    // Supported video formats in preferred order.
    // If the requested resolution is larger than VGA, we prefer MJPEG. Go for
    // I420 otherwise.
    const int nFormats = 5;
    unsigned int fmts[nFormats];
    if (capability.width > 640 || capability.height > 480) {
        fmts[0] = V4L2_PIX_FMT_MJPEG;
        fmts[1] = V4L2_PIX_FMT_YUV420;
        fmts[2] = V4L2_PIX_FMT_YUYV;
        fmts[3] = V4L2_PIX_FMT_UYVY;
        fmts[4] = V4L2_PIX_FMT_JPEG;
    } else {
        fmts[0] = V4L2_PIX_FMT_YUV420;
        fmts[1] = V4L2_PIX_FMT_YUYV;
        fmts[2] = V4L2_PIX_FMT_UYVY;
        fmts[3] = V4L2_PIX_FMT_MJPEG;
        fmts[4] = V4L2_PIX_FMT_JPEG;
    }

    // Enumerate image formats.
    struct v4l2_fmtdesc fmt;
    int fmtsIdx = nFormats;
    memset(&fmt, 0, sizeof(fmt));
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "Video Capture enumerats supported image formats:");
    while (ioctl(_deviceFd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "  { pixelformat = %c%c%c%c, description = '%s' }",
                     fmt.pixelformat & 0xFF, (fmt.pixelformat>>8) & 0xFF,
                     (fmt.pixelformat>>16) & 0xFF, (fmt.pixelformat>>24) & 0xFF,
                     fmt.description);
        // Match the preferred order.
        for (int i = 0; i < nFormats; i++) {
            if (fmt.pixelformat == fmts[i] && i < fmtsIdx)
                fmtsIdx = i;
        }
        // Keep enumerating.
        fmt.index++;
    }

    if (fmtsIdx == nFormats)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "no supporting video formats found");
        return -1;
    } else {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                     "We prefer format %c%c%c%c",
                     fmts[fmtsIdx] & 0xFF, (fmts[fmtsIdx]>>8) & 0xFF,
                     (fmts[fmtsIdx]>>16) & 0xFF, (fmts[fmtsIdx]>>24) & 0xFF);
    }

    struct v4l2_format video_fmt;
    memset(&video_fmt, 0, sizeof(struct v4l2_format));
    video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_fmt.fmt.pix.sizeimage = 0;
    video_fmt.fmt.pix.width = capability.width;
    video_fmt.fmt.pix.height = capability.height;
    video_fmt.fmt.pix.pixelformat = fmts[fmtsIdx];

    if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
        _captureVideoType = kVideoYUY2;
    else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
        _captureVideoType = kVideoI420;
    else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
        _captureVideoType = kVideoUYVY;
    else if (video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG ||
             video_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG)
        _captureVideoType = kVideoMJPEG;

    //set format and frame size now
    if (ioctl(_deviceFd, VIDIOC_S_FMT, &video_fmt) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "error in VIDIOC_S_FMT, errno = %d", errno);
        return -1;
    }

    // initialize current width and height
    _currentWidth = video_fmt.fmt.pix.width;
    _currentHeight = video_fmt.fmt.pix.height;

    // Trying to set frame rate, before check driver capability.
    bool driver_framerate_support = true;
    struct v4l2_streamparm streamparms;
    memset(&streamparms, 0, sizeof(streamparms));
    streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_deviceFd, VIDIOC_G_PARM, &streamparms) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "error in VIDIOC_G_PARM errno = %d", errno);
        driver_framerate_support = false;
      // continue
    } else {
      // check the capability flag is set to V4L2_CAP_TIMEPERFRAME.
      if (streamparms.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
        // driver supports the feature. Set required framerate.
        memset(&streamparms, 0, sizeof(streamparms));
        streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        streamparms.parm.capture.timeperframe.numerator = 1;
        streamparms.parm.capture.timeperframe.denominator = capability.maxFPS;
        if (ioctl(_deviceFd, VIDIOC_S_PARM, &streamparms) < 0) {
          WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "Failed to set the framerate. errno=%d", errno);
          driver_framerate_support = false;
        } else {
          _currentFrameRate = capability.maxFPS;
        }
      }
    }
    // If driver doesn't support framerate control, need to hardcode.
    // Hardcoding the value based on the frame size.
    if (!driver_framerate_support) {
      if(_currentWidth >= 800 && _captureVideoType != kVideoMJPEG) {
        _currentFrameRate = 15;
      } else {
        _currentFrameRate = 30;
      }
    }

    if (!AllocateVideoBuffers())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "failed to allocate video capture buffers");
        return -1;
    }

    //start capture thread;
    if (!_captureThread)
    {
        _captureThread.reset(new rtc::PlatformThread(
            VideoCaptureModuleV4L2::CaptureThread, this, "CaptureThread"));
        _captureThread->Start();
        _captureThread->SetPriority(rtc::kHighPriority);
    }

    // Needed to start UVC camera - from the uvcview application
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_deviceFd, VIDIOC_STREAMON, &type) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Failed to turn on stream");
        return -1;
    }

    _captureStarted = true;
    return 0;
}

int32_t VideoCaptureModuleV4L2::StopCapture()
{
    if (_captureThread) {
        // Make sure the capture thread stop stop using the critsect.
        _captureThread->Stop();
        _captureThread.reset();
    }

    CriticalSectionScoped cs(_captureCritSect);
    if (_captureStarted)
    {
        _captureStarted = false;

        DeAllocateVideoBuffers();
        close(_deviceFd);
        _deviceFd = -1;
    }

    return 0;
}

//critical section protected by the caller

bool VideoCaptureModuleV4L2::AllocateVideoBuffers()
{
    struct v4l2_requestbuffers rbuffer;
    memset(&rbuffer, 0, sizeof(v4l2_requestbuffers));

    rbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rbuffer.memory = V4L2_MEMORY_MMAP;
    rbuffer.count = kNoOfV4L2Bufffers;

    if (ioctl(_deviceFd, VIDIOC_REQBUFS, &rbuffer) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "Could not get buffers from device. errno = %d", errno);
        return false;
    }

    if (rbuffer.count > kNoOfV4L2Bufffers)
        rbuffer.count = kNoOfV4L2Bufffers;

    _buffersAllocatedByDevice = rbuffer.count;

    //Map the buffers
    _pool = new Buffer[rbuffer.count];

    for (unsigned int i = 0; i < rbuffer.count; i++)
    {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(v4l2_buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (ioctl(_deviceFd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            return false;
        }

        _pool[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                              _deviceFd, buffer.m.offset);

        if (MAP_FAILED == _pool[i].start)
        {
            for (unsigned int j = 0; j < i; j++)
                munmap(_pool[j].start, _pool[j].length);
            return false;
        }

        _pool[i].length = buffer.length;

        if (ioctl(_deviceFd, VIDIOC_QBUF, &buffer) < 0)
        {
            return false;
        }
    }
    return true;
}

bool VideoCaptureModuleV4L2::DeAllocateVideoBuffers()
{
    // unmap buffers
    for (int i = 0; i < _buffersAllocatedByDevice; i++)
        munmap(_pool[i].start, _pool[i].length);

    delete[] _pool;

    // turn off stream
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_deviceFd, VIDIOC_STREAMOFF, &type) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                   "VIDIOC_STREAMOFF error. errno: %d", errno);
    }

    return true;
}

bool VideoCaptureModuleV4L2::CaptureStarted()
{
    return _captureStarted;
}

bool VideoCaptureModuleV4L2::CaptureThread(void* obj)
{
    return static_cast<VideoCaptureModuleV4L2*> (obj)->CaptureProcess();
}
bool VideoCaptureModuleV4L2::CaptureProcess()
{
    int retVal = 0;
    fd_set rSet;
    struct timeval timeout;

    _captureCritSect->Enter();

    FD_ZERO(&rSet);
    FD_SET(_deviceFd, &rSet);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    retVal = select(_deviceFd + 1, &rSet, NULL, NULL, &timeout);
    if (retVal < 0 && errno != EINTR) // continue if interrupted
    {
        // select failed
        _captureCritSect->Leave();
        return false;
    }
    else if (retVal == 0)
    {
        // select timed out
        _captureCritSect->Leave();
        return true;
    }
    else if (!FD_ISSET(_deviceFd, &rSet))
    {
        // not event on camera handle
        _captureCritSect->Leave();
        return true;
    }

    if (_captureStarted)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        // dequeue a buffer - repeat until dequeued properly!
        while (ioctl(_deviceFd, VIDIOC_DQBUF, &buf) < 0)
        {
            if (errno != EINTR)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                           "could not sync on a buffer on device %s", strerror(errno));
                _captureCritSect->Leave();
                return true;
            }
        }
        VideoCaptureCapability frameInfo;
        frameInfo.width = _currentWidth;
        frameInfo.height = _currentHeight;
        frameInfo.rawType = _captureVideoType;

        // convert to to I420 if needed
        IncomingFrame((unsigned char*) _pool[buf.index].start,
                      buf.bytesused, frameInfo);
        // enqueue the buffer again
        if (ioctl(_deviceFd, VIDIOC_QBUF, &buf) == -1)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, 0,
                       "Failed to enqueue capture buffer");
        }
    }
    _captureCritSect->Leave();
    usleep(0);
    return true;
}

int32_t VideoCaptureModuleV4L2::CaptureSettings(VideoCaptureCapability& settings)
{
    settings.width = _currentWidth;
    settings.height = _currentHeight;
    settings.maxFPS = _currentFrameRate;
    settings.rawType=_captureVideoType;

    return 0;
}
}  // namespace videocapturemodule
}  // namespace webrtc
