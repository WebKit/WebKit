/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/file.h"

#include <utility>

namespace rtc {

namespace {

std::string NormalizePathname(Pathname&& path) {
  path.Normalize();
  return path.pathname();
}

}  // namespace

File::File(PlatformFile file) : file_(file) {}

File::File() : file_(kInvalidPlatformFileValue) {}

File::~File() {
  Close();
}

// static
File File::Open(const std::string& path) {
  return File(OpenPlatformFile(path));
}

// static
File File::Open(Pathname&& path) {
  return Open(NormalizePathname(std::move(path)));
}

// static
File File::Open(const Pathname& path) {
  return Open(Pathname(path));
}

// static
File File::Create(const std::string& path) {
  return File(CreatePlatformFile(path));
}

// static
File File::Create(Pathname&& path) {
  return Create(NormalizePathname(std::move(path)));
}

// static
File File::Create(const Pathname& path) {
  return Create(Pathname(path));
}

// static
bool File::Remove(const std::string& path) {
  return RemoveFile(path);
}

// static
bool File::Remove(Pathname&& path) {
  return Remove(NormalizePathname(std::move(path)));
}

// static
bool File::Remove(const Pathname& path) {
  return Remove(Pathname(path));
}

File::File(File&& other) : file_(other.file_) {
  other.file_ = kInvalidPlatformFileValue;
}

File& File::operator=(File&& other) {
  Close();
  file_ = other.file_;
  other.file_ = kInvalidPlatformFileValue;
  return *this;
}

bool File::IsOpen() {
  return file_ != kInvalidPlatformFileValue;
}

}  // namespace rtc
