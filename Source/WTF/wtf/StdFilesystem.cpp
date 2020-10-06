//===------------------------- StdFilesystem.cpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE-libc++.txt for details.
//
//===----------------------------------------------------------------------===//

/* Imports a copy of filesystem/filesystem_common.h, filesystem/directory_iterator.cpp,
   filesystem/int128_builtins.cpp and filesystem/operations.cpp from r343838 of the
   libc++ project and has modified them to work outside of a build of libc++ itself.
*/

#include "config.h"
#include <wtf/Platform.h>

#if PLATFORM(MAC)

#include <Availability.h>

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101500

#include "StdFilesystem.h"

#include <__config>

/* -- BEGIN filesystem/filesystem_common.h -- */

#include <array>
#include <chrono>
#include <cstdlib>
#include <climits>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h> // for ::utimes as used in __last_write_time
#include <fcntl.h>    /* values for fchmodat */

/* #include "../include/apple_availability.h" */

#if !defined(__APPLE__)
// We can use the presence of UTIME_OMIT to detect platforms that provide
// utimensat.
#if defined(UTIME_OMIT)
#define _LIBCPP_USE_UTIMENSAT
#endif
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

_LIBCPP_BEGIN_NAMESPACE_FILESYSTEM

namespace detail {
namespace {

static string format_string_imp(const char* msg, ...) {
  // we might need a second shot at this, so pre-emptivly make a copy
  struct GuardVAList {
    va_list& target;
    bool active = true;
    GuardVAList(va_list& target) : target(target), active(true) {}
    void clear() {
      if (active)
        va_end(target);
      active = false;
    }
    ~GuardVAList() {
      if (active)
        va_end(target);
    }
  };
  va_list args;
  va_start(args, msg);
  GuardVAList args_guard(args);

  va_list args_cp;
  va_copy(args_cp, args);
  GuardVAList args_copy_guard(args_cp);

  array<char, 256> local_buff;
  size_t size = local_buff.size();

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
  auto ret = ::vsnprintf(local_buff.data(), size, msg, args_cp);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  args_copy_guard.clear();

  // handle empty expansion
  if (ret == 0)
    return string{};
  if (static_cast<size_t>(ret) < size)
    return string(local_buff.data());

  // we did not provide a long enough buffer on our first attempt.
  // add 1 to size to account for null-byte in size cast to prevent overflow
  size = static_cast<size_t>(ret) + 1;
  auto buff_ptr = unique_ptr<char[]>(new char[size]);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
  ret = ::vsnprintf(buff_ptr.get(), size, msg, args);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  return string(buff_ptr.get());
}

const char* unwrap(string const& s) { return s.c_str(); }
const char* unwrap(path const& p) { return p.native().c_str(); }
template <class Arg>
Arg const& unwrap(Arg const& a) {
  static_assert(!is_class<Arg>::value, "cannot pass class here");
  return a;
}

template <class... Args>
string format_string(const char* fmt, Args const&... args) {
  return format_string_imp(fmt, unwrap(args)...);
}

error_code capture_errno() {
  _LIBCPP_ASSERT(errno, "Expected errno to be non-zero");
  return error_code(errno, generic_category());
}

template <class T>
T error_value();
template <>
_LIBCPP_CONSTEXPR_AFTER_CXX11 void error_value<void>() {}
template <>
bool error_value<bool>() {
  return false;
}
template <>
uintmax_t error_value<uintmax_t>() {
  return uintmax_t(-1);
}
template <>
_LIBCPP_CONSTEXPR_AFTER_CXX11 file_time_type error_value<file_time_type>() {
  return file_time_type::min();
}
template <>
path error_value<path>() {
  return {};
}

template <class T>
struct ErrorHandler {
  const char* func_name;
  error_code* ec = nullptr;
  const path* p1 = nullptr;
  const path* p2 = nullptr;

  ErrorHandler(const char* fname, error_code* ec, const path* p1 = nullptr,
               const path* p2 = nullptr)
      : func_name(fname), ec(ec), p1(p1), p2(p2) {
    if (ec)
      ec->clear();
  }

  T report(const error_code& m_ec) const {
    if (ec) {
      *ec = m_ec;
      return error_value<T>();
    }
    string what = string("in ") + func_name;
    switch (bool(p1) + bool(p2)) {
    case 0:
      __throw_filesystem_error(what, m_ec);
    case 1:
      __throw_filesystem_error(what, *p1, m_ec);
    case 2:
      __throw_filesystem_error(what, *p1, *p2, m_ec);
    }
    _LIBCPP_UNREACHABLE();
  }

  template <class... Args>
  T report(const error_code& m_ec, const char* msg, Args const&... args) const {
    if (ec) {
      *ec = m_ec;
      return error_value<T>();
    }
    string what =
        string("in ") + func_name + ": " + format_string(msg, args...);
    switch (bool(p1) + bool(p2)) {
    case 0:
      __throw_filesystem_error(what, m_ec);
    case 1:
      __throw_filesystem_error(what, *p1, m_ec);
    case 2:
      __throw_filesystem_error(what, *p1, *p2, m_ec);
    }
    _LIBCPP_UNREACHABLE();
  }

  T report(errc const& err) const { return report(make_error_code(err)); }

  template <class... Args>
  T report(errc const& err, const char* msg, Args const&... args) const {
    return report(make_error_code(err), msg, args...);
  }

private:
  ErrorHandler(ErrorHandler const&) = delete;
  ErrorHandler& operator=(ErrorHandler const&) = delete;
};

using chrono::duration;
using chrono::duration_cast;

using TimeSpec = struct ::timespec;
using StatT = struct ::stat;

template <class FileTimeT, class TimeT,
          bool IsFloat = is_floating_point<typename FileTimeT::rep>::value>
struct time_util_base {
  using rep = typename FileTimeT::rep;
  using fs_duration = typename FileTimeT::duration;
  using fs_seconds = duration<rep>;
  using fs_nanoseconds = duration<rep, nano>;
  using fs_microseconds = duration<rep, micro>;

  static constexpr rep max_seconds =
      duration_cast<fs_seconds>(FileTimeT::duration::max()).count();

  static constexpr rep max_nsec =
      duration_cast<fs_nanoseconds>(FileTimeT::duration::max() -
                                    fs_seconds(max_seconds))
          .count();

  static constexpr rep min_seconds =
      duration_cast<fs_seconds>(FileTimeT::duration::min()).count();

  static constexpr rep min_nsec_timespec =
      duration_cast<fs_nanoseconds>(
          (FileTimeT::duration::min() - fs_seconds(min_seconds)) +
          fs_seconds(1))
          .count();

private:
#if _LIBCPP_STD_VER > 11 && !defined(_LIBCPP_HAS_NO_CXX14_CONSTEXPR)
  static constexpr fs_duration get_min_nsecs() {
    return duration_cast<fs_duration>(
        fs_nanoseconds(min_nsec_timespec) -
        duration_cast<fs_nanoseconds>(fs_seconds(1)));
  }
  // Static assert that these values properly round trip.
  static_assert(fs_seconds(min_seconds) + get_min_nsecs() ==
                    FileTimeT::duration::min(),
                "value doesn't roundtrip");

  static constexpr bool check_range() {
    // This kinda sucks, but it's what happens when we don't have __int128_t.
    if (sizeof(TimeT) == sizeof(rep)) {
      typedef duration<long long, ratio<3600 * 24 * 365> > Years;
      return duration_cast<Years>(fs_seconds(max_seconds)) > Years(250) &&
             duration_cast<Years>(fs_seconds(min_seconds)) < Years(-250);
    }
    return max_seconds >= numeric_limits<TimeT>::max() &&
           min_seconds <= numeric_limits<TimeT>::min();
  }
  static_assert(check_range(), "the representable range is unacceptable small");
#endif
};

template <class FileTimeT, class TimeT>
struct time_util_base<FileTimeT, TimeT, true> {
  using rep = typename FileTimeT::rep;
  using fs_duration = typename FileTimeT::duration;
  using fs_seconds = duration<rep>;
  using fs_nanoseconds = duration<rep, nano>;
  using fs_microseconds = duration<rep, micro>;

  static const rep max_seconds;
  static const rep max_nsec;
  static const rep min_seconds;
  static const rep min_nsec_timespec;
};

template <class FileTimeT, class TimeT>
const typename FileTimeT::rep
    time_util_base<FileTimeT, TimeT, true>::max_seconds =
        duration_cast<fs_seconds>(FileTimeT::duration::max()).count();

template <class FileTimeT, class TimeT>
const typename FileTimeT::rep time_util_base<FileTimeT, TimeT, true>::max_nsec =
    duration_cast<fs_nanoseconds>(FileTimeT::duration::max() -
                                  fs_seconds(max_seconds))
        .count();

template <class FileTimeT, class TimeT>
const typename FileTimeT::rep
    time_util_base<FileTimeT, TimeT, true>::min_seconds =
        duration_cast<fs_seconds>(FileTimeT::duration::min()).count();

template <class FileTimeT, class TimeT>
const typename FileTimeT::rep
    time_util_base<FileTimeT, TimeT, true>::min_nsec_timespec =
        duration_cast<fs_nanoseconds>((FileTimeT::duration::min() -
                                       fs_seconds(min_seconds)) +
                                      fs_seconds(1))
            .count();

template <class FileTimeT, class TimeT, class TimeSpecT>
struct time_util : time_util_base<FileTimeT, TimeT> {
  using Base = time_util_base<FileTimeT, TimeT>;
  using Base::max_nsec;
  using Base::max_seconds;
  using Base::min_nsec_timespec;
  using Base::min_seconds;

  using typename Base::fs_duration;
  using typename Base::fs_microseconds;
  using typename Base::fs_nanoseconds;
  using typename Base::fs_seconds;

public:
  template <class CType, class ChronoType>
  static _LIBCPP_CONSTEXPR_AFTER_CXX11 bool checked_set(CType* out,
                                                        ChronoType time) {
    using Lim = numeric_limits<CType>;
    if (time > Lim::max() || time < Lim::min())
      return false;
    *out = static_cast<CType>(time);
    return true;
  }

