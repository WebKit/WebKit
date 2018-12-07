/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

#if defined(WEBRTC_WIN)
#include <windows.h>

#define fileno _fileno
#include "rtc_base/stringutils.h"
#endif

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// StreamInterface
///////////////////////////////////////////////////////////////////////////////
StreamInterface::~StreamInterface() {}

StreamResult StreamInterface::WriteAll(const void* data,
                                       size_t data_len,
                                       size_t* written,
                                       int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_written = 0, current_written;
  while (total_written < data_len) {
    result = Write(static_cast<const char*>(data) + total_written,
                   data_len - total_written, &current_written, error);
    if (result != SR_SUCCESS)
      break;
    total_written += current_written;
  }
  if (written)
    *written = total_written;
  return result;
}

StreamResult StreamInterface::ReadAll(void* buffer,
                                      size_t buffer_len,
                                      size_t* read,
                                      int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_read = 0, current_read;
  while (total_read < buffer_len) {
    result = Read(static_cast<char*>(buffer) + total_read,
                  buffer_len - total_read, &current_read, error);
    if (result != SR_SUCCESS)
      break;
    total_read += current_read;
  }
  if (read)
    *read = total_read;
  return result;
}

void StreamInterface::PostEvent(Thread* t, int events, int err) {
  t->Post(RTC_FROM_HERE, this, MSG_POST_EVENT,
          new StreamEventData(events, err));
}

void StreamInterface::PostEvent(int events, int err) {
  PostEvent(Thread::Current(), events, err);
}

bool StreamInterface::SetPosition(size_t position) {
  return false;
}

bool StreamInterface::GetPosition(size_t* position) const {
  return false;
}

bool StreamInterface::GetSize(size_t* size) const {
  return false;
}

bool StreamInterface::Flush() {
  return false;
}

bool StreamInterface::ReserveSize(size_t size) {
  return true;
}

StreamInterface::StreamInterface() {}

void StreamInterface::OnMessage(Message* msg) {
  if (MSG_POST_EVENT == msg->message_id) {
    StreamEventData* pe = static_cast<StreamEventData*>(msg->pdata);
    SignalEvent(this, pe->events, pe->error);
    delete msg->pdata;
  }
}

///////////////////////////////////////////////////////////////////////////////
// StreamAdapterInterface
///////////////////////////////////////////////////////////////////////////////

