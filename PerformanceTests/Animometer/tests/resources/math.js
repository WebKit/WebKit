var Matrix =
{
    init: function(m, n, v)
    {
        return Array(m * n).fill(v);
    },

    zeros: function(m, n)
    {
        return Matrix.init(m, n, 0);
    },

    ones: function(m, n)
    {
        return Matrix.init(m, n, 1);
    },

    identity: function(n)
    {
        var out = new Matrix.zeros(n, n);
        for (var i = 0; i < n; ++i)
            out[i * n + i] = 1;
        return out;
    },

    str: function(A, n, m)
    {
        var out = (m > 1 && n > 1 ? "Matrix[" + n + ", " + m : "Vector[" + m * n) + "] = [";
        for (var i = 0; i < n * m; ++i) {
            out += A[i];
            if (i < n * m - 1)
                out += ", ";
        }
        return out + "]";
    },

    pos: function(m, i, j)
    {
        return m * i + j;
    },

    add: function(A, B, n, m)
    {
        var out = Matrix.zeros(n, m);
        for (var i = 0; i < n * m; ++i)
            out[i] = A[i] + B[i];
        return out;
    },

    subtract: function(A, B, n, m)
    {
        var out = Matrix.zeros(n, m);
        for (var i = 0; i < n * m; ++i)
            out[i] = A[i] - B[i];
        return out;
    },

    scale: function(s, A, n, m)
    {
        var out = Matrix.zeros(n, m);
        for (var i = 0; i < n * m; ++i)
            out[i] = s * A[i];
        return out;
    },

    transpose: function(A, n, m)
    {
        var out = Matrix.zeros(m, n);
        for (var i = 0; i < n; ++i) {
            for (var j = 0; j < m; ++j)
                out[Matrix.pos(n, i, j)] = A[Matrix.pos(m, j, i)];
        }
        return out;
    },

    multiply: function(A, B, n, m, p)
    {
        var out = Matrix.zeros(n, p);
        for (var i = 0; i < n; ++i) {
            for (var j = 0; j < p; ++j) {
                for (var k = 0; k < m; ++k) {
                    out[Matrix.pos(p, i, j)] += A[Matrix.pos(m, i, k)] * B[Matrix.pos(p, k, j)];
                }
            }
        }
        return out;
    }
}

var Vector3 =
{
    zeros: function()
    {
        return Matrix.zeros(1, 3);
    },

    ones: function()
    {
        return Matrix.ones(1, 3);
    },

    str: function(v)
    {
        return Matrix.str(v, 1, 3);
    },

    add: function(v, w)
    {
        return Matrix.add(v, w, 1, 3);
    },

    subtract: function(v, w)
    {
        return Matrix.subtract(v, w, 1, 3);
    },

    scale: function(s, v)
    {
        return Matrix.scale(s, v, 1, 3);
    },

    multiplyMatrix3: function(v, A)
    {
        return Matrix.multiply(v, A, 1, 3, 3);
    },

    multiplyVector3: function(v, w)
    {
        var out = 0;
        for (var i = 0; i < 3; ++i)
            out += v[i] * w[i];
        return out;
    }
}

var Matrix3 =
{
    zeros: function()
    {
        return Matrix.zeros(3, 3);
    },

    identity: function()
    {
        return Matrix.identity(3, 3);
    },

    str: function(A)
    {
        return Matrix.str(A, 3, 3);
    },

    pos: function(i, j)
    {
        return Matrix.pos(3, i, j);
    },

    add: function(A, B)
    {
        return Matrix.add(A, B, 3, 3);
    },

    subtract: function(A, B)
    {
        return Matrix.subtract(A, B, 3, 3);
    },

    scale: function(s, A)
    {
        return Matrix.scale(s, A, 3, 3);
    },

    transpose: function(A)
    {
        return Matrix.transpose(A, 3, 3);
    },

    multiplyMatrix3: function(A, B)
    {
        return Matrix.multiply(A, B, 3, 3, 3);
    },

    multiplyVector3: function(A, v)
    {
        return Matrix.multiply(A, v, 3, 3, 1);
    }
}

function PIDController(ysp)
{
    this._ysp = ysp;
    this._out = 0;

    this._Kp = 0;
    this._stage = PIDController.stages.WARMING;

    this._eold = 0;
    this._I = 0;
}

// This enum will be used to tell whether the system output (or the controller input)
// is moving towards the set-point or away from it.
PIDController.yPositions = {
    BEFORE_SETPOINT: 0,
    AFTER_SETPOINT: 1
}

