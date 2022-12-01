import numpy as np

# Model A only.
# Uses least squares regression to find the solution
# when there is one unknown variable.
def lstsq_solution(A, B):
    A_inv = np.linalg.pinv(A)
    x = np.matmul(A_inv, B)
    return x[0][0]

# Model B only.
# Uses the pseudoinverse matrix to find the solution
# when there are two unknown variables.
def pinv_solution(A, mv, B):
    new_A = np.concatenate((A, mv), axis=1)
    new_A_inv = np.linalg.pinv(new_A)
    new_x = np.matmul(new_A_inv, B)
    print("pinv solution:", new_x[0][0], new_x[1][0])
    return (new_x[0][0], new_x[1][0])

# Model A only.
# Finds the coefficient to multiply A by to minimize
# the percentage error between A and B.
def minimize_percentage_error_model_a(A, B):
    R = np.divide(A, B)
    num = 0
    den = 0
    best_x = 0
    best_error = 100
    for r_i in R:
        num += r_i
        den += r_i**2
    if den == 0:
        return 0
    return (num/den)[0]

# Model B only.
# Finds the coefficients to multiply to the frame bitrate
# and the motion vector bitrate to minimize the percent error.
def minimize_percentage_error_model_b(r_e, r_m, r_f):
    r_ef = np.divide(r_e, r_f)
    r_mf = np.divide(r_m, r_f)
    sum_ef = np.sum(r_ef)
    sum_ef_sq = np.sum(np.square(r_ef))
    sum_mf = np.sum(r_mf)
    sum_mf_sq = np.sum(np.square(r_mf))
    sum_ef_mf = np.sum(np.multiply(r_ef, r_mf))
    # Divides x by y. If y is zero, returns 0.
    divide = lambda x, y : 0 if y == 0 else x / y
    # Set up and solve the matrix equation
    A = np.array([[1, divide(sum_ef_mf, sum_ef_sq)],[divide(sum_ef_mf, sum_mf_sq), 1]])
    B = np.array([divide(sum_ef, sum_ef_sq), divide(sum_mf, sum_mf_sq)])
    A_inv = np.linalg.pinv(A)
    x = np.matmul(A_inv, B)
    return x

# Model A only.
# Calculates the least squares error between A and B
# using coefficients in X.
def average_lstsq_error(A, B, x):
    error = 0
    n = 0
    for i, a in enumerate(A):
        a = a[0]
        b = B[i][0]
        if b == 0:
            continue
        n += 1
        error += (b - x*a)**2
    if n == 0:
        return None
    error /= n
    return error

# Model A only.
# Calculates the average percentage error between A and B.
def average_percent_error_model_a(A, B, x):
    error = 0
    n = 0
    for i, a in enumerate(A):
        a = a[0]
        b = B[i][0]
        if b == 0:
            continue
        n += 1
        error_i = (abs(x*a-b)/b)*100
        error += error_i
    error /= n
    return error

# Model B only.
# Calculates the average percentage error between A and B.
def average_percent_error_model_b(A, M, B, x):
    error = 0
    for i, a in enumerate(A):
        a = a[0]
        mv = M[i]
        b = B[i][0]
        if b == 0:
            continue
        estimate = x[0]*a
        estimate += x[1]*mv
        error += abs(estimate - b) / b
    error *= 100
    error /= A.shape[0]
    return error

def average_squared_error_model_a(A, B, x):
    error = 0
    n = 0
    for i, a in enumerate(A):
        a = a[0]
        b = B[i][0]
        if b == 0:
            continue
        n += 1
        error_i = (1 - x*(a/b))**2
        error += error_i
    error /= n
    error = error**0.5
    return error * 100

def average_squared_error_model_b(A, M, B, x):
    error = 0
    n = 0
    for i, a in enumerate(A):
        a = a[0]
        b = B[i][0]
        mv = M[i]
        if b == 0:
            continue
        n += 1
        error_i = 1 - ((x[0]*a + x[1]*mv)/b)
        error_i = error_i**2
        error += error_i
    error /= n
    error = error**0.5
    return error * 100

# Traverses the data and prints out one value for
# each update type.
def print_solutions(file_path):
    data = np.genfromtxt(file_path, delimiter="\t")
    prev_update = 0
    split_list_indices = list()
    for i, val in enumerate(data):
        if prev_update != val[3]:
            split_list_indices.append(i)
            prev_update = val[3]
    split = np.split(data, split_list_indices)
    for array in split:
        A, mv, B, update = np.hsplit(array, 4)
        z = np.where(B == 0)[0]
        r_e = np.delete(A, z, axis=0)
        r_m = np.delete(mv, z, axis=0)
        r_f = np.delete(B, z, axis=0)
        A = r_e
        mv = r_m
        B = r_f
        all_zeros = not A.any()
        if all_zeros:
            continue
        print("update type:", update[0][0])
        x_ls = lstsq_solution(A, B)
        x_a = minimize_percentage_error_model_a(A, B)
        x_b = minimize_percentage_error_model_b(A, mv, B)
        percent_error_a = average_percent_error_model_a(A, B, x_a)
        percent_error_b = average_percent_error_model_b(A, mv, B, x_b)[0]
        baseline_percent_error_a = average_percent_error_model_a(A, B, 1)
        baseline_percent_error_b = average_percent_error_model_b(A, mv, B, [1, 1])[0]

        squared_error_a = average_squared_error_model_a(A, B, x_a)
        squared_error_b = average_squared_error_model_b(A, mv, B, x_b)[0]
        baseline_squared_error_a = average_squared_error_model_a(A, B, 1)
        baseline_squared_error_b = average_squared_error_model_b(A, mv, B, [1, 1])[0]

        print("model,\tframe_coeff,\tmv_coeff,\terror,\tbaseline_error")
        print("Model A %_error,\t" + str(x_a) + ",\t" + str(0) + ",\t" + str(percent_error_a) + ",\t" + str(baseline_percent_error_a))
        print("Model A sq_error,\t" + str(x_a) + ",\t" + str(0) + ",\t" + str(squared_error_a) + ",\t" + str(baseline_squared_error_a))
        print("Model B %_error,\t" + str(x_b[0]) + ",\t" + str(x_b[1]) + ",\t" + str(percent_error_b) + ",\t" + str(baseline_percent_error_b))
        print("Model B sq_error,\t" + str(x_b[0]) + ",\t" + str(x_b[1]) + ",\t" + str(squared_error_b) + ",\t" + str(baseline_squared_error_b))
        print()

if __name__ == "__main__":
    print_solutions("data2/all_lowres_target_lt600_data.txt")
