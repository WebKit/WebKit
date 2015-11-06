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

function PIDController(K, ysp, ulow, uhigh)
{
    this._ysp = ysp;

    this._Td = 5;
    this._Ti = 15;

    this._Kp = K;
    this._Kd = K / this._Td;
    this._Ki = K / this._Ti;

    this._eold = 0;
    this._I = 0;
}

PIDController.prototype =
{
    _sat: function(v, low, high)
    {
        return v < low ? low : (v > high ? high : v);
    },
    
    tune: function(y, h)
    {
        // Current error.
        var e = this._ysp - y;

        // Proportional term.
        var P = this._Kp * e;
        
        // Derivative term is the slope of the curve at the current time.
        var D = this._Kd * (e - this._eold) / h;

        // Integral term is the area under the curve starting from the begining till the current time.
        this._I += this._Ki * ((e + this._eold) / 2) * h;

        // pid controller value.
        var v = P + D + this._I;
        
        // Avoid spikes by applying actuator saturation.
        var u = this._sat(v, this._ulow, this._uhigh);        

        this._eold = e;
        return u;
    }
}

function KalmanEstimator(initX)
{
    // Initialize state transition matrix
    this._matA = Matrix3.identity();
    this._matA[Matrix3.pos(0, 2)] = 1;
    
    // Initialize measurement matrix
    this._vecH = Vector3.zeros();
    this._vecH[0] = 1;
    
    this._matQ = Matrix3.identity();
    this._R = 1000;
    
    // Initial state conditions
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