// The Ziegler–Nichols method for is used tuning the PID controller. The workflow of
// the tuning is split into four stages. The first two stages determine the values
// of the PID controller gains. During these two stages we return the proportional
// term only. The third stage is used to determine the min-max values of the
// saturation actuator. In the last stage back-calculation and tracking are applied
// to avoid integrator windup. During the last two stages, we return a PID control
// value.
PIDController.stages = {
    WARMING: 0,         // Increase the value of the Kp until the system output reaches ysp.
    OVERSHOOT: 1,       // Measure the oscillation period and the overshoot value
    UNDERSHOOT: 2,      // Return PID value and measure the undershoot value
    SATURATE: 3         // Return PID value and apply back-calculation and tracking.
}

PIDController.prototype =
{
    // Determines whether the current y is
    //  before ysp => (below ysp if ysp > y0) || (above ysp if ysp < y0)
    //  after ysp => (above ysp if ysp > y0) || (below ysp if ysp < y0)
    _yPosition: function(y)
    {
        return (y < this._ysp) == (this._y0 < this._ysp)
            ? PIDController.yPositions.BEFORE_SETPOINT
            : PIDController.yPositions.AFTER_SETPOINT;
    },

    // Calculate the ultimate distance from y0 after time t. We want to move very
    // slowly at the beginning to see how adding few items to the test can affect
    // its output. The complexity of a single item might be big enough to keep the
    // proportional gain very small but achieves the desired progress. But if y does
    // not change significantly after adding few items, that means we need a much
    // bigger gain. So we need to move over a cubic curve which increases very
    // slowly with small t values but moves very fast with larger t values.
    // The basic formula is: y = t^3
    // Change the formula to reach y=1 after 1000 ms: y = (t/1000)^3
    // Change the formula to reach y=(ysp - y0) after 1000 ms: y = (ysp - y0) * (t/1000)^3
    _distanceUltimate: function(t)
    {
        return (this._ysp - this._y0) * Math.pow(t / 1000, 3);
    },

    // Calculates the distance of y relative to y0. It also ensures we do not return
    // zero by returning a epsilon value in the same direction as ultimate distance.
    _distance: function(y, du)
    {
        const epsilon = 0.0001;
        var d  = y - this._y0;
        return du < 0 ? Math.min(d, -epsilon) : Math.max(d, epsilon);
    },

    // Decides how much the proportional gain should be increased during the manual
    // gain stage. We choose to use the ratio of the ultimate distance to the current
    // distance as an indication of how much the system is responsive. We want
    // to keep the increment under control so it does not cause the system instability
    // So we choose to take the natural logarithm of this ratio.
    _gainIncrement: function(t, y, e)
    {
        var du = this._distanceUltimate(t);
        var d = this._distance(y, du);
        return Math.log(du / d) * 0.1;
    },

    // Update the stage of the controller based on its current stage and the system output
    _updateStage: function(y)
    {
        var yPosition = this._yPosition(y);

        switch (this._stage) {
        case PIDController.stages.WARMING:
            if (yPosition == PIDController.yPositions.AFTER_SETPOINT)
                this._stage = PIDController.stages.OVERSHOOT;
            break;

        case PIDController.stages.OVERSHOOT:
            if (yPosition == PIDController.yPositions.BEFORE_SETPOINT)
                this._stage = PIDController.stages.UNDERSHOOT;
            break;

        case PIDController.stages.UNDERSHOOT:
            if (yPosition == PIDController.yPositions.AFTER_SETPOINT)
                this._stage = PIDController.stages.SATURATE;
            break;
        }
    },

    // Manual tuning is used before calculating the PID controller gains.
    _tuneP: function(e)
    {
        // The output is the proportional term only.
        return this._Kp * e;
    },

    // PID tuning function. Kp, Ti and Td were already calculated
    _tunePID: function(h, y, e)
    {
        // Proportional term.
        var P = this._Kp * e;

        // Integral term is the area under the curve starting from the beginning
        // till the current time.
        this._I += (this._Kp / this._Ti) * ((e + this._eold) / 2) * h;

        // Derivative term is the slope of the curve at the current time.
        var D = (this._Kp * this._Td) * (e - this._eold) / h;

        // The ouput is a PID function.
       return P + this._I + D;
    },

    // Apply different strategies for the tuning based on the stage of the controller.
    _tune: function(t, h, y, e)
    {
        switch (this._stage) {
        case PIDController.stages.WARMING:
            // This is the first stage of the ZieglerâNichols method. It increments
            // the proportional gain till the system output passes the set-point value.
            if (typeof this._y0 == "undefined") {
                // This is the first time a tuning value is required. We want the test
                // to add only one item. So we need to return -1 which forces us to
                // choose the initial value of Kp to be = -1 / e
                this._y0 = y;
                this._Kp = -1 / e;
            } else {
                // Keep incrementing the Kp as long as we have not reached the
                // set-point yet
                this._Kp += this._gainIncrement(t, y, e);
            }

            return this._tuneP(e);

        case PIDController.stages.OVERSHOOT:
            // This is the second stage of the ZieglerâNichols method. It measures the
            // oscillation period.
            if (typeof this._t0 == "undefined") {
                // t is the time of the begining of the first overshot
                this._t0 = t;
                this._Kp /= 2;
            }

            return this._tuneP(e);

        case PIDController.stages.UNDERSHOOT:
            // This is the end of the ZieglerâNichols method. We need to calculate the
            // integral and derivative periods.
            if (typeof this._Ti == "undefined") {
                // t is the time of the end of the first overshot
                var Tu = t - this._t0;

                // Calculate the system parameters from Kp and Tu assuming
                // a "some overshoot" control type. See:
                // https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method
                this._Ti = Tu / 2;
                this._Td = Tu / 3;
                this._Kp = 0.33 * this._Kp;

                // Calculate the tracking time.
                this._Tt = Math.sqrt(this._Ti * this._Td);
            }

            return this._tunePID(h, y, e);

        case PIDController.stages.SATURATE:
            return this._tunePID(h, y, e);
        }

        return 0;
    },

    // Ensures the system does not fluctuates.
    _saturate: function(v, e)
    {
        var u = v;

        switch (this._stage) {
        case PIDController.stages.OVERSHOOT:
        case PIDController.stages.UNDERSHOOT:
            // Calculate the min-max values of the saturation actuator.
            if (typeof this._min == "undefined")
                this._min = this._max = this._out;
            else {
                this._min = Math.min(this._min, this._out);
                this._max = Math.max(this._max, this._out);
            }
            break;

        case PIDController.stages.SATURATE:
            const limitPercentage = 0.90;
            var min = this._min > 0 ? Math.min(this._min, this._max * limitPercentage) : this._min;
            var max = this._max < 0 ? Math.max(this._max, this._min * limitPercentage) : this._max;
            var out = this._out + u;

            // Clip the controller output to the min-max values
            out = Math.max(Math.min(max, out), min);
            u = out - this._out;

            // Apply the back-calculation and tracking
            if (u != v)
                u += (this._Kp * this._Tt / this._Ti) * e;
            break;
        }

        this._out += u;
        return u;
    },

    // Called from the benchmark to tune its test. It uses Ziegler–Nichols method
    // to calculate the controller parameters. It then returns a PID tuning value.
    tune: function(t, h, y)
    {
        this._updateStage(y);

        // Current error.
        var e = this._ysp - y;
        var v = this._tune(t, h, y, e);

        // Save e for the next call.
        this._eold = e;

        // Apply back-calculation and tracking to avoid integrator windup
        return this._saturate(v, e);
    }
}

