// FFT routines.  Obtained from
// https://github.com/rtoy/js-hacks/blob/master/fft/fft-r2-nn.js.
//
// Create an FFT object of order |order|.  This will
// compute forward and inverse FFTs of length 2^|order|.
function FFT(order) {
  if (order <= 1) {
    throw new this.FFTException(order);
  }

  this.order = order;
  this.N = 1 << order;
  this.halfN = 1 << (order - 1);

  // Internal variables needed for computing each stage of the FFT.
  this.pairsInGroup = 0;
  this.numberOfGroups = 0;
  this.distance = 0;
  this.notSwitchInput = 0;

  // Work arrays
  this.aReal = new Float32Array(this.N);
  this.aImag = new Float32Array(this.N);
  this.debug = 0;

  // Twiddle tables for the FFT.
  this.twiddleCos = new Float32Array(this.N);
  this.twiddleSin = new Float32Array(this.N);

  let omega = -2 * Math.PI / this.N;
  for (let k = 0; k < this.N; ++k) {
    this.twiddleCos[k] = Math.fround(Math.cos(omega * k));
    this.twiddleSin[k] = Math.fround(Math.sin(omega * k));
  }
}

FFT.prototype.FFTException =
    function(order) {
  this.value = order;
  this.message = 'Order must be greater than 1: ';
  this.toString = function() {
    return this.message + this.value;
  };
}

    // Core routine that does one stage of the FFT, implementing all of
    // the butterflies for that stage.
    FFT.prototype.FFTRadix2Core =
        function(aReal, aImag, bReal, bImag) {
  let index = 0;
  for (let k = 0; k < this.numberOfGroups; ++k) {
    let jfirst = 2 * k * this.pairsInGroup;
    let jlast = jfirst + this.pairsInGroup - 1;
    let jtwiddle = k * this.pairsInGroup;
    let wr = this.twiddleCos[jtwiddle];
    let wi = this.twiddleSin[jtwiddle];

    for (let j = jfirst; j <= jlast; ++j) {
      // temp = w * a[j + distance]
      let idx = j + this.distance;
      let tr = wr * aReal[idx] - wi * aImag[idx];
      let ti = wr * aImag[idx] + wi * aReal[idx];

      bReal[index] = aReal[j] + tr;
      bImag[index] = aImag[j] + ti;
      bReal[index + this.halfN] = aReal[j] - tr;
      bImag[index + this.halfN] = aImag[j] - ti;
      ++index;
    }
  }
}

        // Forward out-of-place complex FFT.
        //
        // Computes the forward FFT, b,  of a complex vector x:
        //
        //   b[k] = sum(x[n] * W^(k*n), n, 0, N - 1), k = 0, 1,..., N-1
        //
        // where
        //   N = length of x, which must be a power of 2 and
        //   W = exp(-2*i*pi/N)
        //
        //   x = |xr| + i*|xi|
        //   b = |bReal| + i*|bImag|
        FFT.prototype.fft =
            function(xr, xi, bReal, bImag) {
  this.pairsInGroup = this.halfN;
  this.numberOfGroups = 1;
  this.distance = this.halfN;
  this.notSwitchInput = true;

  // Arrange it so that the last iteration puts the desired output
  // in bReal/bImag.
  if (this.order & 1 == 1) {
    this.FFTRadix2Core(xr, xi, bReal, bImag);
    this.notSwitchInput = !this.notSwitchInput;
  } else {
    this.FFTRadix2Core(xr, xi, this.aReal, this.aImag);
  }

  this.pairsInGroup >>= 1;
  this.numberOfGroups <<= 1;
  this.distance >>= 1;

  while (this.numberOfGroups < this.N) {
    if (this.notSwitchInput) {
      this.FFTRadix2Core(this.aReal, this.aImag, bReal, bImag);
    } else {
      this.FFTRadix2Core(bReal, bImag, this.aReal, this.aImag);
    }

    this.notSwitchInput = !this.notSwitchInput;
    this.pairsInGroup >>= 1;
    this.numberOfGroups <<= 1;
    this.distance >>= 1;
  }
}

            // Core routine that does one stage of the FFT, implementing all of
            // the butterflies for that stage.  This is identical to
            // FFTRadix2Core, except the twiddle factor, w, is the conjugate.
            FFT.prototype.iFFTRadix2Core =
                function(aReal, aImag, bReal, bImag) {
  let index = 0;
  for (let k = 0; k < this.numberOfGroups; ++k) {
    let jfirst = 2 * k * this.pairsInGroup;
    let jlast = jfirst + this.pairsInGroup - 1;
    let jtwiddle = k * this.pairsInGroup;
    let wr = this.twiddleCos[jtwiddle];
    let wi = -this.twiddleSin[jtwiddle];

    for (let j = jfirst; j <= jlast; ++j) {
      // temp = w * a[j + distance]
      let idx = j + this.distance;
      let tr = wr * aReal[idx] - wi * aImag[idx];
      let ti = wr * aImag[idx] + wi * aReal[idx];

      bReal[index] = aReal[j] + tr;
      bImag[index] = aImag[j] + ti;
      bReal[index + this.halfN] = aReal[j] - tr;
      bImag[index + this.halfN] = aImag[j] - ti;
      ++index;
    }
  }
}

                // Inverse out-of-place complex FFT.
                //
                // Computes the inverse FFT, b,  of a complex vector x:
                //
                //   b[k] = sum(x[n] * W^(-k*n), n, 0, N - 1), k = 0, 1,..., N-1
                //
                // where
                //   N = length of x, which must be a power of 2 and
                //   W = exp(-2*i*pi/N)
                //
                //   x = |xr| + i*|xi|
                //   b = |bReal| + i*|bImag|
                //
                // Note that ifft(fft(x)) = N * x.  To get x, call ifftScale to
                // scale the output of the ifft appropriately.
                FFT.prototype.ifft =
                    function(xr, xi, bReal, bImag) {
  this.pairsInGroup = this.halfN;
  this.numberOfGroups = 1;
  this.distance = this.halfN;
  this.notSwitchInput = true;

  // Arrange it so that the last iteration puts the desired output
  // in bReal/bImag.
  if (this.order & 1 == 1) {
    this.iFFTRadix2Core(xr, xi, bReal, bImag);
    this.notSwitchInput = !this.notSwitchInput;
  } else {
    this.iFFTRadix2Core(xr, xi, this.aReal, this.aImag);
  }

  this.pairsInGroup >>= 1;
  this.numberOfGroups <<= 1;
  this.distance >>= 1;

  while (this.numberOfGroups < this.N) {
    if (this.notSwitchInput) {
      this.iFFTRadix2Core(this.aReal, this.aImag, bReal, bImag);
    } else {
      this.iFFTRadix2Core(bReal, bImag, this.aReal, this.aImag);
    }

    this.notSwitchInput = !this.notSwitchInput;
    this.pairsInGroup >>= 1;
    this.numberOfGroups <<= 1;
    this.distance >>= 1;
  }
}

                    //
                    // Scales the IFFT by 1/N, done in place.
                    FFT.prototype.ifftScale =
                        function(xr, xi) {
  let factor = 1 / this.N;
  for (let k = 0; k < this.N; ++k) {
    xr[k] *= factor;
    xi[k] *= factor;
  }
}

                        // First stage for the RFFT.  Basically the same as
                        // FFTRadix2Core, but we assume aImag is 0, and adjust
                        // the code accordingly.
                        FFT.prototype.RFFTRadix2CoreStage1 =
                            function(aReal, bReal, bImag) {
  let index = 0;
  for (let k = 0; k < this.numberOfGroups; ++k) {
    let jfirst = 2 * k * this.pairsInGroup;
    let jlast = jfirst + this.pairsInGroup - 1;
    let jtwiddle = k * this.pairsInGroup;
    let wr = this.twiddleCos[jtwiddle];
    let wi = this.twiddleSin[jtwiddle];

    for (let j = jfirst; j <= jlast; ++j) {
      // temp = w * a[j + distance]
      let idx = j + this.distance;
      let tr = wr * aReal[idx];
      let ti = wi * aReal[idx];

      bReal[index] = aReal[j] + tr;
      bImag[index] = ti;
      bReal[index + this.halfN] = aReal[j] - tr;
      bImag[index + this.halfN] = -ti;
      ++index;
    }
  }
}

                            // Forward Real FFT.  Like fft, but the signal is
                            // assumed to be real, so the imaginary part is not
                            // supplied.  The output, however, is still the same
                            // and is returned in two arrays.  (This could be
                            // optimized to use less storage, both internally
                            // and for the output, but we don't do that.)
                            FFT.prototype.rfft = function(xr, bReal, bImag) {
  this.pairsInGroup = this.halfN;
  this.numberOfGroups = 1;
  this.distance = this.halfN;
  this.notSwitchInput = true;

  // Arrange it so that the last iteration puts the desired output
  // in bReal/bImag.
  if (this.order & 1 == 1) {
    this.RFFTRadix2CoreStage1(xr, bReal, bImag);
    this.notSwitchInput = !this.notSwitchInput;
  } else {
    this.RFFTRadix2CoreStage1(xr, this.aReal, this.aImag);
  }

  this.pairsInGroup >>= 1;
  this.numberOfGroups <<= 1;
  this.distance >>= 1;

  while (this.numberOfGroups < this.N) {
    if (this.notSwitchInput) {
      this.FFTRadix2Core(this.aReal, this.aImag, bReal, bImag);
    } else {
      this.FFTRadix2Core(bReal, bImag, this.aReal, this.aImag);
    }

    this.notSwitchInput = !this.notSwitchInput;
    this.pairsInGroup >>= 1;
    this.numberOfGroups <<= 1;
    this.distance >>= 1;
  }
}
