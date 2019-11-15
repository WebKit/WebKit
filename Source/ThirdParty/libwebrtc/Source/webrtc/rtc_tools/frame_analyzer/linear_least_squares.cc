/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/linear_least_squares.h"

#include <math.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

template <class T>
using Matrix = std::valarray<std::valarray<T>>;

namespace {

template <typename R, typename T>
R DotProduct(const std::valarray<T>& a, const std::valarray<T>& b) {
  RTC_CHECK_EQ(a.size(), b.size());
  return std::inner_product(std::begin(a), std::end(a), std::begin(b), R(0));
}

// Calculates a^T * b.
template <typename R, typename T>
Matrix<R> MatrixMultiply(const Matrix<T>& a, const Matrix<T>& b) {
  Matrix<R> result(std::valarray<R>(a.size()), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    for (size_t j = 0; j < b.size(); ++j)
      result[j][i] = DotProduct<R>(a[i], b[j]);
  }

  return result;
}

template <typename T>
Matrix<T> Transpose(const Matrix<T>& matrix) {
  if (matrix.size() == 0)
    return Matrix<T>();
  const size_t rows = matrix.size();
  const size_t columns = matrix[0].size();
  Matrix<T> result(std::valarray<T>(rows), columns);

  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < columns; ++j)
      result[j][i] = matrix[i][j];
  }

  return result;
}

// Convert valarray from type T to type R.
template <typename R, typename T>
std::valarray<R> ConvertTo(const std::valarray<T>& v) {
  std::valarray<R> result(v.size());
  for (size_t i = 0; i < v.size(); ++i)
    result[i] = static_cast<R>(v[i]);
  return result;
}

// Convert valarray Matrix from type T to type R.
template <typename R, typename T>
Matrix<R> ConvertTo(const Matrix<T>& mat) {
  Matrix<R> result(mat.size());
  for (size_t i = 0; i < mat.size(); ++i)
    result[i] = ConvertTo<R>(mat[i]);
  return result;
}

// Convert from valarray Matrix back to the more conventional std::vector.
template <typename T>
std::vector<std::vector<T>> ToVectorMatrix(const Matrix<T>& m) {
  std::vector<std::vector<T>> result;
  for (const std::valarray<T>& v : m)
    result.emplace_back(std::begin(v), std::end(v));
  return result;
}

// Create a valarray Matrix from a conventional std::vector.
template <typename T>
Matrix<T> FromVectorMatrix(const std::vector<std::vector<T>>& mat) {
  Matrix<T> result(mat.size());
  for (size_t i = 0; i < mat.size(); ++i)
    result[i] = std::valarray<T>(mat[i].data(), mat[i].size());
  return result;
}

// Returns |matrix_to_invert|^-1 * |right_hand_matrix|. |matrix_to_invert| must
// have square size.
Matrix<double> GaussianElimination(Matrix<double> matrix_to_invert,
                                   Matrix<double> right_hand_matrix) {
  // |n| is the width/height of |matrix_to_invert|.
  const size_t n = matrix_to_invert.size();
  // Make sure |matrix_to_invert| has square size.
  for (const std::valarray<double>& column : matrix_to_invert)
    RTC_CHECK_EQ(n, column.size());
  // Make sure |right_hand_matrix| has correct size.
  for (const std::valarray<double>& column : right_hand_matrix)
    RTC_CHECK_EQ(n, column.size());

  // Transpose the matrices before and after so that we can perform Gaussian
  // elimination on the columns instead of the rows, since that is easier with
  // our representation.
  matrix_to_invert = Transpose(matrix_to_invert);
  right_hand_matrix = Transpose(right_hand_matrix);

  // Loop over the diagonal of |matrix_to_invert| and perform column reduction.
  // Column reduction is a sequence of elementary column operations that is
  // performed on both |matrix_to_invert| and |right_hand_matrix| until
  // |matrix_to_invert| has been transformed to the identity matrix.
  for (size_t diagonal_index = 0; diagonal_index < n; ++diagonal_index) {
    // Make sure the diagonal element has the highest absolute value by
    // swapping columns if necessary.
    for (size_t column = diagonal_index + 1; column < n; ++column) {
      if (std::abs(matrix_to_invert[column][diagonal_index]) >
          std::abs(matrix_to_invert[diagonal_index][diagonal_index])) {
        std::swap(matrix_to_invert[column], matrix_to_invert[diagonal_index]);
        std::swap(right_hand_matrix[column], right_hand_matrix[diagonal_index]);
      }
    }

    // Reduce the diagonal element to be 1, by dividing the column with that
    // value. If the diagonal element is 0, it means the system of equations has
    // many solutions, and in that case we will return an arbitrary solution.
    if (matrix_to_invert[diagonal_index][diagonal_index] == 0.0) {
      RTC_LOG(LS_WARNING) << "Matrix is not invertible, ignoring.";
      continue;
    }
    const double diagonal_element =
        matrix_to_invert[diagonal_index][diagonal_index];
    matrix_to_invert[diagonal_index] /= diagonal_element;
    right_hand_matrix[diagonal_index] /= diagonal_element;

    // Eliminate the other entries in row |diagonal_index| by making them zero.
    for (size_t column = 0; column < n; ++column) {
      if (column == diagonal_index)
        continue;
      const double row_element = matrix_to_invert[column][diagonal_index];
      matrix_to_invert[column] -=
          row_element * matrix_to_invert[diagonal_index];
      right_hand_matrix[column] -=
          row_element * right_hand_matrix[diagonal_index];
    }
  }

  // Transpose the result before returning it, explained in comment above.
  return Transpose(right_hand_matrix);
}

}  // namespace

IncrementalLinearLeastSquares::IncrementalLinearLeastSquares() = default;
IncrementalLinearLeastSquares::~IncrementalLinearLeastSquares() = default;

void IncrementalLinearLeastSquares::AddObservations(
    const std::vector<std::vector<uint8_t>>& x,
    const std::vector<std::vector<uint8_t>>& y) {
  if (x.empty() || y.empty())
    return;
  // Make sure all columns are the same size.
  const size_t n = x[0].size();
  for (const std::vector<uint8_t>& column : x)
    RTC_CHECK_EQ(n, column.size());
  for (const std::vector<uint8_t>& column : y)
    RTC_CHECK_EQ(n, column.size());

  // We will multiply the uint8_t values together, so we need to expand to a
  // type that can safely store those values, i.e. uint16_t.
  const Matrix<uint16_t> unpacked_x = ConvertTo<uint16_t>(FromVectorMatrix(x));
  const Matrix<uint16_t> unpacked_y = ConvertTo<uint16_t>(FromVectorMatrix(y));

  const Matrix<uint64_t> xx = MatrixMultiply<uint64_t>(unpacked_x, unpacked_x);
  const Matrix<uint64_t> xy = MatrixMultiply<uint64_t>(unpacked_x, unpacked_y);
  if (sum_xx && sum_xy) {
    *sum_xx += xx;
    *sum_xy += xy;
  } else {
    sum_xx = xx;
    sum_xy = xy;
  }
}

std::vector<std::vector<double>>
IncrementalLinearLeastSquares::GetBestSolution() const {
  RTC_CHECK(sum_xx && sum_xy) << "No observations have been added";
  return ToVectorMatrix(GaussianElimination(ConvertTo<double>(*sum_xx),
                                            ConvertTo<double>(*sum_xy)));
}

}  // namespace test
}  // namespace webrtc
