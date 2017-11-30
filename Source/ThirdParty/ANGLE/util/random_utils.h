//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// random_utils:
//   Helper functions for random number generation.
//

#ifndef UTIL_RANDOM_UTILS_H
#define UTIL_RANDOM_UTILS_H

// TODO(jmadill): Rework this if Chromium decides to ban <random>
#include <random>

#include <export.h>

namespace angle
{

class ANGLE_EXPORT RNG
{
  public:
    // Seed from clock
    RNG();
    // Seed from fixed number.
    RNG(unsigned int seed);
    ~RNG();

    void reseed(unsigned int newSeed);

    int randomInt();
    int randomIntBetween(int min, int max);
    unsigned int randomUInt();
    float randomFloat();
    float randomFloatBetween(float min, float max);
    float randomNegativeOneToOne();

  private:
    std::default_random_engine mGenerator;
};

// Implemented htis way because of cross-module allocation issues.
inline void FillVectorWithRandomUBytes(std::vector<uint8_t> *data)
{
    RNG rng;
    for (size_t i = 0; i < data->size(); ++i)
    {
        (*data)[i] = static_cast<uint8_t>(rng.randomIntBetween(0, 255));
    }
}

}  // namespace angle

#endif // UTIL_RANDOM_UTILS_H
