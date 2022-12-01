#!/usr/bin/env python3
##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
"""Generate the probability model for the constrained token set.

Model obtained from a 2-sided zero-centered distribution derived
from a Pareto distribution. The cdf of the distribution is:
cdf(x) = 0.5 + 0.5 * sgn(x) * [1 - {alpha/(alpha + |x|)} ^ beta]

For a given beta and a given probability of the 1-node, the alpha
is first solved, and then the {alpha, beta} pair is used to generate
the probabilities for the rest of the nodes.
"""

import heapq
import sys
import numpy as np
import scipy.optimize
import scipy.stats


def cdf_spareto(x, xm, beta):
  p = 1 - (xm / (np.abs(x) + xm))**beta
  p = 0.5 + 0.5 * np.sign(x) * p
  return p


def get_spareto(p, beta):
  cdf = cdf_spareto

  def func(x):
    return ((cdf(1.5, x, beta) - cdf(0.5, x, beta)) /
            (1 - cdf(0.5, x, beta)) - p)**2

  alpha = scipy.optimize.fminbound(func, 1e-12, 10000, xtol=1e-12)
  parray = np.zeros(11)
  parray[0] = 2 * (cdf(0.5, alpha, beta) - 0.5)
  parray[1] = (2 * (cdf(1.5, alpha, beta) - cdf(0.5, alpha, beta)))
  parray[2] = (2 * (cdf(2.5, alpha, beta) - cdf(1.5, alpha, beta)))
  parray[3] = (2 * (cdf(3.5, alpha, beta) - cdf(2.5, alpha, beta)))
  parray[4] = (2 * (cdf(4.5, alpha, beta) - cdf(3.5, alpha, beta)))
  parray[5] = (2 * (cdf(6.5, alpha, beta) - cdf(4.5, alpha, beta)))
  parray[6] = (2 * (cdf(10.5, alpha, beta) - cdf(6.5, alpha, beta)))
  parray[7] = (2 * (cdf(18.5, alpha, beta) - cdf(10.5, alpha, beta)))
  parray[8] = (2 * (cdf(34.5, alpha, beta) - cdf(18.5, alpha, beta)))
  parray[9] = (2 * (cdf(66.5, alpha, beta) - cdf(34.5, alpha, beta)))
  parray[10] = 2 * (1. - cdf(66.5, alpha, beta))
  return parray


def quantize_probs(p, save_first_bin, bits):
  """Quantize probability precisely.

  Quantize probabilities minimizing dH (Kullback-Leibler divergence)
  approximated by: sum (p_i-q_i)^2/p_i.
  References:
  https://en.wikipedia.org/wiki/Kullback%E2%80%93Leibler_divergence
  https://github.com/JarekDuda/AsymmetricNumeralSystemsToolkit
  """
  num_sym = p.size
  p = np.clip(p, 1e-16, 1)
  L = 2**bits
  pL = p * L
  ip = 1. / p  # inverse probability
  q = np.clip(np.round(pL), 1, L + 1 - num_sym)
  quant_err = (pL - q)**2 * ip
  sgn = np.sign(L - q.sum())  # direction of correction
  if sgn != 0:  # correction is needed
    v = []  # heap of adjustment results (adjustment err, index) of each symbol
    for i in range(1 if save_first_bin else 0, num_sym):
      q_adj = q[i] + sgn
      if q_adj > 0 and q_adj < L:
        adj_err = (pL[i] - q_adj)**2 * ip[i] - quant_err[i]
        heapq.heappush(v, (adj_err, i))
    while q.sum() != L:
      # apply lowest error adjustment
      (adj_err, i) = heapq.heappop(v)
      quant_err[i] += adj_err
      q[i] += sgn
      # calculate the cost of adjusting this symbol again
      q_adj = q[i] + sgn
      if q_adj > 0 and q_adj < L:
        adj_err = (pL[i] - q_adj)**2 * ip[i] - quant_err[i]
        heapq.heappush(v, (adj_err, i))
  return q


def get_quantized_spareto(p, beta, bits, first_token):
  parray = get_spareto(p, beta)
  parray = parray[1:] / (1 - parray[0])
  # CONFIG_NEW_TOKENSET
  if first_token > 1:
    parray = parray[1:] / (1 - parray[0])
  qarray = quantize_probs(parray, first_token == 1, bits)
  return qarray.astype(np.int)


def main(bits=15, first_token=1):
  beta = 8
  for q in range(1, 256):
    parray = get_quantized_spareto(q / 256., beta, bits, first_token)
    assert parray.sum() == 2**bits
    print('{', ', '.join('%d' % i for i in parray), '},')


if __name__ == '__main__':
  if len(sys.argv) > 2:
    main(int(sys.argv[1]), int(sys.argv[2]))
  elif len(sys.argv) > 1:
    main(int(sys.argv[1]))
  else:
    main()