  static _LIBCPP_CONSTEXPR_AFTER_CXX11 bool is_representable(TimeSpecT tm) {
    if (tm.tv_sec >= 0) {
      return tm.tv_sec < max_seconds ||
             (tm.tv_sec == max_seconds && tm.tv_nsec <= max_nsec);
    } else if (tm.tv_sec == (min_seconds - 1)) {
      return tm.tv_nsec >= min_nsec_timespec;
    } else {
      return tm.tv_sec >= min_seconds;
    }
  }

  static _LIBCPP_CONSTEXPR_AFTER_CXX11 bool is_representable(FileTimeT tm) {
    auto secs = duration_cast<fs_seconds>(tm.time_since_epoch());
    auto nsecs = duration_cast<fs_nanoseconds>(tm.time_since_epoch() - secs);
    if (nsecs.count() < 0) {
      secs = secs + fs_seconds(1);
      nsecs = nsecs + fs_seconds(1);
    }
    using TLim = numeric_limits<TimeT>;
    if (secs.count() >= 0)
      return secs.count() <= TLim::max();
    return secs.count() >= TLim::min();
  }

  static _LIBCPP_CONSTEXPR_AFTER_CXX11 FileTimeT
  convert_from_timespec(TimeSpecT tm) {
    if (tm.tv_sec >= 0 || tm.tv_nsec == 0) {
      return FileTimeT(fs_seconds(tm.tv_sec) +
                       duration_cast<fs_duration>(fs_nanoseconds(tm.tv_nsec)));
    } else { // tm.tv_sec < 0
      auto adj_subsec = duration_cast<fs_duration>(fs_seconds(1) -
                                                   fs_nanoseconds(tm.tv_nsec));
      auto Dur = fs_seconds(tm.tv_sec + 1) - adj_subsec;
      return FileTimeT(Dur);
    }
  }

  template <class SubSecT>
  static _LIBCPP_CONSTEXPR_AFTER_CXX11 bool
  set_times_checked(TimeT* sec_out, SubSecT* subsec_out, FileTimeT tp) {
    auto dur = tp.time_since_epoch();
    auto sec_dur = duration_cast<fs_seconds>(dur);
    auto subsec_dur = duration_cast<fs_nanoseconds>(dur - sec_dur);
    // The tv_nsec and tv_usec fields must not be negative so adjust accordingly
    if (subsec_dur.count() < 0) {
      if (sec_dur.count() > min_seconds) {
        sec_dur = sec_dur - fs_seconds(1);
        subsec_dur = subsec_dur + fs_seconds(1);
      } else {
        subsec_dur = fs_nanoseconds::zero();
      }
    }
    return checked_set(sec_out, sec_dur.count()) &&
           checked_set(subsec_out, subsec_dur.count());
  }
  static _LIBCPP_CONSTEXPR_AFTER_CXX11 bool convert_to_timespec(TimeSpecT& dest,
                                                                FileTimeT tp) {
    if (!is_representable(tp))
      return false;
    return set_times_checked(&dest.tv_sec, &dest.tv_nsec, tp);
  }
};

using fs_time = time_util<file_time_type, time_t, TimeSpec>;

#if defined(__APPLE__)
TimeSpec extract_mtime(StatT const& st) { return st.st_mtimespec; }
TimeSpec extract_atime(StatT const& st) { return st.st_atimespec; }
#else
TimeSpec extract_mtime(StatT const& st) { return st.st_mtim; }
TimeSpec extract_atime(StatT const& st) { return st.st_atim; }
#endif

// allow the utimes implementation to compile even it we're not going
// to use it.

bool posix_utimes(const path& p, std::array<TimeSpec, 2> const& TS,
                  error_code& ec) {
  using namespace chrono;
  auto Convert = [](long nsec) {
    using int_type = decltype(std::declval< ::timeval>().tv_usec);
    auto dur = duration_cast<microseconds>(nanoseconds(nsec)).count();
    return static_cast<int_type>(dur);
  };
  struct ::timeval ConvertedTS[2] = {{TS[0].tv_sec, Convert(TS[0].tv_nsec)},
                                     {TS[1].tv_sec, Convert(TS[1].tv_nsec)}};
  if (::utimes(p.c_str(), ConvertedTS) == -1) {
    ec = capture_errno();
    return true;
  }
  return false;
}

#if defined(_LIBCPP_USE_UTIMENSAT)
bool posix_utimensat(const path& p, std::array<TimeSpec, 2> const& TS,
                     error_code& ec) {
  if (::utimensat(AT_FDCWD, p.c_str(), TS.data(), 0) == -1) {
    ec = capture_errno();
    return true;
  }
  return false;
}
#endif

bool set_file_times(const path& p, std::array<TimeSpec, 2> const& TS,
                    error_code& ec) {
#if !defined(_LIBCPP_USE_UTIMENSAT)
  return posix_utimes(p, TS, ec);
#else
  return posix_utimensat(p, TS, ec);
#endif
}

} // namespace
} // end namespace detail

_LIBCPP_END_NAMESPACE_FILESYSTEM

/* -- END filesystem/filesystem_common.h -- */

/* -- BEGIN filesystem/directory_iterator.cpp -- */

#if defined(_LIBCPP_WIN32API)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dirent.h>
#endif
#include <errno.h>

/* #include "filesystem_common.h" */

_LIBCPP_BEGIN_NAMESPACE_FILESYSTEM

namespace detail {
namespace {

#if !defined(_LIBCPP_WIN32API)
template <class DirEntT, class = decltype(DirEntT::d_type)>
static file_type get_file_type(DirEntT* ent, int) {
  switch (ent->d_type) {
  case DT_BLK:
    return file_type::block;
  case DT_CHR:
    return file_type::character;
  case DT_DIR:
    return file_type::directory;
  case DT_FIFO:
    return file_type::fifo;
  case DT_LNK:
    return file_type::symlink;
  case DT_REG:
    return file_type::regular;
  case DT_SOCK:
    return file_type::socket;
  // Unlike in lstat, hitting "unknown" here simply means that the underlying
  // filesystem doesn't support d_type. Report is as 'none' so we correctly
  // set the cache to empty.
  case DT_UNKNOWN:
    break;
  }
  return file_type::none;
}

template <class DirEntT>
static file_type get_file_type(DirEntT* ent, long) {
  return file_type::none;
}

static pair<string_view, file_type> posix_readdir(DIR* dir_stream,
                                                  error_code& ec) {
  struct dirent* dir_entry_ptr = nullptr;
  errno = 0; // zero errno in order to detect errors
  ec.clear();
  if ((dir_entry_ptr = ::readdir(dir_stream)) == nullptr) {
    if (errno)
      ec = capture_errno();
    return {};
  } else {
    return {dir_entry_ptr->d_name, get_file_type(dir_entry_ptr, 0)};
  }
}
#else

static file_type get_file_type(const WIN32_FIND_DATA& data) {
  //auto attrs = data.dwFileAttributes;
  // FIXME(EricWF)
  return file_type::unknown;
}
static uintmax_t get_file_size(const WIN32_FIND_DATA& data) {
  return (data.nFileSizeHight * (MAXDWORD + 1)) + data.nFileSizeLow;
}
static file_time_type get_write_time(const WIN32_FIND_DATA& data) {
  ULARGE_INTEGER tmp;
  FILETIME& time = data.ftLastWriteTime;
  tmp.u.LowPart = time.dwLowDateTime;
  tmp.u.HighPart = time.dwHighDateTime;
  return file_time_type(file_time_type::duration(time.QuadPart));
}

#endif

} // namespace
} // namespace detail

using detail::ErrorHandler;

#if defined(_LIBCPP_WIN32API)
class __dir_stream {
public:
  __dir_stream() = delete;
  __dir_stream& operator=(const __dir_stream&) = delete;

  __dir_stream(__dir_stream&& __ds) noexcept : __stream_(__ds.__stream_),
                                               __root_(move(__ds.__root_)),
                                               __entry_(move(__ds.__entry_)) {
    __ds.__stream_ = INVALID_HANDLE_VALUE;
  }

  __dir_stream(const path& root, directory_options opts, error_code& ec)
      : __stream_(INVALID_HANDLE_VALUE), __root_(root) {
    __stream_ = ::FindFirstFileEx(root.c_str(), &__data_);
    if (__stream_ == INVALID_HANDLE_VALUE) {
      ec = error_code(::GetLastError(), generic_category());
      const bool ignore_permission_denied =
          bool(opts & directory_options::skip_permission_denied);
      if (ignore_permission_denied && ec.value() == ERROR_ACCESS_DENIED)
        ec.clear();
      return;
    }
  }

  ~__dir_stream() noexcept {
    if (__stream_ == INVALID_HANDLE_VALUE)
      return;
    close();
  }

  bool good() const noexcept { return __stream_ != INVALID_HANDLE_VALUE; }

  bool advance(error_code& ec) {
    while (::FindNextFile(__stream_, &__data_)) {
      if (!strcmp(__data_.cFileName, ".") || strcmp(__data_.cFileName, ".."))
        continue;
      // FIXME: Cache more of this
      //directory_entry::__cached_data cdata;
      //cdata.__type_ = get_file_type(__data_);
      //cdata.__size_ = get_file_size(__data_);
      //cdata.__write_time_ = get_write_time(__data_);
      __entry_.__assign_iter_entry(
          __root_ / __data_.cFileName,
          directory_entry::__create_iter_result(get_file_type(__data)));
      return true;
    }
    ec = error_code(::GetLastError(), generic_category());
    close();
    return false;
  }

private:
  error_code close() noexcept {
    error_code ec;
    if (!::FindClose(__stream_))
      ec = error_code(::GetLastError(), generic_category());
    __stream_ = INVALID_HANDLE_VALUE;
    return ec;
  }

  HANDLE __stream_{INVALID_HANDLE_VALUE};
  WIN32_FIND_DATA __data_;

public:
  path __root_;
  directory_entry __entry_;
};
#else
class __dir_stream {
public:
  __dir_stream() = delete;
  __dir_stream& operator=(const __dir_stream&) = delete;

  __dir_stream(__dir_stream&& other) noexcept : __stream_(other.__stream_),
                                                __root_(move(other.__root_)),
                                                __entry_(move(other.__entry_)) {
    other.__stream_ = nullptr;
  }

