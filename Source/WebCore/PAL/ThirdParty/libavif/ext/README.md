# libavif ext dir

This contains references to various external repositories which are known to
build and work properly with the current libavif. If you are building for
Windows or any kind of fully static (embedded) release, using these scripts in
conjunction with the `BUILD_SHARED_LIBS=0` and `AVIF_LOCAL_*` CMake flags make
for a convenient way to get all of the dependencies necessary. This method is
how many of libavif's continuous builders work.

However, if you are building this for a distribution or Unix-like environment
in general, you can safely *ignore* this directory. Allow `BUILD_SHARED_LIBS`
to keep its default (`ON`), enable the appropriate `AVIF_BUILD_*` CMake flags
depending on which shared AV1 codec libraries you plan to leverage and depend
on, and use the system's zlib, libpng, and libjpeg.
