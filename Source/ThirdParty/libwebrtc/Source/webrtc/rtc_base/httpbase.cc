/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#else  // !WEBRTC_WIN
#define SEC_E_CERT_EXPIRED (-2146893016)
#endif  // !WEBRTC_WIN

#include "rtc_base/checks.h"
#include "rtc_base/httpbase.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/system/fallthrough.h"
#include "rtc_base/thread.h"

namespace rtc {

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

bool MatchHeader(const char* str, size_t len, HttpHeader header) {
  const char* const header_str = ToString(header);
  const size_t header_len = strlen(header_str);
  return (len == header_len) && (_strnicmp(str, header_str, header_len) == 0);
}

enum { MSG_READ };

//////////////////////////////////////////////////////////////////////
// HttpParser
//////////////////////////////////////////////////////////////////////

HttpParser::HttpParser() {
  reset();
}

HttpParser::~HttpParser() {}

void HttpParser::reset() {
  state_ = ST_LEADER;
  chunked_ = false;
  data_size_ = SIZE_UNKNOWN;
}

HttpParser::ProcessResult HttpParser::Process(const char* buffer,
                                              size_t len,
                                              size_t* processed,
                                              HttpError* error) {
  *processed = 0;
  *error = HE_NONE;

  if (state_ >= ST_COMPLETE) {
    RTC_NOTREACHED();
    return PR_COMPLETE;
  }

  while (true) {
    if (state_ < ST_DATA) {
      size_t pos = *processed;
      while ((pos < len) && (buffer[pos] != '\n')) {
        pos += 1;
      }
      if (pos >= len) {
        break;  // don't have a full header
      }
      const char* line = buffer + *processed;
      size_t len = (pos - *processed);
      *processed = pos + 1;
      while ((len > 0) && isspace(static_cast<unsigned char>(line[len - 1]))) {
        len -= 1;
      }
      ProcessResult result = ProcessLine(line, len, error);
      RTC_LOG(LS_VERBOSE) << "Processed line, result=" << result;

      if (PR_CONTINUE != result) {
        return result;
      }
    } else if (data_size_ == 0) {
      if (chunked_) {
        state_ = ST_CHUNKTERM;
      } else {
        return PR_COMPLETE;
      }
    } else {
      size_t available = len - *processed;
      if (available <= 0) {
        break;  // no more data
      }
      if ((data_size_ != SIZE_UNKNOWN) && (available > data_size_)) {
        available = data_size_;
      }
      size_t read = 0;
      ProcessResult result =
          ProcessData(buffer + *processed, available, read, error);
      RTC_LOG(LS_VERBOSE) << "Processed data, result: " << result
                          << " read: " << read << " err: " << error;

      if (PR_CONTINUE != result) {
        return result;
      }
      *processed += read;
      if (data_size_ != SIZE_UNKNOWN) {
        data_size_ -= read;
      }
    }
  }

  return PR_CONTINUE;
}

HttpParser::ProcessResult HttpParser::ProcessLine(const char* line,
                                                  size_t len,
                                                  HttpError* error) {
  RTC_LOG_F(LS_VERBOSE) << " state: " << state_
                        << " line: " << std::string(line, len)
                        << " len: " << len << " err: " << error;

  switch (state_) {
    case ST_LEADER:
      state_ = ST_HEADERS;
      return ProcessLeader(line, len, error);

    case ST_HEADERS:
      if (len > 0) {
        const char* value = strchrn(line, len, ':');
        if (!value) {
          *error = HE_PROTOCOL;
          return PR_COMPLETE;
        }
        size_t nlen = (value - line);
        const char* eol = line + len;
        do {
          value += 1;
        } while ((value < eol) && isspace(static_cast<unsigned char>(*value)));
        size_t vlen = eol - value;
        if (MatchHeader(line, nlen, HH_CONTENT_LENGTH)) {
          // sscanf isn't safe with strings that aren't null-terminated, and
          // there is no guarantee that |value| is. Create a local copy that is
          // null-terminated.
          std::string value_str(value, vlen);
          unsigned int temp_size;
          if (sscanf(value_str.c_str(), "%u", &temp_size) != 1) {
            *error = HE_PROTOCOL;
            return PR_COMPLETE;
          }
          data_size_ = static_cast<size_t>(temp_size);
        } else if (MatchHeader(line, nlen, HH_TRANSFER_ENCODING)) {
          if ((vlen == 7) && (_strnicmp(value, "chunked", 7) == 0)) {
            chunked_ = true;
          } else if ((vlen == 8) && (_strnicmp(value, "identity", 8) == 0)) {
            chunked_ = false;
          } else {
            *error = HE_PROTOCOL;
            return PR_COMPLETE;
          }
        }
        return ProcessHeader(line, nlen, value, vlen, error);
      } else {
        state_ = chunked_ ? ST_CHUNKSIZE : ST_DATA;
        return ProcessHeaderComplete(chunked_, data_size_, error);
      }
      break;

    case ST_CHUNKSIZE:
      if (len > 0) {
        char* ptr = nullptr;
        data_size_ = strtoul(line, &ptr, 16);
        if (ptr != line + len) {
          *error = HE_PROTOCOL;
          return PR_COMPLETE;
        }
        state_ = (data_size_ == 0) ? ST_TRAILERS : ST_DATA;
      } else {
        *error = HE_PROTOCOL;
        return PR_COMPLETE;
      }
      break;

    case ST_CHUNKTERM:
      if (len > 0) {
        *error = HE_PROTOCOL;
        return PR_COMPLETE;
      } else {
        state_ = chunked_ ? ST_CHUNKSIZE : ST_DATA;
      }
      break;

    case ST_TRAILERS:
      if (len == 0) {
        return PR_COMPLETE;
      }
      // *error = onHttpRecvTrailer();
      break;

    default:
      RTC_NOTREACHED();
      break;
  }

  return PR_CONTINUE;
}

bool HttpParser::is_valid_end_of_input() const {
  return (state_ == ST_DATA) && (data_size_ == SIZE_UNKNOWN);
}

void HttpParser::complete(HttpError error) {
  if (state_ < ST_COMPLETE) {
    state_ = ST_COMPLETE;
    OnComplete(error);
  }
}

//////////////////////////////////////////////////////////////////////
// HttpBase
//////////////////////////////////////////////////////////////////////

HttpBase::HttpBase()
    : mode_(HM_NONE), data_(nullptr), notify_(nullptr), http_stream_(nullptr) {}

HttpBase::~HttpBase() {
  RTC_DCHECK(HM_NONE == mode_);
}

bool HttpBase::isConnected() const {
  return (http_stream_ != nullptr) && (http_stream_->GetState() == SS_OPEN);
}

bool HttpBase::attach(StreamInterface* stream) {
  if ((mode_ != HM_NONE) || (http_stream_ != nullptr) || (stream == nullptr)) {
    RTC_NOTREACHED();
    return false;
  }
  http_stream_ = stream;
  http_stream_->SignalEvent.connect(this, &HttpBase::OnHttpStreamEvent);
  mode_ = (http_stream_->GetState() == SS_OPENING) ? HM_CONNECT : HM_NONE;
  return true;
}

StreamInterface* HttpBase::detach() {
  RTC_DCHECK(HM_NONE == mode_);
  if (mode_ != HM_NONE) {
    return nullptr;
  }
  StreamInterface* stream = http_stream_;
  http_stream_ = nullptr;
  if (stream) {
    stream->SignalEvent.disconnect(this);
  }
  return stream;
}

void HttpBase::send(HttpData* data) {
  RTC_DCHECK(HM_NONE == mode_);
  if (mode_ != HM_NONE) {
    return;
  } else if (!isConnected()) {
    OnHttpStreamEvent(http_stream_, SE_CLOSE, HE_DISCONNECTED);
    return;
  }

  mode_ = HM_SEND;
  data_ = data;
  len_ = 0;
  ignore_data_ = chunk_data_ = false;

  if (data_->document) {
    data_->document->SignalEvent.connect(this, &HttpBase::OnDocumentEvent);
  }

  std::string encoding;
  if (data_->hasHeader(HH_TRANSFER_ENCODING, &encoding) &&
      (encoding == "chunked")) {
    chunk_data_ = true;
  }

  len_ = data_->formatLeader(buffer_, sizeof(buffer_));
  len_ += strcpyn(buffer_ + len_, sizeof(buffer_) - len_, "\r\n");

  header_ = data_->begin();
  if (header_ == data_->end()) {
    // We must call this at least once, in the case where there are no headers.
    queue_headers();
  }

  flush_data();
}

void HttpBase::recv(HttpData* data) {
  RTC_DCHECK(HM_NONE == mode_);
  if (mode_ != HM_NONE) {
    return;
  } else if (!isConnected()) {
    OnHttpStreamEvent(http_stream_, SE_CLOSE, HE_DISCONNECTED);
    return;
  }

  mode_ = HM_RECV;
  data_ = data;
  len_ = 0;
  ignore_data_ = chunk_data_ = false;

  reset();
  read_and_process_data();
}

void HttpBase::abort(HttpError err) {
  if (mode_ != HM_NONE) {
    if (http_stream_ != nullptr) {
      http_stream_->Close();
    }
    do_complete(err);
  }
}

HttpError HttpBase::HandleStreamClose(int error) {
  if (http_stream_ != nullptr) {
    http_stream_->Close();
  }
  if (error == 0) {
    if ((mode_ == HM_RECV) && is_valid_end_of_input()) {
      return HE_NONE;
    } else {
      return HE_DISCONNECTED;
    }
  } else if (error == SOCKET_EACCES) {
    return HE_AUTH;
  } else if (error == SEC_E_CERT_EXPIRED) {
    return HE_CERTIFICATE_EXPIRED;
  }
  RTC_LOG_F(LS_ERROR) << "(" << error << ")";
  return (HM_CONNECT == mode_) ? HE_CONNECT_FAILED : HE_SOCKET_ERROR;
}

bool HttpBase::DoReceiveLoop(HttpError* error) {
  RTC_DCHECK(HM_RECV == mode_);
  RTC_DCHECK(nullptr != error);

  // Do to the latency between receiving read notifications from
  // pseudotcpchannel, we rely on repeated calls to read in order to acheive
  // ideal throughput.  The number of reads is limited to prevent starving
  // the caller.

  size_t loop_count = 0;
  const size_t kMaxReadCount = 20;
  bool process_requires_more_data = false;
  do {
    // The most frequent use of this function is response to new data available
    // on http_stream_.  Therefore, we optimize by attempting to read from the
    // network first (as opposed to processing existing data first).

    if (len_ < sizeof(buffer_)) {
      // Attempt to buffer more data.
      size_t read;
      int read_error;
      StreamResult read_result = http_stream_->Read(
          buffer_ + len_, sizeof(buffer_) - len_, &read, &read_error);
      switch (read_result) {
        case SR_SUCCESS:
          RTC_DCHECK(len_ + read <= sizeof(buffer_));
          len_ += read;
          break;
        case SR_BLOCK:
          if (process_requires_more_data) {
            // We're can't make progress until more data is available.
            return false;
          }
          // Attempt to process the data already in our buffer.
          break;
        case SR_EOS:
          // Clean close, with no error.
          read_error = 0;
          RTC_FALLTHROUGH();  // Fall through to HandleStreamClose.
        case SR_ERROR:
          *error = HandleStreamClose(read_error);
          return true;
      }
    } else if (process_requires_more_data) {
      // We have too much unprocessed data in our buffer.  This should only
      // occur when a single HTTP header is longer than the buffer size (32K).
      // Anything longer than that is almost certainly an error.
      *error = HE_OVERFLOW;
      return true;
    }

    // Process data in our buffer.  Process is not guaranteed to process all
    // the buffered data.  In particular, it will wait until a complete
    // protocol element (such as http header, or chunk size) is available,
    // before processing it in its entirety.  Also, it is valid and sometimes
    // necessary to call Process with an empty buffer, since the state machine
    // may have interrupted state transitions to complete.
    size_t processed;
    ProcessResult process_result = Process(buffer_, len_, &processed, error);
    RTC_DCHECK(processed <= len_);
    len_ -= processed;
    memmove(buffer_, buffer_ + processed, len_);
    switch (process_result) {
      case PR_CONTINUE:
        // We need more data to make progress.
        process_requires_more_data = true;
        break;
      case PR_BLOCK:
        // We're stalled on writing the processed data.
        return false;
      case PR_COMPLETE:
        // *error already contains the correct code.
        return true;
    }
  } while (++loop_count <= kMaxReadCount);

  RTC_LOG_F(LS_WARNING) << "danger of starvation";
  return false;
}

void HttpBase::read_and_process_data() {
  HttpError error;
  if (DoReceiveLoop(&error)) {
    complete(error);
  }
}

void HttpBase::flush_data() {
  RTC_DCHECK(HM_SEND == mode_);

  // When send_required is true, no more buffering can occur without a network
  // write.
  bool send_required = (len_ >= sizeof(buffer_));

  while (true) {
    RTC_DCHECK(len_ <= sizeof(buffer_));

    // HTTP is inherently sensitive to round trip latency, since a frequent use
    // case is for small requests and responses to be sent back and forth, and
    // the lack of pipelining forces a single request to take a minimum of the
    // round trip time.  As a result, it is to our benefit to pack as much data
    // into each packet as possible.  Thus, we defer network writes until we've
    // buffered as much data as possible.

    if (!send_required && (header_ != data_->end())) {
      // First, attempt to queue more header data.
      send_required = queue_headers();
    }

    if (!send_required && data_->document) {
      // Next, attempt to queue document data.

      const size_t kChunkDigits = 8;
      size_t offset, reserve;
      if (chunk_data_) {
        // Reserve characters at the start for X-byte hex value and \r\n
        offset = len_ + kChunkDigits + 2;
        // ... and 2 characters at the end for \r\n
        reserve = offset + 2;
      } else {
        offset = len_;
        reserve = offset;
      }

      if (reserve >= sizeof(buffer_)) {
        send_required = true;
      } else {
        size_t read;
        int error;
        StreamResult result = data_->document->Read(
            buffer_ + offset, sizeof(buffer_) - reserve, &read, &error);
        if (result == SR_SUCCESS) {
          RTC_DCHECK(reserve + read <= sizeof(buffer_));
          if (chunk_data_) {
            // Prepend the chunk length in hex.
            // Note: sprintfn appends a null terminator, which is why we can't
            // combine it with the line terminator.
            sprintfn(buffer_ + len_, kChunkDigits + 1, "%.*x", kChunkDigits,
                     read);
            // Add line terminator to the chunk length.
            memcpy(buffer_ + len_ + kChunkDigits, "\r\n", 2);
            // Add line terminator to the end of the chunk.
            memcpy(buffer_ + offset + read, "\r\n", 2);
          }
          len_ = reserve + read;
        } else if (result == SR_BLOCK) {
          // Nothing to do but flush data to the network.
          send_required = true;
        } else if (result == SR_EOS) {
          if (chunk_data_) {
            // Append the empty chunk and empty trailers, then turn off
            // chunking.
            RTC_DCHECK(len_ + 5 <= sizeof(buffer_));
            memcpy(buffer_ + len_, "0\r\n\r\n", 5);
            len_ += 5;
            chunk_data_ = false;
          } else if (0 == len_) {
            // No more data to read, and no more data to write.
            do_complete();
            return;
          }
          // Although we are done reading data, there is still data which needs
          // to be flushed to the network.
          send_required = true;
        } else {
          RTC_LOG_F(LS_ERROR) << "Read error: " << error;
          do_complete(HE_STREAM);
          return;
        }
      }
    }

    if (0 == len_) {
      // No data currently available to send.
      if (!data_->document) {
        // If there is no source document, that means we're done.
        do_complete();
      }
      return;
    }

    size_t written;
    int error;
    StreamResult result = http_stream_->Write(buffer_, len_, &written, &error);
    if (result == SR_SUCCESS) {
      RTC_DCHECK(written <= len_);
      len_ -= written;
      memmove(buffer_, buffer_ + written, len_);
      send_required = false;
    } else if (result == SR_BLOCK) {
      if (send_required) {
        // Nothing more we can do until network is writeable.
        return;
      }
    } else {
      RTC_DCHECK(result == SR_ERROR);
      RTC_LOG_F(LS_ERROR) << "error";
      OnHttpStreamEvent(http_stream_, SE_CLOSE, error);
      return;
    }
  }

  RTC_NOTREACHED();
}

bool HttpBase::queue_headers() {
  RTC_DCHECK(HM_SEND == mode_);
  while (header_ != data_->end()) {
    size_t len =
        sprintfn(buffer_ + len_, sizeof(buffer_) - len_, "%.*s: %.*s\r\n",
                 header_->first.size(), header_->first.data(),
                 header_->second.size(), header_->second.data());
    if (len_ + len < sizeof(buffer_) - 3) {
      len_ += len;
      ++header_;
    } else if (len_ == 0) {
      RTC_LOG(WARNING) << "discarding header that is too long: "
                       << header_->first;
      ++header_;
    } else {
      // Not enough room for the next header, write to network first.
      return true;
    }
  }
  // End of headers
  len_ += strcpyn(buffer_ + len_, sizeof(buffer_) - len_, "\r\n");
  return false;
}

void HttpBase::do_complete(HttpError err) {
  RTC_DCHECK(mode_ != HM_NONE);
  HttpMode mode = mode_;
  mode_ = HM_NONE;
  if (data_ && data_->document) {
    data_->document->SignalEvent.disconnect(this);
  }
  data_ = nullptr;
  if (notify_) {
    notify_->onHttpComplete(mode, err);
  }
}

//
// Stream Signals
//

void HttpBase::OnHttpStreamEvent(StreamInterface* stream,
                                 int events,
                                 int error) {
  RTC_DCHECK(stream == http_stream_);
  if ((events & SE_OPEN) && (mode_ == HM_CONNECT)) {
    do_complete();
    return;
  }

  if ((events & SE_WRITE) && (mode_ == HM_SEND)) {
    flush_data();
    return;
  }

  if ((events & SE_READ) && (mode_ == HM_RECV)) {
    read_and_process_data();
    return;
  }

  if ((events & SE_CLOSE) == 0)
    return;

  HttpError http_error = HandleStreamClose(error);
  if (mode_ == HM_RECV) {
    complete(http_error);
  } else if (mode_ != HM_NONE) {
    do_complete(http_error);
  } else if (notify_) {
    notify_->onHttpClosed(http_error);
  }
}

void HttpBase::OnDocumentEvent(StreamInterface* stream, int events, int error) {
  RTC_DCHECK(stream == data_->document.get());
  if ((events & SE_WRITE) && (mode_ == HM_RECV)) {
    read_and_process_data();
    return;
  }

  if ((events & SE_READ) && (mode_ == HM_SEND)) {
    flush_data();
    return;
  }

  if (events & SE_CLOSE) {
    RTC_LOG_F(LS_ERROR) << "Read error: " << error;
    do_complete(HE_STREAM);
    return;
  }
}

//
// HttpParser Implementation
//

HttpParser::ProcessResult HttpBase::ProcessLeader(const char* line,
                                                  size_t len,
                                                  HttpError* error) {
  *error = data_->parseLeader(line, len);
  return (HE_NONE == *error) ? PR_CONTINUE : PR_COMPLETE;
}

HttpParser::ProcessResult HttpBase::ProcessHeader(const char* name,
                                                  size_t nlen,
                                                  const char* value,
                                                  size_t vlen,
                                                  HttpError* error) {
  std::string sname(name, nlen), svalue(value, vlen);
  data_->addHeader(sname, svalue);
  return PR_CONTINUE;
}

HttpParser::ProcessResult HttpBase::ProcessHeaderComplete(bool chunked,
                                                          size_t& data_size,
                                                          HttpError* error) {
  if (notify_) {
    *error = notify_->onHttpHeaderComplete(chunked, data_size);
    // The request must not be aborted as a result of this callback.
    RTC_DCHECK(nullptr != data_);
  }
  if ((HE_NONE == *error) && data_->document) {
    data_->document->SignalEvent.connect(this, &HttpBase::OnDocumentEvent);
  }
  if (HE_NONE != *error) {
    return PR_COMPLETE;
  }
  return PR_CONTINUE;
}

HttpParser::ProcessResult HttpBase::ProcessData(const char* data,
                                                size_t len,
                                                size_t& read,
                                                HttpError* error) {
  if (ignore_data_ || !data_->document) {
    read = len;
    return PR_CONTINUE;
  }
  int write_error = 0;
  switch (data_->document->Write(data, len, &read, &write_error)) {
    case SR_SUCCESS:
      return PR_CONTINUE;
    case SR_BLOCK:
      return PR_BLOCK;
    case SR_EOS:
      RTC_LOG_F(LS_ERROR) << "Unexpected EOS";
      *error = HE_STREAM;
      return PR_COMPLETE;
    case SR_ERROR:
    default:
      RTC_LOG_F(LS_ERROR) << "Write error: " << write_error;
      *error = HE_STREAM;
      return PR_COMPLETE;
  }
}

void HttpBase::OnComplete(HttpError err) {
  RTC_LOG_F(LS_VERBOSE);
  do_complete(err);
}

}  // namespace rtc