function KalmanEstimator(initX)
{
    // Initialize state transition matrix.
    this._matA = Matrix3.identity();
    this._matA[Matrix3.pos(0, 2)] = 1;

    // Initialize measurement matrix.
    this._vecH = Vector3.zeros();
    this._vecH[0] = 1;

    this._matQ = Matrix3.identity();
    this._R = 1000;

    // Initial state conditions.
    this._vecX_est = Vector3.zeros();
    this._vecX_est[0] = initX;
    this._matP_est = Matrix3.zeros();
}

KalmanEstimator.prototype =
{
    estimate: function(current)
    {
        // Project the state ahead
        //  X_prd(k) = A * X_est(k-1)
        var vecX_prd = Matrix3.multiplyVector3(this._matA, this._vecX_est);

        // Project the error covariance ahead
        //  P_prd(k) = A * P_est(k-1) * A' + Q
        var matP_prd = Matrix3.add(Matrix3.multiplyMatrix3(Matrix3.multiplyMatrix3(this._matA, this._matP_est), Matrix3.transpose(this._matA)), this._matQ);

        // Compute Kalman gain
        //  B = H * P_prd(k)';
        //  S = B * H' + R;
        //  K(k) = (S \ B)';
        var vecB = Vector3.multiplyMatrix3(this._vecH, Matrix3.transpose(matP_prd));
        var S = Vector3.multiplyVector3(vecB, this._vecH) + this._R;
        var vecGain = Vector3.scale(1/S, vecB);

        // Update the estimate via z(k)
        //  X_est(k) = x_prd + K(k) * (z(k) - H * X_prd(k));
        this._vecX_est = Vector3.add(vecX_prd, Vector3.scale(current - Vector3.multiplyVector3(this._vecH, vecX_prd), vecGain));

        // Update the error covariance
        //  P_est(k) = P_prd(k) - K(k) * H * P_prd(k);
        this._matP_est = Matrix3.subtract(matP_prd, Matrix3.scale(Vector3.multiplyVector3(vecGain, this._vecH), matP_prd));

        // Compute the estimated measurement.
        //  y = H * X_est(k);
        return Vector3.multiplyVector3(this._vecH,  this._vecX_est);
    }
}

function IdentityEstimator() {}
IdentityEstimator.prototype =
{
    estimate: function(current)
    {
        return current;
    }
};