  __dir_stream(const path& root, directory_options opts, error_code& ec)
      : __stream_(nullptr), __root_(root) {
    if ((__stream_ = ::opendir(root.c_str())) == nullptr) {
      ec = detail::capture_errno();
      const bool allow_eacess =
          bool(opts & directory_options::skip_permission_denied);
      if (allow_eacess && ec.value() == EACCES)
        ec.clear();
      return;
    }
    advance(ec);
  }

  ~__dir_stream() noexcept {
    if (__stream_)
      close();
  }

  bool good() const noexcept { return __stream_ != nullptr; }

  bool advance(error_code& ec) {
    while (true) {
      auto str_type_pair = detail::posix_readdir(__stream_, ec);
      auto& str = str_type_pair.first;
      if (str == "." || str == "..") {
        continue;
      } else if (ec || str.empty()) {
        close();
        return false;
      } else {
        __entry_.__assign_iter_entry(
            __root_ / str,
            directory_entry::__create_iter_result(str_type_pair.second));
        return true;
      }
    }
  }

private:
  error_code close() noexcept {
    error_code m_ec;
    if (::closedir(__stream_) == -1)
      m_ec = detail::capture_errno();
    __stream_ = nullptr;
    return m_ec;
  }

  DIR* __stream_{nullptr};

public:
  path __root_;
  directory_entry __entry_;
};
#endif

// directory_iterator

directory_iterator::directory_iterator(const path& p, error_code* ec,
                                       directory_options opts) {
  ErrorHandler<void> err("directory_iterator::directory_iterator(...)", ec, &p);

  error_code m_ec;
  __imp_ = make_shared<__dir_stream>(p, opts, m_ec);
  if (ec)
    *ec = m_ec;
  if (!__imp_->good()) {
    __imp_.reset();
    if (m_ec)
      err.report(m_ec);
  }
}

directory_iterator& directory_iterator::__increment(error_code* ec) {
  _LIBCPP_ASSERT(__imp_, "Attempting to increment an invalid iterator");
  ErrorHandler<void> err("directory_iterator::operator++()", ec);

  error_code m_ec;
  if (!__imp_->advance(m_ec)) {
    path root = move(__imp_->__root_);
    __imp_.reset();
    if (m_ec)
      err.report(m_ec, "at root \"%s\"", root);
  }
  return *this;
}

directory_entry const& directory_iterator::__dereference() const {
  _LIBCPP_ASSERT(__imp_, "Attempting to dereference an invalid iterator");
  return __imp_->__entry_;
}

// recursive_directory_iterator

struct recursive_directory_iterator::__shared_imp {
  stack<__dir_stream> __stack_;
  directory_options __options_;
};

recursive_directory_iterator::recursive_directory_iterator(
    const path& p, directory_options opt, error_code* ec)
    : __imp_(nullptr), __rec_(true) {
  ErrorHandler<void> err("recursive_directory_iterator", ec, &p);

  error_code m_ec;
  __dir_stream new_s(p, opt, m_ec);
  if (m_ec)
    err.report(m_ec);
  if (m_ec || !new_s.good())
    return;

  __imp_ = make_shared<__shared_imp>();
  __imp_->__options_ = opt;
  __imp_->__stack_.push(move(new_s));
}

void recursive_directory_iterator::__pop(error_code* ec) {
  _LIBCPP_ASSERT(__imp_, "Popping the end iterator");
  if (ec)
    ec->clear();
  __imp_->__stack_.pop();
  if (__imp_->__stack_.size() == 0)
    __imp_.reset();
  else
    __advance(ec);
}

directory_options recursive_directory_iterator::options() const {
  return __imp_->__options_;
}

int recursive_directory_iterator::depth() const {
  return __imp_->__stack_.size() - 1;
}

const directory_entry& recursive_directory_iterator::__dereference() const {
  return __imp_->__stack_.top().__entry_;
}

recursive_directory_iterator&
recursive_directory_iterator::__increment(error_code* ec) {
  if (ec)
    ec->clear();
  if (recursion_pending()) {
    if (__try_recursion(ec) || (ec && *ec))
      return *this;
  }
  __rec_ = true;
  __advance(ec);
  return *this;
}

void recursive_directory_iterator::__advance(error_code* ec) {
  ErrorHandler<void> err("recursive_directory_iterator::operator++()", ec);

  const directory_iterator end_it;
  auto& stack = __imp_->__stack_;
  error_code m_ec;
  while (stack.size() > 0) {
    if (stack.top().advance(m_ec))
      return;
    if (m_ec)
      break;
    stack.pop();
  }

  if (m_ec) {
    path root = move(stack.top().__root_);
    __imp_.reset();
    err.report(m_ec, "at root \"%s\"", root);
  } else {
    __imp_.reset();
  }
}

bool recursive_directory_iterator::__try_recursion(error_code* ec) {
  ErrorHandler<void> err("recursive_directory_iterator::operator++()", ec);

  bool rec_sym = bool(options() & directory_options::follow_directory_symlink);

  auto& curr_it = __imp_->__stack_.top();

  bool skip_rec = false;
  error_code m_ec;
  if (!rec_sym) {
    file_status st(curr_it.__entry_.__get_sym_ft(&m_ec));
    if (m_ec && status_known(st))
      m_ec.clear();
    if (m_ec || is_symlink(st) || !is_directory(st))
      skip_rec = true;
  } else {
    file_status st(curr_it.__entry_.__get_ft(&m_ec));
    if (m_ec && status_known(st))
      m_ec.clear();
    if (m_ec || !is_directory(st))
      skip_rec = true;
  }

  if (!skip_rec) {
    __dir_stream new_it(curr_it.__entry_.path(), __imp_->__options_, m_ec);
    if (new_it.good()) {
      __imp_->__stack_.push(move(new_it));
      return true;
    }
  }
  if (m_ec) {
    const bool allow_eacess =
        bool(__imp_->__options_ & directory_options::skip_permission_denied);
    if (m_ec.value() == EACCES && allow_eacess) {
      if (ec)
        ec->clear();
    } else {
      path at_ent = move(curr_it.__entry_.__p_);
      __imp_.reset();
      err.report(m_ec, "attempting recursion into \"%s\"", at_ent);
    }
  }
  return false;
}

_LIBCPP_END_NAMESPACE_FILESYSTEM

/* -- END filesystem/directory_iterator.cpp -- */

/* -- BEGIN filesystem/int128_builtins.cpp -- */

/* ===----------------------------------------------------------------------===
 *
 * This file implements __muloti4, and is stolen from the compiler_rt library.
 *
 * FIXME: we steal and re-compile it into filesystem, which uses __int128_t,
 * and requires this builtin when sanitized. See llvm.org/PR30643
 *
 * ===----------------------------------------------------------------------===
 */

/* #include "__config" */
#include <climits>

#ifndef __SIZEOF_INT128__
#define _LIBCPP_HAS_NO_INT128
#endif

