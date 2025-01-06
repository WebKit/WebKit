# Introduction

Several routines in libyuv have multiple implementations specialized for a
variety of CPU architecture extensions. Libyuv will automatically detect and
use the latest architecture extension present on a machine for which a kernel
implementation is available.

# Feature detection on AArch64

## Architecture extensions of interest

The Arm 64-bit A-class architecture has a number of vector extensions which can
be used to accelerate libyuv kernels.

### Neon extensions

Neon is available and mandatory in AArch64 from the base Armv8.0-A
architecture. Neon can be used even if later extensions like the Scalable
Vector Extension (SVE) are also present. The exception to this is if the CPU is
currently operating in streaming mode as introduced by the Scalable Matrix
Extension, described later.

There are also a couple of architecture extensions present for Neon that we can
take advantage of in libyuv:

* The Neon DotProd extension is architecturally available from Armv8.1-A and
  becomes mandatory from Armv8.4-A. This extension provides instructions to
  perform a pairwise widening multiply of groups of four bytes from two source
  vectors, taking the sum of the four widened multiply results within each
  group to give a 32-bit result, accumulating into a destination vector.

* The Neon I8MM extension extends the DotProd extension with support for
  mixed-sign DotProds. The I8MM extension is architecturally available from
  Armv8.1-A and becomes mandatory from Armv8.6-A. It does not strictly depend
  on the DotProd extension being implemented, however at time of writing there
  is no known micro-architecture implementation where I8MM is implemented
  without the DotProd extension also being implemented.

### The Scalable Vector Extension (SVE)

The two Scalable Vector extensions (SVE and SVE2) provides equivalent
functionality to most existing Neon instructions but with the ability to
efficiently operate on vector registers with a run-time-determined vector
length.

The original version of SVE is architecturally available from Armv8.2-A and is
primarily targeted at HPC applications. This focus means it does not include
most of the DSP-style operations that are necessary for most libyuv
color-conversion kernels, though it can still be used for many scaling or
rotation kernels.

SVE does not strictly depend on either of the Neon DotProd or I8MM extensions
being implemented. The only micro-architecture at time of writing where SVE is
implemented without these two extensions both also being implemented is the
Fujitsu A64FX, which is not a CPU of interest for libyuv.

SVE2 extends the base SVE extension with the remaining instructions from Neon,
porting these instructions to operate on scalable vectors. SVE2 is
architecturally available from Armv9.0-A. If SVE2 is implemented then SVE must
also be implemented. Since Armv9.0-A is based on Armv8.5-A this implies that
the Neon DotProd extension is also implemented. Interestingly this means that
the I8MM extension is not mandatory since it only becomes mandatory from
Armv8.6-A or Armv9.1-A, however there is no micro-architecture at time of
writing where SVE2 is implemented without all previously-mentioned features
also being implemented.

### The Scalable Matrix Extension (SME)

The Scalable Matrix Extension (SME) is an optional feature introduced from
Armv9.2-A. SME exists alongside SVE and introduces new execution modes for
applications performing extended periods of data processing. In particular SME
introduces a few new components of interest:

* Access to a scalable two-dimensional ZA tile register and new instructions to
  interact with rows and columns of the ZA tiles. This can be useful for data
  transformations like transposes.

* A streaming SVE (SSVE) mode, during which the SVE vector length matches the
  ZA tile register width. In typical systems where the ZA tile register width
  is longer than the core SVE vector length, SSVE processing allows for faster
  data processing, even if the ZA tile register is unused.  While the CPU is
  executing in streaming mode, Neon instructions are unavailable.

* When both SSVE and the ZA tile registers are enabled there are additional
  outer-product instructions accumulating into a whole ZA tile, suitable for
  accelerating matrix arithmetic. This is likely less useful in libyuv.

## Linux and Android

On AArch64 running under Linux and Android, features are detected by inspecting
the CPU auxiliary vector via `getauxval(AT_HWCAP)` and `getauxval(AT_HWCAP2)`,
inspecting the returned bitmask.

## Windows

On Windows we detect features using the `IsProcessorFeaturePresent` interface
and passing an enum parameter for the feature we want to check. More
information on this can be found here:

    https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent#parameters

## Apple Silicon

On Apple Silicon we detect features using the `sysctlbyname` interface and
passing a string representing the feature we want to detect. More information
on this can be found here:

    https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_instruction_set_characteristics
