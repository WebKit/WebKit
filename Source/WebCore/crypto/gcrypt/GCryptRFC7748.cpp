/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GCryptRFC7748.h"

#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {
namespace GCrypt {
namespace RFC7748 {

struct X25519Impl {
    static constexpr char s_curveName[] = "Curve25519";
    static constexpr size_t argSize = 32;
};

struct X448Impl {
    static constexpr char s_curveName[] = "X448";
    static constexpr size_t argSize = 56;
};

template<typename ImplType>
std::optional<Vector<uint8_t>> xImpl(const std::span<const uint8_t>& kArg, const std::span<const uint8_t>& uArg)
{
    if (kArg.size() != ImplType::argSize || uArg.size() != ImplType::argSize)
        return std::nullopt;

    gcry_error_t error = GPG_ERR_NO_ERROR;

    PAL::GCrypt::Handle<gcry_ctx_t> context;
    error = gcry_mpi_ec_new(&context, nullptr, ImplType::s_curveName);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    PAL::GCrypt::Handle<gcry_mpi_t> kMPI;
    {
        std::array<uint8_t, ImplType::argSize> k;
        std::copy(kArg.rbegin(), kArg.rend(), k.begin());

        error = gcry_mpi_scan(&kMPI, GCRYMPI_FMT_USG, k.data(), k.size(), nullptr);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }

        // For X25519:
        //   - three least-significant bits of the first byte are set to zero,
        //   - second most-significant bit of the last byte is set to 1,
        //   - most-significant bit of the of the last byte is set to zero.
        if constexpr (std::is_same_v<ImplType, X25519Impl>) {
            gcry_mpi_clear_bit(kMPI, 0);
            gcry_mpi_clear_bit(kMPI, 1);
            gcry_mpi_clear_bit(kMPI, 2);
            gcry_mpi_set_bit(kMPI, 254);
            gcry_mpi_clear_bit(kMPI, 255);
        }

        // For X448:
        //   - two least-significant bits of the first byte are set to zero,
        //   - most-significant bit of the last byte is set to zero.
        if constexpr (std::is_same_v<ImplType, X448Impl>) {
            gcry_mpi_clear_bit(kMPI, 0);
            gcry_mpi_clear_bit(kMPI, 1);
            gcry_mpi_set_bit(kMPI, 447);
        }
    }

    PAL::GCrypt::Handle<gcry_mpi_t> uMPI;
    {
        std::array<uint8_t, ImplType::argSize> u;
        std::copy(uArg.rbegin(), uArg.rend(), u.begin());

        error = gcry_mpi_scan(&uMPI, GCRYMPI_FMT_USG, u.data(), u.size(), nullptr);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }

        // For X25519, the most significant bit in the last byte of u-coordinate must be cleared.
        if constexpr (std::is_same_v<ImplType, X25519Impl>)
            gcry_mpi_clear_bit(uMPI, 255);
    }

    Vector<uint8_t> result(ImplType::argSize, uint8_t(0));
    {
        // Perform the multiplication on the given curve. The result is retrieved back into kMPI.
        PAL::GCrypt::Handle<gcry_mpi_point_t> Q(gcry_mpi_point_new(0));
        PAL::GCrypt::Handle<gcry_mpi_point_t> P(gcry_mpi_point_set(nullptr, uMPI, nullptr, GCRYMPI_CONST_ONE));

        gcry_mpi_ec_mul(Q, kMPI, P, context);
        int ret = gcry_mpi_ec_get_affine(kMPI, nullptr, Q, context);

        // Store the resulting MPI data into a separate allocation. In case of an infinite point,
        // a zero Vector is established. Otherwise, the MPI data is retrieved into a properly-sized Vector.
        Vector<uint8_t> kData;
        if (!ret) {
            size_t numBytes = 0;
            error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &numBytes, kMPI);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }

            kData = Vector<uint8_t>(numBytes, uint8_t(0));
            error = gcry_mpi_print(GCRYMPI_FMT_USG, kData.data(), kData.size(), nullptr, kMPI);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return std::nullopt;
            }
        } else
            kData = Vector<uint8_t>(ImplType::argSize, uint8_t(0));

        // Up to curve-specific amount of bytes of MPI data is copied in reverse order
        // into the initially-zeroed result Vector.
        size_t kSize = std::min<size_t>(kData.size(), ImplType::argSize);
        std::copy(kData.rbegin(), std::next(kData.rbegin(), kSize), result.begin());
    }

    return result;
}

std::optional<Vector<uint8_t>> X25519(const std::span<const uint8_t>& kArg, const std::span<const uint8_t>& uArg)
{
    return xImpl<X25519Impl>(kArg, uArg);
}

} } } // namespace WebCore::GCrypt::RFC7748