#if !defined(_LIBCPP_HAS_NO_INT128)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif
extern "C" __attribute__((no_sanitize("undefined")))
__int128_t __muloti4(__int128_t a, __int128_t b, int* overflow) {
  const int N = (int)(sizeof(__int128_t) * CHAR_BIT);
  const __int128_t MIN = (__int128_t)1 << (N - 1);
  const __int128_t MAX = ~MIN;
  *overflow = 0;
  __int128_t result = a * b;
  if (a == MIN) {
    if (b != 0 && b != 1)
      *overflow = 1;
    return result;
  }
  if (b == MIN) {
    if (a != 0 && a != 1)
      *overflow = 1;
    return result;
  }
  __int128_t sa = a >> (N - 1);
  __int128_t abs_a = (a ^ sa) - sa;
  __int128_t sb = b >> (N - 1);
  __int128_t abs_b = (b ^ sb) - sb;
  if (abs_a < 2 || abs_b < 2)
    return result;
  if (sa == sb) {
    if (abs_a > MAX / abs_b)
      *overflow = 1;
  } else {
    if (abs_a > MIN / -abs_b)
      *overflow = 1;
  }
  return result;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif

/* -- END filesystem/int128_builtins.cpp -- */

/* -- BEGIN filesystem/operations.cpp -- */

/* #include "filesystem" */
#include <array>
#include <iterator>
#include <fstream>
#include <random> /* for unique_path */
#include <string_view>
#include <type_traits>
#include <vector>
#include <cstdlib>
#include <climits>

/* #include "filesystem_common.h" */

#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <fcntl.h> /* values for fchmodat */

#if defined(__linux__)
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
#include <sys/sendfile.h>
#define _LIBCPP_USE_SENDFILE
#endif
#elif defined(__APPLE__) || __has_include(<copyfile.h>)
#include <copyfile.h>
#define _LIBCPP_USE_COPYFILE
#endif

#if !defined(__APPLE__)
#define _LIBCPP_USE_CLOCK_GETTIME
#endif

#if !defined(CLOCK_REALTIME) || !defined(_LIBCPP_USE_CLOCK_GETTIME)
#include <sys/time.h> // for gettimeofday and timeval
#endif                // !defined(CLOCK_REALTIME)

#if defined(_LIBCPP_COMPILER_GCC)
#if _GNUC_VER < 500
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#endif

_LIBCPP_BEGIN_NAMESPACE_FILESYSTEM

namespace {
namespace parser {

using string_view_t = path::__string_view;
using string_view_pair = pair<string_view_t, string_view_t>;
using PosPtr = path::value_type const*;

struct PathParser {
  enum ParserState : unsigned char {
    // Zero is a special sentinel value used by default constructed iterators.
    PS_BeforeBegin = path::iterator::_BeforeBegin,
    PS_InRootName = path::iterator::_InRootName,
    PS_InRootDir = path::iterator::_InRootDir,
    PS_InFilenames = path::iterator::_InFilenames,
    PS_InTrailingSep = path::iterator::_InTrailingSep,
    PS_AtEnd = path::iterator::_AtEnd
  };

  const string_view_t Path;
  string_view_t RawEntry;
  ParserState State;

private:
  PathParser(string_view_t P, ParserState State) noexcept : Path(P),
                                                            State(State) {}

public:
  PathParser(string_view_t P, string_view_t E, unsigned char S)
      : Path(P), RawEntry(E), State(static_cast<ParserState>(S)) {
    // S cannot be '0' or PS_BeforeBegin.
  }

  static PathParser CreateBegin(string_view_t P) noexcept {
    PathParser PP(P, PS_BeforeBegin);
    PP.increment();
    return PP;
  }

  static PathParser CreateEnd(string_view_t P) noexcept {
    PathParser PP(P, PS_AtEnd);
    return PP;
  }

  PosPtr peek() const noexcept {
    auto TkEnd = getNextTokenStartPos();
    auto End = getAfterBack();
    return TkEnd == End ? nullptr : TkEnd;
  }

  void increment() noexcept {
    const PosPtr End = getAfterBack();
    const PosPtr Start = getNextTokenStartPos();
    if (Start == End)
      return makeState(PS_AtEnd);

    switch (State) {
    case PS_BeforeBegin: {
      PosPtr TkEnd = consumeSeparator(Start, End);
      if (TkEnd)
        return makeState(PS_InRootDir, Start, TkEnd);
      else
        return makeState(PS_InFilenames, Start, consumeName(Start, End));
    }
    case PS_InRootDir:
      return makeState(PS_InFilenames, Start, consumeName(Start, End));

    case PS_InFilenames: {
      PosPtr SepEnd = consumeSeparator(Start, End);
      if (SepEnd != End) {
        PosPtr TkEnd = consumeName(SepEnd, End);
        if (TkEnd)
          return makeState(PS_InFilenames, SepEnd, TkEnd);
      }
      return makeState(PS_InTrailingSep, Start, SepEnd);
    }

    case PS_InTrailingSep:
      return makeState(PS_AtEnd);

    case PS_InRootName:
    case PS_AtEnd:
      _LIBCPP_UNREACHABLE();
    }
  }

  void decrement() noexcept {
    const PosPtr REnd = getBeforeFront();
    const PosPtr RStart = getCurrentTokenStartPos() - 1;
    if (RStart == REnd) // we're decrementing the begin
      return makeState(PS_BeforeBegin);

    switch (State) {
    case PS_AtEnd: {
      // Try to consume a trailing separator or root directory first.
      if (PosPtr SepEnd = consumeSeparator(RStart, REnd)) {
        if (SepEnd == REnd)
          return makeState(PS_InRootDir, Path.data(), RStart + 1);
        return makeState(PS_InTrailingSep, SepEnd + 1, RStart + 1);
      } else {
        PosPtr TkStart = consumeName(RStart, REnd);
        return makeState(PS_InFilenames, TkStart + 1, RStart + 1);
      }
    }
    case PS_InTrailingSep:
      return makeState(PS_InFilenames, consumeName(RStart, REnd) + 1,
                       RStart + 1);
    case PS_InFilenames: {
      PosPtr SepEnd = consumeSeparator(RStart, REnd);
      if (SepEnd == REnd)
        return makeState(PS_InRootDir, Path.data(), RStart + 1);
      PosPtr TkEnd = consumeName(SepEnd, REnd);
      return makeState(PS_InFilenames, TkEnd + 1, SepEnd + 1);
    }
    case PS_InRootDir:
      // return makeState(PS_InRootName, Path.data(), RStart + 1);
    case PS_InRootName:
    case PS_BeforeBegin:
      _LIBCPP_UNREACHABLE();
    }
  }

  /// \brief Return a view with the "preferred representation" of the current
  ///   element. For example trailing separators are represented as a '.'
  string_view_t operator*() const noexcept {
    switch (State) {
    case PS_BeforeBegin:
    case PS_AtEnd:
      return "";
    case PS_InRootDir:
      return "/";
    case PS_InTrailingSep:
      return "";
    case PS_InRootName:
    case PS_InFilenames:
      return RawEntry;
    }
    _LIBCPP_UNREACHABLE();
  }

  explicit operator bool() const noexcept {
    return State != PS_BeforeBegin && State != PS_AtEnd;
  }

  PathParser& operator++() noexcept {
    increment();
    return *this;
  }

  PathParser& operator--() noexcept {
    decrement();
    return *this;
  }

  bool inRootPath() const noexcept {
    return State == PS_InRootDir || State == PS_InRootName;
  }

private:
  void makeState(ParserState NewState, PosPtr Start, PosPtr End) noexcept {
    State = NewState;
    RawEntry = string_view_t(Start, End - Start);
  }
  void makeState(ParserState NewState) noexcept {
    State = NewState;
    RawEntry = {};
  }

  PosPtr getAfterBack() const noexcept { return Path.data() + Path.size(); }

  PosPtr getBeforeFront() const noexcept { return Path.data() - 1; }

  /// \brief Return a pointer to the first character after the currently
  ///   lexed element.
  PosPtr getNextTokenStartPos() const noexcept {
    switch (State) {
    case PS_BeforeBegin:
      return Path.data();
    case PS_InRootName:
    case PS_InRootDir:
    case PS_InFilenames:
      return &RawEntry.back() + 1;
    case PS_InTrailingSep:
    case PS_AtEnd:
      return getAfterBack();
    }
    _LIBCPP_UNREACHABLE();
  }

  /// \brief Return a pointer to the first character in the currently lexed
  ///   element.
  PosPtr getCurrentTokenStartPos() const noexcept {
    switch (State) {
    case PS_BeforeBegin:
    case PS_InRootName:
      return &Path.front();
    case PS_InRootDir:
    case PS_InFilenames:
    case PS_InTrailingSep:
      return &RawEntry.front();
    case PS_AtEnd:
      return &Path.back() + 1;
    }
    _LIBCPP_UNREACHABLE();
  }

  PosPtr consumeSeparator(PosPtr P, PosPtr End) const noexcept {
    if (P == End || *P != '/')
      return nullptr;
    const int Inc = P < End ? 1 : -1;
    P += Inc;
    while (P != End && *P == '/')
      P += Inc;
    return P;
  }

  PosPtr consumeName(PosPtr P, PosPtr End) const noexcept {
    if (P == End || *P == '/')
      return nullptr;
    const int Inc = P < End ? 1 : -1;
    P += Inc;
    while (P != End && *P != '/')
      P += Inc;
    return P;
  }
};

static string_view_pair separate_filename(string_view_t const& s) {
  if (s == "." || s == ".." || s.empty())
    return string_view_pair{s, ""};
  auto pos = s.find_last_of('.');
  if (pos == string_view_t::npos || pos == 0)
    return string_view_pair{s, string_view_t{}};
  return string_view_pair{s.substr(0, pos), s.substr(pos)};
}

static string_view_t createView(PosPtr S, PosPtr E) noexcept {
  return {S, static_cast<size_t>(E - S) + 1};
}

} // namespace parser
} // namespace

//                       POSIX HELPERS

namespace detail {
namespace {

using value_type = path::value_type;
using string_type = path::string_type;

struct FileDescriptor {
  const path& name;
  int fd = -1;
  StatT m_stat;
  file_status m_status;

  template <class... Args>
  static FileDescriptor create(const path* p, error_code& ec, Args... args) {
    ec.clear();
    int fd;
    if ((fd = ::open(p->c_str(), args...)) == -1) {
      ec = capture_errno();
      return FileDescriptor{p};
    }
    return FileDescriptor(p, fd);
  }

  template <class... Args>
  static FileDescriptor create_with_status(const path* p, error_code& ec,
                                           Args... args) {
    FileDescriptor fd = create(p, ec, args...);
    if (!ec)
      fd.refresh_status(ec);

    return fd;
  }

  file_status get_status() const { return m_status; }
  StatT const& get_stat() const { return m_stat; }

  bool status_known() const { return _VSTD_FS::status_known(m_status); }

  file_status refresh_status(error_code& ec);

  void close() noexcept {
    if (fd != -1)
      ::close(fd);
    fd = -1;
  }

  FileDescriptor(FileDescriptor&& other)
      : name(other.name), fd(other.fd), m_stat(other.m_stat),
        m_status(other.m_status) {
    other.fd = -1;
    other.m_status = file_status{};
  }

  ~FileDescriptor() { close(); }

  FileDescriptor(FileDescriptor const&) = delete;
  FileDescriptor& operator=(FileDescriptor const&) = delete;

private:
  explicit FileDescriptor(const path* p, int fd = -1) : name(*p), fd(fd) {}
};

perms posix_get_perms(const StatT& st) noexcept {
  return static_cast<perms>(st.st_mode) & perms::mask;
}

::mode_t posix_convert_perms(perms prms) {
  return static_cast< ::mode_t>(prms & perms::mask);
}

file_status create_file_status(error_code& m_ec, path const& p,
                               const StatT& path_stat, error_code* ec) {
  if (ec)
    *ec = m_ec;
  if (m_ec && (m_ec.value() == ENOENT || m_ec.value() == ENOTDIR)) {
    return file_status(file_type::not_found);
  } else if (m_ec) {
    ErrorHandler<void> err("posix_stat", ec, &p);
    err.report(m_ec, "failed to determine attributes for the specified path");
    return file_status(file_type::none);
  }
  // else

  file_status fs_tmp;
  auto const mode = path_stat.st_mode;
  if (S_ISLNK(mode))
    fs_tmp.type(file_type::symlink);
  else if (S_ISREG(mode))
    fs_tmp.type(file_type::regular);
  else if (S_ISDIR(mode))
    fs_tmp.type(file_type::directory);
  else if (S_ISBLK(mode))
    fs_tmp.type(file_type::block);
  else if (S_ISCHR(mode))
    fs_tmp.type(file_type::character);
  else if (S_ISFIFO(mode))
    fs_tmp.type(file_type::fifo);
  else if (S_ISSOCK(mode))
    fs_tmp.type(file_type::socket);
  else
    fs_tmp.type(file_type::unknown);

  fs_tmp.permissions(detail::posix_get_perms(path_stat));
  return fs_tmp;
}

file_status posix_stat(path const& p, StatT& path_stat, error_code* ec) {
  error_code m_ec;
  if (::stat(p.c_str(), &path_stat) == -1)
    m_ec = detail::capture_errno();
  return create_file_status(m_ec, p, path_stat, ec);
}

file_status posix_stat(path const& p, error_code* ec) {
  StatT path_stat;
  return posix_stat(p, path_stat, ec);
}

file_status posix_lstat(path const& p, StatT& path_stat, error_code* ec) {
  error_code m_ec;
  if (::lstat(p.c_str(), &path_stat) == -1)
    m_ec = detail::capture_errno();
  return create_file_status(m_ec, p, path_stat, ec);
}

file_status posix_lstat(path const& p, error_code* ec) {
  StatT path_stat;
  return posix_lstat(p, path_stat, ec);
}

bool posix_ftruncate(const FileDescriptor& fd, size_t to_size, error_code& ec) {
  if (::ftruncate(fd.fd, to_size) == -1) {
    ec = capture_errno();
    return true;
  }
  ec.clear();
  return false;
}

bool posix_fchmod(const FileDescriptor& fd, const StatT& st, error_code& ec) {
  if (::fchmod(fd.fd, st.st_mode) == -1) {
    ec = capture_errno();
    return true;
  }
  ec.clear();
  return false;
}

bool stat_equivalent(const StatT& st1, const StatT& st2) {
  return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}

file_status FileDescriptor::refresh_status(error_code& ec) {
  // FD must be open and good.
  m_status = file_status{};
  m_stat = {};
  error_code m_ec;
  if (::fstat(fd, &m_stat) == -1)
    m_ec = capture_errno();
  m_status = create_file_status(m_ec, name, m_stat, &ec);
  return m_status;
}
} // namespace
} // end namespace detail

using detail::capture_errno;
using detail::ErrorHandler;
using detail::StatT;
using detail::TimeSpec;
using parser::createView;
using parser::PathParser;
using parser::string_view_t;

const bool _FilesystemClock::is_steady;

_FilesystemClock::time_point _FilesystemClock::now() noexcept {
  typedef chrono::duration<rep> __secs;
#if defined(_LIBCPP_USE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
  typedef chrono::duration<rep, nano> __nsecs;
  struct timespec tp;
  if (0 != clock_gettime(CLOCK_REALTIME, &tp))
    __throw_system_error(errno, "clock_gettime(CLOCK_REALTIME) failed");
  return time_point(__secs(tp.tv_sec) +
                    chrono::duration_cast<duration>(__nsecs(tp.tv_nsec)));
#else
  typedef chrono::duration<rep, micro> __microsecs;
  timeval tv;
  gettimeofday(&tv, 0);
  return time_point(__secs(tv.tv_sec) + __microsecs(tv.tv_usec));
#endif // _LIBCPP_USE_CLOCK_GETTIME && CLOCK_REALTIME
}

filesystem_error::~filesystem_error() {}

void filesystem_error::__create_what(int __num_paths) {
  const char* derived_what = system_error::what();
  __storage_->__what_ = [&]() -> string {
    const char* p1 = path1().native().empty() ? "\"\"" : path1().c_str();
    const char* p2 = path2().native().empty() ? "\"\"" : path2().c_str();
    switch (__num_paths) {
    default:
      return detail::format_string("filesystem error: %s", derived_what);
    case 1:
      return detail::format_string("filesystem error: %s [%s]", derived_what,
                                   p1);
    case 2:
      return detail::format_string("filesystem error: %s [%s] [%s]",
                                   derived_what, p1, p2);
    }
  }();
}

static path __do_absolute(const path& p, path* cwd, error_code* ec) {
  if (ec)
    ec->clear();
  if (p.is_absolute())
    return p;
  *cwd = __current_path(ec);
  if (ec && *ec)
    return {};
  return (*cwd) / p;
}

path __absolute(const path& p, error_code* ec) {
  path cwd;
  return __do_absolute(p, &cwd, ec);
}

path __canonical(path const& orig_p, error_code* ec) {
  path cwd;
  ErrorHandler<path> err("canonical", ec, &orig_p, &cwd);

  path p = __do_absolute(orig_p, &cwd, ec);
  char buff[PATH_MAX + 1];
  char* ret;
  if ((ret = ::realpath(p.c_str(), buff)) == nullptr)
    return err.report(capture_errno());
  return {ret};
}

void __copy(const path& from, const path& to, copy_options options,
            error_code* ec) {
  ErrorHandler<void> err("copy", ec, &from, &to);

  const bool sym_status = bool(
      options & (copy_options::create_symlinks | copy_options::skip_symlinks));

  const bool sym_status2 = bool(options & copy_options::copy_symlinks);

  error_code m_ec1;
  StatT f_st = {};
  const file_status f = sym_status || sym_status2
                            ? detail::posix_lstat(from, f_st, &m_ec1)
                            : detail::posix_stat(from, f_st, &m_ec1);
  if (m_ec1)
    return err.report(m_ec1);

  StatT t_st = {};
  const file_status t = sym_status ? detail::posix_lstat(to, t_st, &m_ec1)
                                   : detail::posix_stat(to, t_st, &m_ec1);

  if (not status_known(t))
    return err.report(m_ec1);

  if (!exists(f) || is_other(f) || is_other(t) ||
      (is_directory(f) && is_regular_file(t)) ||
      detail::stat_equivalent(f_st, t_st)) {
    return err.report(errc::function_not_supported);
  }

  if (ec)
    ec->clear();

  if (is_symlink(f)) {
    if (bool(copy_options::skip_symlinks & options)) {
      // do nothing
    } else if (not exists(t)) {
      __copy_symlink(from, to, ec);
    } else {
      return err.report(errc::file_exists);
    }
    return;
  } else if (is_regular_file(f)) {
    if (bool(copy_options::directories_only & options)) {
      // do nothing
    } else if (bool(copy_options::create_symlinks & options)) {
      __create_symlink(from, to, ec);
    } else if (bool(copy_options::create_hard_links & options)) {
      __create_hard_link(from, to, ec);
    } else if (is_directory(t)) {
      __copy_file(from, to / from.filename(), options, ec);
    } else {
      __copy_file(from, to, options, ec);
    }
    return;
  } else if (is_directory(f) && bool(copy_options::create_symlinks & options)) {
    return err.report(errc::is_a_directory);
  } else if (is_directory(f) && (bool(copy_options::recursive & options) ||
                                 copy_options::none == options)) {

    if (!exists(t)) {
      // create directory to with attributes from 'from'.
      __create_directory(to, from, ec);
      if (ec && *ec) {
        return;
      }
    }
    directory_iterator it =
        ec ? directory_iterator(from, *ec) : directory_iterator(from);
    if (ec && *ec) {
      return;
    }
    error_code m_ec2;
    for (; it != directory_iterator(); it.increment(m_ec2)) {
      if (m_ec2) {
        return err.report(m_ec2);
      }
      __copy(it->path(), to / it->path().filename(),
             options | copy_options::__in_recursive_copy, ec);
      if (ec && *ec) {
        return;
      }
    }
  }
}

namespace detail {
namespace {

#ifdef _LIBCPP_USE_SENDFILE
bool copy_file_impl_sendfile(FileDescriptor& read_fd, FileDescriptor& write_fd,
                             error_code& ec) {

  size_t count = read_fd.get_stat().st_size;
  do {
    ssize_t res;
    if ((res = ::sendfile(write_fd.fd, read_fd.fd, nullptr, count)) == -1) {
      ec = capture_errno();
      return false;
    }
    count -= res;
  } while (count > 0);

  ec.clear();

  return true;
}
#elif defined(_LIBCPP_USE_COPYFILE)
bool copy_file_impl_copyfile(FileDescriptor& read_fd, FileDescriptor& write_fd,
                             error_code& ec) {
  struct CopyFileState {
    copyfile_state_t state;
    CopyFileState() { state = copyfile_state_alloc(); }
    ~CopyFileState() { copyfile_state_free(state); }

  private:
    CopyFileState(CopyFileState const&) = delete;
    CopyFileState& operator=(CopyFileState const&) = delete;
  };

  CopyFileState cfs;
  if (fcopyfile(read_fd.fd, write_fd.fd, cfs.state, COPYFILE_DATA) < 0) {
    ec = capture_errno();
    return false;
  }

  ec.clear();
  return true;
}
#endif

// This function is guarded by ifdef's in Juniper due to the current
// clang not supporting an __open function on ifstream and ofstream.
#if !defined(_LIBCPP_USE_SENDFILE) && !defined(_LIBCPP_USE_COPYFILE)
// Note: This function isn't guarded by ifdef's even though it may be unused
// in order to assure it still compiles.
__attribute__((unused)) bool copy_file_impl_default(FileDescriptor& read_fd,
                                                    FileDescriptor& write_fd,
                                                    error_code& ec) {
  ifstream in;
  in.__open(read_fd.fd, ios::binary);
  if (!in.is_open()) {
    // This assumes that __open didn't reset the error code.
    ec = capture_errno();
    return false;
  }
  ofstream out;
  out.__open(write_fd.fd, ios::binary);
  if (!out.is_open()) {
    ec = capture_errno();
    return false;
  }

  if (in.good() && out.good()) {
    using InIt = istreambuf_iterator<char>;
    using OutIt = ostreambuf_iterator<char>;
    InIt bin(in);
    InIt ein;
    OutIt bout(out);
    copy(bin, ein, bout);
  }
  if (out.fail() || in.fail()) {
    ec = make_error_code(errc::io_error);
    return false;
  }

  ec.clear();
  return true;
}
#endif

bool copy_file_impl(FileDescriptor& from, FileDescriptor& to, error_code& ec) {
#if defined(_LIBCPP_USE_SENDFILE)
  return copy_file_impl_sendfile(from, to, ec);
#elif defined(_LIBCPP_USE_COPYFILE)
  return copy_file_impl_copyfile(from, to, ec);
#else
  return copy_file_impl_default(from, to, ec);
#endif
}

} // namespace
} // namespace detail

bool __copy_file(const path& from, const path& to, copy_options options,
                 error_code* ec) {
  using detail::FileDescriptor;
  ErrorHandler<bool> err("copy_file", ec, &to, &from);

  error_code m_ec;
  FileDescriptor from_fd =
      FileDescriptor::create_with_status(&from, m_ec, O_RDONLY | O_NONBLOCK);
  if (m_ec)
    return err.report(m_ec);

  auto from_st = from_fd.get_status();
  StatT const& from_stat = from_fd.get_stat();
  if (!is_regular_file(from_st)) {
    if (not m_ec)
      m_ec = make_error_code(errc::not_supported);
    return err.report(m_ec);
  }

  const bool skip_existing = bool(copy_options::skip_existing & options);
  const bool update_existing = bool(copy_options::update_existing & options);
  const bool overwrite_existing =
      bool(copy_options::overwrite_existing & options);

  StatT to_stat_path;
  file_status to_st = detail::posix_stat(to, to_stat_path, &m_ec);
  if (!status_known(to_st))
    return err.report(m_ec);

  const bool to_exists = exists(to_st);
  if (to_exists && !is_regular_file(to_st))
    return err.report(errc::not_supported);

  if (to_exists && detail::stat_equivalent(from_stat, to_stat_path))
    return err.report(errc::file_exists);

  if (to_exists && skip_existing)
    return false;

  bool ShouldCopy = [&]() {
    if (to_exists && update_existing) {
      auto from_time = detail::extract_mtime(from_stat);
      auto to_time = detail::extract_mtime(to_stat_path);
      if (from_time.tv_sec < to_time.tv_sec)
        return false;
      if (from_time.tv_sec == to_time.tv_sec &&
          from_time.tv_nsec <= to_time.tv_nsec)
        return false;
      return true;
    }
    if (!to_exists || overwrite_existing)
      return true;
    return err.report(errc::file_exists);
  }();
  if (!ShouldCopy)
    return false;

  // Don't truncate right away. We may not be opening the file we originally
  // looked at; we'll check this later.
  int to_open_flags = O_WRONLY;
  if (!to_exists)
    to_open_flags |= O_CREAT;
  FileDescriptor to_fd = FileDescriptor::create_with_status(
      &to, m_ec, to_open_flags, from_stat.st_mode);
  if (m_ec)
    return err.report(m_ec);

  if (to_exists) {
    // Check that the file we initially stat'ed is equivalent to the one
    // we opened.
    // FIXME: report this better.
    if (!detail::stat_equivalent(to_stat_path, to_fd.get_stat()))
      return err.report(errc::bad_file_descriptor);

    // Set the permissions and truncate the file we opened.
    if (detail::posix_fchmod(to_fd, from_stat, m_ec))
      return err.report(m_ec);
    if (detail::posix_ftruncate(to_fd, 0, m_ec))
      return err.report(m_ec);
  }

  if (!copy_file_impl(from_fd, to_fd, m_ec)) {
    // FIXME: Remove the dest file if we failed, and it didn't exist previously.
    return err.report(m_ec);
  }

  return true;
}

void __copy_symlink(const path& existing_symlink, const path& new_symlink,
                    error_code* ec) {
  const path real_path(__read_symlink(existing_symlink, ec));
  if (ec && *ec) {
    return;
  }
  // NOTE: proposal says you should detect if you should call
  // create_symlink or create_directory_symlink. I don't think this
  // is needed with POSIX
  __create_symlink(real_path, new_symlink, ec);
}

bool __create_directories(const path& p, error_code* ec) {
  ErrorHandler<bool> err("create_directories", ec, &p);

  error_code m_ec;
  auto const st = detail::posix_stat(p, &m_ec);
  if (!status_known(st))
    return err.report(m_ec);
  else if (is_directory(st))
    return false;
  else if (exists(st))
    return err.report(errc::file_exists);

  const path parent = p.parent_path();
  if (!parent.empty()) {
    const file_status parent_st = status(parent, m_ec);
    if (not status_known(parent_st))
      return err.report(m_ec);
    if (not exists(parent_st)) {
      __create_directories(parent, ec);
      if (ec && *ec) {
        return false;
      }
    }
  }
  return __create_directory(p, ec);
}

bool __create_directory(const path& p, error_code* ec) {
  ErrorHandler<bool> err("create_directory", ec, &p);

  if (::mkdir(p.c_str(), static_cast<int>(perms::all)) == 0)
    return true;
  if (errno != EEXIST)
    err.report(capture_errno());
  return false;
}

bool __create_directory(path const& p, path const& attributes, error_code* ec) {
  ErrorHandler<bool> err("create_directory", ec, &p, &attributes);

  StatT attr_stat;
  error_code mec;
  auto st = detail::posix_stat(attributes, attr_stat, &mec);
  if (!status_known(st))
    return err.report(mec);
  if (!is_directory(st))
    return err.report(errc::not_a_directory,
                      "the specified attribute path is invalid");

  if (::mkdir(p.c_str(), attr_stat.st_mode) == 0)
    return true;
  if (errno != EEXIST)
    err.report(capture_errno());
  return false;
}

void __create_directory_symlink(path const& from, path const& to,
                                error_code* ec) {
  ErrorHandler<void> err("create_directory_symlink", ec, &from, &to);
  if (::symlink(from.c_str(), to.c_str()) != 0)
    return err.report(capture_errno());
}

void __create_hard_link(const path& from, const path& to, error_code* ec) {
  ErrorHandler<void> err("create_hard_link", ec, &from, &to);
  if (::link(from.c_str(), to.c_str()) == -1)
    return err.report(capture_errno());
}

void __create_symlink(path const& from, path const& to, error_code* ec) {
  ErrorHandler<void> err("create_symlink", ec, &from, &to);
  if (::symlink(from.c_str(), to.c_str()) == -1)
    return err.report(capture_errno());
}

path __current_path(error_code* ec) {
  ErrorHandler<path> err("current_path", ec);

  auto size = ::pathconf(".", _PC_PATH_MAX);
  _LIBCPP_ASSERT(size >= 0, "pathconf returned a 0 as max size");

  auto buff = unique_ptr<char[]>(new char[size + 1]);
  char* ret;
  if ((ret = ::getcwd(buff.get(), static_cast<size_t>(size))) == nullptr)
    return err.report(capture_errno(), "call to getcwd failed");

  return {buff.get()};
}

void __current_path(const path& p, error_code* ec) {
  ErrorHandler<void> err("current_path", ec, &p);
  if (::chdir(p.c_str()) == -1)
    err.report(capture_errno());
}

bool __equivalent(const path& p1, const path& p2, error_code* ec) {
  ErrorHandler<bool> err("equivalent", ec, &p1, &p2);

  error_code ec1, ec2;
  StatT st1 = {}, st2 = {};
  auto s1 = detail::posix_stat(p1.native(), st1, &ec1);
  if (!exists(s1))
    return err.report(errc::not_supported);
  auto s2 = detail::posix_stat(p2.native(), st2, &ec2);
  if (!exists(s2))
    return err.report(errc::not_supported);

  return detail::stat_equivalent(st1, st2);
}

uintmax_t __file_size(const path& p, error_code* ec) {
  ErrorHandler<uintmax_t> err("file_size", ec, &p);

  error_code m_ec;
  StatT st;
  file_status fst = detail::posix_stat(p, st, &m_ec);
  if (!exists(fst) || !is_regular_file(fst)) {
    errc error_kind =
        is_directory(fst) ? errc::is_a_directory : errc::not_supported;
    if (!m_ec)
      m_ec = make_error_code(error_kind);
    return err.report(m_ec);
  }
  // is_regular_file(p) == true
  return static_cast<uintmax_t>(st.st_size);
}

uintmax_t __hard_link_count(const path& p, error_code* ec) {
  ErrorHandler<uintmax_t> err("hard_link_count", ec, &p);

  error_code m_ec;
  StatT st;
  detail::posix_stat(p, st, &m_ec);
  if (m_ec)
    return err.report(m_ec);
  return static_cast<uintmax_t>(st.st_nlink);
}

bool __fs_is_empty(const path& p, error_code* ec) {
  ErrorHandler<bool> err("is_empty", ec, &p);

  error_code m_ec;
  StatT pst;
  auto st = detail::posix_stat(p, pst, &m_ec);
  if (m_ec)
    return err.report(m_ec);
  else if (!is_directory(st) && !is_regular_file(st))
    return err.report(errc::not_supported);
  else if (is_directory(st)) {
    auto it = ec ? directory_iterator(p, *ec) : directory_iterator(p);
    if (ec && *ec)
      return false;
    return it == directory_iterator{};
  } else if (is_regular_file(st))
    return static_cast<uintmax_t>(pst.st_size) == 0;

  _LIBCPP_UNREACHABLE();
}

static file_time_type __extract_last_write_time(const path& p, const StatT& st,
                                                error_code* ec) {
  using detail::fs_time;
  ErrorHandler<file_time_type> err("last_write_time", ec, &p);

  auto ts = detail::extract_mtime(st);
  if (!fs_time::is_representable(ts))
    return err.report(errc::value_too_large);

  return fs_time::convert_from_timespec(ts);
}

file_time_type __last_write_time(const path& p, error_code* ec) {
  using namespace chrono;
  ErrorHandler<file_time_type> err("last_write_time", ec, &p);

  error_code m_ec;
  StatT st;
  detail::posix_stat(p, st, &m_ec);
  if (m_ec)
    return err.report(m_ec);
  return __extract_last_write_time(p, st, ec);
}

void __last_write_time(const path& p, file_time_type new_time, error_code* ec) {
  using detail::fs_time;
  ErrorHandler<void> err("last_write_time", ec, &p);

  error_code m_ec;
  array<TimeSpec, 2> tbuf;
#if !defined(_LIBCPP_USE_UTIMENSAT)
  // This implementation has a race condition between determining the
  // last access time and attempting to set it to the same value using
  // ::utimes
  StatT st;
  file_status fst = detail::posix_stat(p, st, &m_ec);
  if (m_ec)
    return err.report(m_ec);
  tbuf[0] = detail::extract_atime(st);
#else
  tbuf[0].tv_sec = 0;
  tbuf[0].tv_nsec = UTIME_OMIT;
#endif
  if (!fs_time::convert_to_timespec(tbuf[1], new_time))
    return err.report(errc::value_too_large);

  detail::set_file_times(p, tbuf, m_ec);
  if (m_ec)
    return err.report(m_ec);
}

void __permissions(const path& p, perms prms, perm_options opts,
                   error_code* ec) {
  ErrorHandler<void> err("permissions", ec, &p);

  auto has_opt = [&](perm_options o) { return bool(o & opts); };
  const bool resolve_symlinks = !has_opt(perm_options::nofollow);
  const bool add_perms = has_opt(perm_options::add);
  const bool remove_perms = has_opt(perm_options::remove);
  _LIBCPP_ASSERT(
      (add_perms + remove_perms + has_opt(perm_options::replace)) == 1,
      "One and only one of the perm_options constants replace, add, or remove "
      "is present in opts");

  bool set_sym_perms = false;
  prms &= perms::mask;
  if (!resolve_symlinks || (add_perms || remove_perms)) {
    error_code m_ec;
    file_status st = resolve_symlinks ? detail::posix_stat(p, &m_ec)
                                      : detail::posix_lstat(p, &m_ec);
    set_sym_perms = is_symlink(st);
    if (m_ec)
      return err.report(m_ec);
    _LIBCPP_ASSERT(st.permissions() != perms::unknown,
                   "Permissions unexpectedly unknown");
    if (add_perms)
      prms |= st.permissions();
    else if (remove_perms)
      prms = st.permissions() & ~prms;
  }
  const auto real_perms = detail::posix_convert_perms(prms);

#if defined(AT_SYMLINK_NOFOLLOW) && defined(AT_FDCWD)
  const int flags = set_sym_perms ? AT_SYMLINK_NOFOLLOW : 0;
  if (::fchmodat(AT_FDCWD, p.c_str(), real_perms, flags) == -1) {
    return err.report(capture_errno());
  }
#else
  if (set_sym_perms)
    return err.report(errc::operation_not_supported);
  if (::chmod(p.c_str(), real_perms) == -1) {
    return err.report(capture_errno());
  }
#endif
}

path __read_symlink(const path& p, error_code* ec) {
  ErrorHandler<path> err("read_symlink", ec, &p);

  char buff[PATH_MAX + 1];
  error_code m_ec;
  ::ssize_t ret;
  if ((ret = ::readlink(p.c_str(), buff, PATH_MAX)) == -1) {
    return err.report(capture_errno());
  }
  _LIBCPP_ASSERT(ret <= PATH_MAX, "TODO");
  _LIBCPP_ASSERT(ret > 0, "TODO");
  buff[ret] = 0;
  return {buff};
}

bool __remove(const path& p, error_code* ec) {
  ErrorHandler<bool> err("remove", ec, &p);
  if (::remove(p.c_str()) == -1) {
    if (errno != ENOENT)
      err.report(capture_errno());
    return false;
  }
  return true;
}

namespace {

uintmax_t remove_all_impl(path const& p, error_code& ec) {
  const auto npos = static_cast<uintmax_t>(-1);
  const file_status st = __symlink_status(p, &ec);
  if (ec)
    return npos;
  uintmax_t count = 1;
  if (is_directory(st)) {
    for (directory_iterator it(p, ec); !ec && it != directory_iterator();
         it.increment(ec)) {
      auto other_count = remove_all_impl(it->path(), ec);
      if (ec)
        return npos;
      count += other_count;
    }
    if (ec)
      return npos;
  }
  if (!__remove(p, &ec))
    return npos;
  return count;
}

} // end namespace

uintmax_t __remove_all(const path& p, error_code* ec) {
  ErrorHandler<uintmax_t> err("remove_all", ec, &p);

  error_code mec;
  auto count = remove_all_impl(p, mec);
  if (mec) {
    if (mec == errc::no_such_file_or_directory)
      return 0;
    return err.report(mec);
  }
  return count;
}

void __rename(const path& from, const path& to, error_code* ec) {
  ErrorHandler<void> err("rename", ec, &from, &to);
  if (::rename(from.c_str(), to.c_str()) == -1)
    err.report(capture_errno());
}

void __resize_file(const path& p, uintmax_t size, error_code* ec) {
  ErrorHandler<void> err("resize_file", ec, &p);
  if (::truncate(p.c_str(), static_cast< ::off_t>(size)) == -1)
    return err.report(capture_errno());
}

space_info __space(const path& p, error_code* ec) {
  ErrorHandler<void> err("space", ec, &p);
  space_info si;
  struct statvfs m_svfs = {};
  if (::statvfs(p.c_str(), &m_svfs) == -1) {
    err.report(capture_errno());
    si.capacity = si.free = si.available = static_cast<uintmax_t>(-1);
    return si;
  }
  // Multiply with overflow checking.
  auto do_mult = [&](uintmax_t& out, uintmax_t other) {
    out = other * m_svfs.f_frsize;
    if (other == 0 || out / other != m_svfs.f_frsize)
      out = static_cast<uintmax_t>(-1);
  };
  do_mult(si.capacity, m_svfs.f_blocks);
  do_mult(si.free, m_svfs.f_bfree);
  do_mult(si.available, m_svfs.f_bavail);
  return si;
}

file_status __status(const path& p, error_code* ec) {
  return detail::posix_stat(p, ec);
}

file_status __symlink_status(const path& p, error_code* ec) {
  return detail::posix_lstat(p, ec);
}

path __temp_directory_path(error_code* ec) {
  ErrorHandler<path> err("temp_directory_path", ec);

  const char* env_paths[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
  const char* ret = nullptr;

  for (auto& ep : env_paths)
    if ((ret = getenv(ep)))
      break;
  if (ret == nullptr)
    ret = "/tmp";

  path p(ret);
  error_code m_ec;
  file_status st = detail::posix_stat(p, &m_ec);
  if (!status_known(st))
    return err.report(m_ec, "cannot access path \"%s\"", p);

  if (!exists(st) || !is_directory(st))
    return err.report(errc::not_a_directory, "path \"%s\" is not a directory",
                      p);

  return p;
}

path __weakly_canonical(const path& p, error_code* ec) {
  ErrorHandler<path> err("weakly_canonical", ec, &p);

  if (p.empty())
    return __canonical("", ec);

  path result;
  path tmp;
  tmp.__reserve(p.native().size());
  auto PP = PathParser::CreateEnd(p.native());
  --PP;
  vector<string_view_t> DNEParts;

  while (PP.State != PathParser::PS_BeforeBegin) {
    tmp.assign(createView(p.native().data(), &PP.RawEntry.back()));
    error_code m_ec;
    file_status st = __status(tmp, &m_ec);
    if (!status_known(st)) {
      return err.report(m_ec);
    } else if (exists(st)) {
      result = __canonical(tmp, ec);
      break;
    }
    DNEParts.push_back(*PP);
    --PP;
  }
  if (PP.State == PathParser::PS_BeforeBegin)
    result = __canonical("", ec);
  if (ec)
    ec->clear();
  if (DNEParts.empty())
    return result;
  for (auto It = DNEParts.rbegin(); It != DNEParts.rend(); ++It)
    result /= *It;
  return result.lexically_normal();
}

///////////////////////////////////////////////////////////////////////////////
//                            path definitions
///////////////////////////////////////////////////////////////////////////////

/* constexpr path::value_type path::preferred_separator; */

path& path::replace_extension(path const& replacement) {
  path p = extension();
  if (not p.empty()) {
    __pn_.erase(__pn_.size() - p.native().size());
  }
  if (!replacement.empty()) {
    if (replacement.native()[0] != '.') {
      __pn_ += ".";
    }
    __pn_.append(replacement.__pn_);
  }
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// path.decompose

string_view_t path::__root_name() const {
  auto PP = PathParser::CreateBegin(__pn_);
  if (PP.State == PathParser::PS_InRootName)
    return *PP;
  return {};
}

string_view_t path::__root_directory() const {
  auto PP = PathParser::CreateBegin(__pn_);
  if (PP.State == PathParser::PS_InRootName)
    ++PP;
  if (PP.State == PathParser::PS_InRootDir)
    return *PP;
  return {};
}

string_view_t path::__root_path_raw() const {
  auto PP = PathParser::CreateBegin(__pn_);
  if (PP.State == PathParser::PS_InRootName) {
    auto NextCh = PP.peek();
    if (NextCh && *NextCh == '/') {
      ++PP;
      return createView(__pn_.data(), &PP.RawEntry.back());
    }
    return PP.RawEntry;
  }
  if (PP.State == PathParser::PS_InRootDir)
    return *PP;
  return {};
}

static bool ConsumeRootDir(PathParser* PP) {
  while (PP->State <= PathParser::PS_InRootDir)
    ++(*PP);
  return PP->State == PathParser::PS_AtEnd;
}

string_view_t path::__relative_path() const {
  auto PP = PathParser::CreateBegin(__pn_);
  if (ConsumeRootDir(&PP))
    return {};
  return createView(PP.RawEntry.data(), &__pn_.back());
}

string_view_t path::__parent_path() const {
  if (empty())
    return {};
  // Determine if we have a root path but not a relative path. In that case
  // return *this.
  {
    auto PP = PathParser::CreateBegin(__pn_);
    if (ConsumeRootDir(&PP))
      return __pn_;
  }
  // Otherwise remove a single element from the end of the path, and return
  // a string representing that path
  {
    auto PP = PathParser::CreateEnd(__pn_);
    --PP;
    if (PP.RawEntry.data() == __pn_.data())
      return {};
    --PP;
    return createView(__pn_.data(), &PP.RawEntry.back());
  }
}

string_view_t path::__filename() const {
  if (empty())
    return {};
  {
    PathParser PP = PathParser::CreateBegin(__pn_);
    if (ConsumeRootDir(&PP))
      return {};
  }
  return *(--PathParser::CreateEnd(__pn_));
}

string_view_t path::__stem() const {
  return parser::separate_filename(__filename()).first;
}

string_view_t path::__extension() const {
  return parser::separate_filename(__filename()).second;
}

////////////////////////////////////////////////////////////////////////////
// path.gen

enum PathPartKind : unsigned char {
  PK_None,
  PK_RootSep,
  PK_Filename,
  PK_Dot,
  PK_DotDot,
  PK_TrailingSep
};

static PathPartKind ClassifyPathPart(string_view_t Part) {
  if (Part.empty())
    return PK_TrailingSep;
  if (Part == ".")
    return PK_Dot;
  if (Part == "..")
    return PK_DotDot;
  if (Part == "/")
    return PK_RootSep;
  return PK_Filename;
}

path path::lexically_normal() const {
  if (__pn_.empty())
    return *this;

  using PartKindPair = pair<string_view_t, PathPartKind>;
  vector<PartKindPair> Parts;
  // Guess as to how many elements the path has to avoid reallocating.
  Parts.reserve(32);

  // Track the total size of the parts as we collect them. This allows the
  // resulting path to reserve the correct amount of memory.
  size_t NewPathSize = 0;
  auto AddPart = [&](PathPartKind K, string_view_t P) {
    NewPathSize += P.size();
    Parts.emplace_back(P, K);
  };
  auto LastPartKind = [&]() {
    if (Parts.empty())
      return PK_None;
    return Parts.back().second;
  };

  bool MaybeNeedTrailingSep = false;
  // Build a stack containing the remaining elements of the path, popping off
  // elements which occur before a '..' entry.
  for (auto PP = PathParser::CreateBegin(__pn_); PP; ++PP) {
    auto Part = *PP;
    PathPartKind Kind = ClassifyPathPart(Part);
    switch (Kind) {
    case PK_Filename:
    case PK_RootSep: {
      // Add all non-dot and non-dot-dot elements to the stack of elements.
      AddPart(Kind, Part);
      MaybeNeedTrailingSep = false;
      break;
    }
    case PK_DotDot: {
      // Only push a ".." element if there are no elements preceding the "..",
      // or if the preceding element is itself "..".
      auto LastKind = LastPartKind();
      if (LastKind == PK_Filename) {
        NewPathSize -= Parts.back().first.size();
        Parts.pop_back();
      } else if (LastKind != PK_RootSep)
        AddPart(PK_DotDot, "..");
      MaybeNeedTrailingSep = LastKind == PK_Filename;
      break;
    }
    case PK_Dot:
    case PK_TrailingSep: {
      MaybeNeedTrailingSep = true;
      break;
    }
    case PK_None:
      _LIBCPP_UNREACHABLE();
    }
  }
  // [fs.path.generic]p6.8: If the path is empty, add a dot.
  if (Parts.empty())
    return ".";

  // [fs.path.generic]p6.7: If the last filename is dot-dot, remove any
  // trailing directory-separator.
  bool NeedTrailingSep = MaybeNeedTrailingSep && LastPartKind() == PK_Filename;

  path Result;
  Result.__pn_.reserve(Parts.size() + NewPathSize + NeedTrailingSep);
  for (auto& PK : Parts)
    Result /= PK.first;

  if (NeedTrailingSep)
    Result /= "";

  return Result;
}

static int DetermineLexicalElementCount(PathParser PP) {
  int Count = 0;
  for (; PP; ++PP) {
    auto Elem = *PP;
    if (Elem == "..")
      --Count;
    else if (Elem != ".")
      ++Count;
  }
  return Count;
}

path path::lexically_relative(const path& base) const {
  { // perform root-name/root-directory mismatch checks
    auto PP = PathParser::CreateBegin(__pn_);
    auto PPBase = PathParser::CreateBegin(base.__pn_);
    auto CheckIterMismatchAtBase = [&]() {
      return PP.State != PPBase.State &&
             (PP.inRootPath() || PPBase.inRootPath());
    };
    if (PP.State == PathParser::PS_InRootName &&
        PPBase.State == PathParser::PS_InRootName) {
      if (*PP != *PPBase)
        return {};
    } else if (CheckIterMismatchAtBase())
      return {};

    if (PP.inRootPath())
      ++PP;
    if (PPBase.inRootPath())
      ++PPBase;
    if (CheckIterMismatchAtBase())
      return {};
  }

  // Find the first mismatching element
  auto PP = PathParser::CreateBegin(__pn_);
  auto PPBase = PathParser::CreateBegin(base.__pn_);
  while (PP && PPBase && PP.State == PPBase.State && *PP == *PPBase) {
    ++PP;
    ++PPBase;
  }

  // If there is no mismatch, return ".".
  if (!PP && !PPBase)
    return ".";

  // Otherwise, determine the number of elements, 'n', which are not dot or
  // dot-dot minus the number of dot-dot elements.
  int ElemCount = DetermineLexicalElementCount(PPBase);
  if (ElemCount < 0)
    return {};

  // return a path constructed with 'n' dot-dot elements, followed by the the
  // elements of '*this' after the mismatch.
  path Result;
  // FIXME: Reserve enough room in Result that it won't have to re-allocate.
  while (ElemCount--)
    Result /= "..";
  for (; PP; ++PP)
    Result /= *PP;
  return Result;
}

////////////////////////////////////////////////////////////////////////////
// path.comparisons
int path::__compare(string_view_t __s) const {
  auto PP = PathParser::CreateBegin(__pn_);
  auto PP2 = PathParser::CreateBegin(__s);
  while (PP && PP2) {
    int res = (*PP).compare(*PP2);
    if (res != 0)
      return res;
    ++PP;
    ++PP2;
  }
  if (PP.State == PP2.State && !PP)
    return 0;
  if (!PP)
    return -1;
  return 1;
}

////////////////////////////////////////////////////////////////////////////
// path.nonmembers
size_t hash_value(const path& __p) noexcept {
  auto PP = PathParser::CreateBegin(__p.native());
  size_t hash_value = 0;
  hash<string_view_t> hasher;
  while (PP) {
    hash_value = __hash_combine(hash_value, hasher(*PP));
    ++PP;
  }
  return hash_value;
}

////////////////////////////////////////////////////////////////////////////
// path.itr
path::iterator path::begin() const {
  auto PP = PathParser::CreateBegin(__pn_);
  iterator it;
  it.__path_ptr_ = this;
  it.__state_ = static_cast<path::iterator::_ParserState>(PP.State);
  it.__entry_ = PP.RawEntry;
  it.__stashed_elem_.__assign_view(*PP);
  return it;
}

path::iterator path::end() const {
  iterator it{};
  it.__state_ = path::iterator::_AtEnd;
  it.__path_ptr_ = this;
  return it;
}

path::iterator& path::iterator::__increment() {
  PathParser PP(__path_ptr_->native(), __entry_, __state_);
  ++PP;
  __state_ = static_cast<_ParserState>(PP.State);
  __entry_ = PP.RawEntry;
  __stashed_elem_.__assign_view(*PP);
  return *this;
}

path::iterator& path::iterator::__decrement() {
  PathParser PP(__path_ptr_->native(), __entry_, __state_);
  --PP;
  __state_ = static_cast<_ParserState>(PP.State);
  __entry_ = PP.RawEntry;
  __stashed_elem_.__assign_view(*PP);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
//                           directory entry definitions
///////////////////////////////////////////////////////////////////////////////

#ifndef _LIBCPP_WIN32API
error_code directory_entry::__do_refresh() noexcept {
  __data_.__reset();
  error_code failure_ec;

  StatT full_st;
  file_status st = detail::posix_lstat(__p_, full_st, &failure_ec);
  if (!status_known(st)) {
    __data_.__reset();
    return failure_ec;
  }

  if (!_VSTD_FS::exists(st) || !_VSTD_FS::is_symlink(st)) {
    __data_.__cache_type_ = directory_entry::_RefreshNonSymlink;
    __data_.__type_ = st.type();
    __data_.__non_sym_perms_ = st.permissions();
  } else { // we have a symlink
    __data_.__sym_perms_ = st.permissions();
    // Get the information about the linked entity.
    // Ignore errors from stat, since we don't want errors regarding symlink
    // resolution to be reported to the user.
    error_code ignored_ec;
    st = detail::posix_stat(__p_, full_st, &ignored_ec);

    __data_.__type_ = st.type();
    __data_.__non_sym_perms_ = st.permissions();

    // If we failed to resolve the link, then only partially populate the
    // cache.
    if (!status_known(st)) {
      __data_.__cache_type_ = directory_entry::_RefreshSymlinkUnresolved;
      return error_code{};
    }
    // Otherwise, we resolved the link, potentially as not existing.
    // That's OK.
    __data_.__cache_type_ = directory_entry::_RefreshSymlink;
  }

  if (_VSTD_FS::is_regular_file(st))
    __data_.__size_ = static_cast<uintmax_t>(full_st.st_size);

  if (_VSTD_FS::exists(st)) {
    __data_.__nlink_ = static_cast<uintmax_t>(full_st.st_nlink);

    // Attempt to extract the mtime, and fail if it's not representable using
    // file_time_type. For now we ignore the error, as we'll report it when
    // the value is actually used.
    error_code ignored_ec;
    __data_.__write_time_ =
        __extract_last_write_time(__p_, full_st, &ignored_ec);
  }

  return failure_ec;
}
#else
error_code directory_entry::__do_refresh() noexcept {
  __data_.__reset();
  error_code failure_ec;

  file_status st = _VSTD_FS::symlink_status(__p_, failure_ec);
  if (!status_known(st)) {
    __data_.__reset();
    return failure_ec;
  }

  if (!_VSTD_FS::exists(st) || !_VSTD_FS::is_symlink(st)) {
    __data_.__cache_type_ = directory_entry::_RefreshNonSymlink;
    __data_.__type_ = st.type();
    __data_.__non_sym_perms_ = st.permissions();
  } else { // we have a symlink
    __data_.__sym_perms_ = st.permissions();
    // Get the information about the linked entity.
    // Ignore errors from stat, since we don't want errors regarding symlink
    // resolution to be reported to the user.
    error_code ignored_ec;
    st = _VSTD_FS::status(__p_, ignored_ec);

    __data_.__type_ = st.type();
    __data_.__non_sym_perms_ = st.permissions();

    // If we failed to resolve the link, then only partially populate the
    // cache.
    if (!status_known(st)) {
      __data_.__cache_type_ = directory_entry::_RefreshSymlinkUnresolved;
      return error_code{};
    }
    __data_.__cache_type_ = directory_entry::_RefreshSymlink;
  }

  // FIXME: This is currently broken, and the implementation only a placeholder.
  // We need to cache last_write_time, file_size, and hard_link_count here before
  // the implementation actually works.

  return failure_ec;
}
#endif

_LIBCPP_END_NAMESPACE_FILESYSTEM

/* -- END filesystem/operations.cpp -- */

#endif // __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
#endif // PLATFORM(MAC)