StreamAdapterInterface::StreamAdapterInterface(StreamInterface* stream,
                                               bool owned)
    : stream_(stream), owned_(owned) {
  if (nullptr != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

StreamState StreamAdapterInterface::GetState() const {
  return stream_->GetState();
}
StreamResult StreamAdapterInterface::Read(void* buffer,
                                          size_t buffer_len,
                                          size_t* read,
                                          int* error) {
  return stream_->Read(buffer, buffer_len, read, error);
}
StreamResult StreamAdapterInterface::Write(const void* data,
                                           size_t data_len,
                                           size_t* written,
                                           int* error) {
  return stream_->Write(data, data_len, written, error);
}
void StreamAdapterInterface::Close() {
  stream_->Close();
}

bool StreamAdapterInterface::SetPosition(size_t position) {
  return stream_->SetPosition(position);
}

bool StreamAdapterInterface::GetPosition(size_t* position) const {
  return stream_->GetPosition(position);
}

bool StreamAdapterInterface::GetSize(size_t* size) const {
  return stream_->GetSize(size);
}

bool StreamAdapterInterface::ReserveSize(size_t size) {
  return stream_->ReserveSize(size);
}

bool StreamAdapterInterface::Flush() {
  return stream_->Flush();
}

void StreamAdapterInterface::Attach(StreamInterface* stream, bool owned) {
  if (nullptr != stream_)
    stream_->SignalEvent.disconnect(this);
  if (owned_)
    delete stream_;
  stream_ = stream;
  owned_ = owned;
  if (nullptr != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

StreamInterface* StreamAdapterInterface::Detach() {
  if (nullptr != stream_)
    stream_->SignalEvent.disconnect(this);
  StreamInterface* stream = stream_;
  stream_ = nullptr;
  return stream;
}

StreamAdapterInterface::~StreamAdapterInterface() {
  if (owned_)
    delete stream_;
}

void StreamAdapterInterface::OnEvent(StreamInterface* stream,
                                     int events,
                                     int err) {
  SignalEvent(this, events, err);
}

///////////////////////////////////////////////////////////////////////////////
// FileStream
///////////////////////////////////////////////////////////////////////////////

FileStream::FileStream() : file_(nullptr) {}

FileStream::~FileStream() {
  FileStream::Close();
}

bool FileStream::Open(const std::string& filename,
                      const char* mode,
                      int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfopen(wfilename.c_str(), ToUtf16(mode).c_str());
  } else {
    if (error) {
      *error = -1;
      return false;
    }
  }
#else
  file_ = fopen(filename.c_str(), mode);
#endif
  if (!file_ && error) {
    *error = errno;
  }
  return (file_ != nullptr);
}

bool FileStream::OpenShare(const std::string& filename,
                           const char* mode,
                           int shflag,
                           int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfsopen(wfilename.c_str(), ToUtf16(mode).c_str(), shflag);
    if (!file_ && error) {
      *error = errno;
      return false;
    }
    return file_ != nullptr;
  } else {
    if (error) {
      *error = -1;
    }
    return false;
  }
#else
  return Open(filename, mode, error);
#endif
}

bool FileStream::DisableBuffering() {
  if (!file_)
    return false;
  return (setvbuf(file_, nullptr, _IONBF, 0) == 0);
}

StreamState FileStream::GetState() const {
  return (file_ == nullptr) ? SS_CLOSED : SS_OPEN;
}

StreamResult FileStream::Read(void* buffer,
                              size_t buffer_len,
                              size_t* read,
                              int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fread(buffer, 1, buffer_len, file_);
  if ((result == 0) && (buffer_len > 0)) {
    if (feof(file_))
      return SR_EOS;
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (read)
    *read = result;
  return SR_SUCCESS;
}

StreamResult FileStream::Write(const void* data,
                               size_t data_len,
                               size_t* written,
                               int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fwrite(data, 1, data_len, file_);
  if ((result == 0) && (data_len > 0)) {
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (written)
    *written = result;
  return SR_SUCCESS;
}

void FileStream::Close() {
  if (file_) {
    DoClose();
    file_ = nullptr;
  }
}

bool FileStream::SetPosition(size_t position) {
  if (!file_)
    return false;
  return (fseek(file_, static_cast<int>(position), SEEK_SET) == 0);
}

bool FileStream::GetPosition(size_t* position) const {
  RTC_DCHECK(nullptr != position);
  if (!file_)
    return false;
  long result = ftell(file_);
  if (result < 0)
    return false;
  if (position)
    *position = result;
  return true;
}

bool FileStream::GetSize(size_t* size) const {
  RTC_DCHECK(nullptr != size);
  if (!file_)
    return false;
  struct stat file_stats;
  if (fstat(fileno(file_), &file_stats) != 0)
    return false;
  if (size)
    *size = file_stats.st_size;
  return true;
}

bool FileStream::ReserveSize(size_t size) {
  // TODO: extend the file to the proper length
  return true;
}

bool FileStream::Flush() {
  if (file_) {
    return (0 == fflush(file_));
  }
  // try to flush empty file?
  RTC_NOTREACHED();
  return false;
}

void FileStream::DoClose() {
  fclose(file_);
}

///////////////////////////////////////////////////////////////////////////////
// FifoBuffer
///////////////////////////////////////////////////////////////////////////////

FifoBuffer::FifoBuffer(size_t size)
    : state_(SS_OPEN),
      buffer_(new char[size]),
      buffer_length_(size),
      data_length_(0),
      read_position_(0),
      owner_(Thread::Current()) {
  // all events are done on the owner_ thread
}

FifoBuffer::FifoBuffer(size_t size, Thread* owner)
    : state_(SS_OPEN),
      buffer_(new char[size]),
      buffer_length_(size),
      data_length_(0),
      read_position_(0),
      owner_(owner) {
  // all events are done on the owner_ thread
}

FifoBuffer::~FifoBuffer() {}

bool FifoBuffer::GetBuffered(size_t* size) const {
  CritScope cs(&crit_);
  *size = data_length_;
  return true;
}

bool FifoBuffer::SetCapacity(size_t size) {
  CritScope cs(&crit_);
  if (data_length_ > size) {
    return false;
  }

  if (size != buffer_length_) {
    char* buffer = new char[size];
    const size_t copy = data_length_;
    const size_t tail_copy = std::min(copy, buffer_length_ - read_position_);
    memcpy(buffer, &buffer_[read_position_], tail_copy);
    memcpy(buffer + tail_copy, &buffer_[0], copy - tail_copy);
    buffer_.reset(buffer);
    read_position_ = 0;
    buffer_length_ = size;
  }
  return true;
}

StreamResult FifoBuffer::ReadOffset(void* buffer,
                                    size_t bytes,
                                    size_t offset,
                                    size_t* bytes_read) {
  CritScope cs(&crit_);
  return ReadOffsetLocked(buffer, bytes, offset, bytes_read);
}

StreamResult FifoBuffer::WriteOffset(const void* buffer,
                                     size_t bytes,
                                     size_t offset,
                                     size_t* bytes_written) {
  CritScope cs(&crit_);
  return WriteOffsetLocked(buffer, bytes, offset, bytes_written);
}

StreamState FifoBuffer::GetState() const {
  CritScope cs(&crit_);
  return state_;
}

StreamResult FifoBuffer::Read(void* buffer,
                              size_t bytes,
                              size_t* bytes_read,
                              int* error) {
  CritScope cs(&crit_);
  const bool was_writable = data_length_ < buffer_length_;
  size_t copy = 0;
  StreamResult result = ReadOffsetLocked(buffer, bytes, 0, &copy);

  if (result == SR_SUCCESS) {
    // If read was successful then adjust the read position and number of
    // bytes buffered.
    read_position_ = (read_position_ + copy) % buffer_length_;
    data_length_ -= copy;
    if (bytes_read) {
      *bytes_read = copy;
    }

    // if we were full before, and now we're not, post an event
    if (!was_writable && copy > 0) {
      PostEvent(owner_, SE_WRITE, 0);
    }
  }
  return result;
}

StreamResult FifoBuffer::Write(const void* buffer,
                               size_t bytes,
                               size_t* bytes_written,
                               int* error) {
  CritScope cs(&crit_);

  const bool was_readable = (data_length_ > 0);
  size_t copy = 0;
  StreamResult result = WriteOffsetLocked(buffer, bytes, 0, &copy);

  if (result == SR_SUCCESS) {
    // If write was successful then adjust the number of readable bytes.
    data_length_ += copy;
    if (bytes_written) {
      *bytes_written = copy;
    }

    // if we didn't have any data to read before, and now we do, post an event
    if (!was_readable && copy > 0) {
      PostEvent(owner_, SE_READ, 0);
    }
  }
  return result;
}

void FifoBuffer::Close() {
  CritScope cs(&crit_);
  state_ = SS_CLOSED;
}

const void* FifoBuffer::GetReadData(size_t* size) {
  CritScope cs(&crit_);
  *size = (read_position_ + data_length_ <= buffer_length_)
              ? data_length_
              : buffer_length_ - read_position_;
  return &buffer_[read_position_];
}

void FifoBuffer::ConsumeReadData(size_t size) {
  CritScope cs(&crit_);
  RTC_DCHECK(size <= data_length_);
  const bool was_writable = data_length_ < buffer_length_;
  read_position_ = (read_position_ + size) % buffer_length_;
  data_length_ -= size;
  if (!was_writable && size > 0) {
    PostEvent(owner_, SE_WRITE, 0);
  }
}

void* FifoBuffer::GetWriteBuffer(size_t* size) {
  CritScope cs(&crit_);
  if (state_ == SS_CLOSED) {
    return nullptr;
  }

  // if empty, reset the write position to the beginning, so we can get
  // the biggest possible block
  if (data_length_ == 0) {
    read_position_ = 0;
  }

  const size_t write_position =
      (read_position_ + data_length_) % buffer_length_;
  *size = (write_position > read_position_ || data_length_ == 0)
              ? buffer_length_ - write_position
              : read_position_ - write_position;
  return &buffer_[write_position];
}

void FifoBuffer::ConsumeWriteBuffer(size_t size) {
  CritScope cs(&crit_);
  RTC_DCHECK(size <= buffer_length_ - data_length_);
  const bool was_readable = (data_length_ > 0);
  data_length_ += size;
  if (!was_readable && size > 0) {
    PostEvent(owner_, SE_READ, 0);
  }
}

bool FifoBuffer::GetWriteRemaining(size_t* size) const {
  CritScope cs(&crit_);
  *size = buffer_length_ - data_length_;
  return true;
}

StreamResult FifoBuffer::ReadOffsetLocked(void* buffer,
                                          size_t bytes,
                                          size_t offset,
                                          size_t* bytes_read) {
  if (offset >= data_length_) {
    return (state_ != SS_CLOSED) ? SR_BLOCK : SR_EOS;
  }

  const size_t available = data_length_ - offset;
  const size_t read_position = (read_position_ + offset) % buffer_length_;
  const size_t copy = std::min(bytes, available);
  const size_t tail_copy = std::min(copy, buffer_length_ - read_position);
  char* const p = static_cast<char*>(buffer);
  memcpy(p, &buffer_[read_position], tail_copy);
  memcpy(p + tail_copy, &buffer_[0], copy - tail_copy);

  if (bytes_read) {
    *bytes_read = copy;
  }
  return SR_SUCCESS;
}

StreamResult FifoBuffer::WriteOffsetLocked(const void* buffer,
                                           size_t bytes,
                                           size_t offset,
                                           size_t* bytes_written) {
  if (state_ == SS_CLOSED) {
    return SR_EOS;
  }

  if (data_length_ + offset >= buffer_length_) {
    return SR_BLOCK;
  }

  const size_t available = buffer_length_ - data_length_ - offset;
  const size_t write_position =
      (read_position_ + data_length_ + offset) % buffer_length_;
  const size_t copy = std::min(bytes, available);
  const size_t tail_copy = std::min(copy, buffer_length_ - write_position);
  const char* const p = static_cast<const char*>(buffer);
  memcpy(&buffer_[write_position], p, tail_copy);
  memcpy(&buffer_[0], p + tail_copy, copy - tail_copy);

  if (bytes_written) {
    *bytes_written = copy;
  }
  return SR_SUCCESS;
}

}  // namespace rtc
