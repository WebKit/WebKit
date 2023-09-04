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

namespace WebCore {

struct X25519Impl {
    static constexpr char s_curveName[] = "Curve25519";
    static constexpr size_t argSize = 32;
};

struct X448Impl {
    static constexpr char s_curveName[] = "X448";
    static constexpr size_t argSize = 56;
};

template<typename ImplType>
std::optional<Vector<uint8_t>> xImpl(const Vector<uint8_t>& kArg, const Vector<uint8_t>& uArg)

{
    if (!(kArg.size() == ImplType::argSize) && uArg.size() == ImplType::argSize)
        return std::nullopt;

    PAL::GCrypt::Handle<gcry_ctx_t> context;
    if (auto error = gcry_mpi_ec_new(&context, nullptr, ImplType::s_curveName)) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    Vector<uint8_t> k = kArg;
    std::reverse(k.begin(), k.end());
    Vector<uint8_t> u = uArg;
    std::reverse(u.begin(), u.end());

    PAL::GCrypt::Handle<gcry_mpi_t> kMPI;
    gcry_mpi_scan(&kMPI, GCRYMPI_FMT_USG, k.data(), k.size(), nullptr);
    PAL::GCrypt::Handle<gcry_mpi_t> uMPI;
    gcry_mpi_scan(&uMPI, GCRYMPI_FMT_USG, u.data(), u.size(), nullptr);

    if constexpr (std::is_same_v<ImplType, X25519Impl>)
        gcry_mpi_clear_bit(uMPI, 255);

    PAL::GCrypt::Handle<gcry_mpi_point_t> Q(gcry_mpi_point_new(0));
    PAL::GCrypt::Handle<gcry_mpi_point_t> P(gcry_mpi_point_set(nullptr, uMPI, nullptr, GCRYMPI_CONST_ONE));

    Vector<uint8_t> result;
    {
        if constexpr (std::is_same_v<ImplType, X25519Impl>) {
            gcry_mpi_clear_bit(kMPI, 0);
            gcry_mpi_clear_bit(kMPI, 1);
            gcry_mpi_clear_bit(kMPI, 2);
            gcry_mpi_set_bit(kMPI, 254);
            gcry_mpi_clear_bit(kMPI, 255);
        }

        if constexpr (std::is_same_v<ImplType, X448Impl>) {
            gcry_mpi_clear_bit(kMPI, 0);
            gcry_mpi_clear_bit(kMPI, 1);
            gcry_mpi_set_bit(kMPI, 447);
        }

        gcry_mpi_ec_mul(Q, kMPI, P, context);
        int ret = gcry_mpi_ec_get_affine(kMPI, nullptr, Q, context);
        if (!ret) {
            size_t numBytes = 0;
            gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &numBytes, kMPI);
            result = Vector<uint8_t>(numBytes, uint8_t(0));
            gcry_mpi_print(GCRYMPI_FMT_USG, result.data(), result.size(), nullptr, kMPI);
        } else
            result = Vector<uint8_t>(ImplType::argSize, uint8_t(0));
    }

    std::reverse(result.begin(), result.end());
    if (result.size() > ImplType::argSize)
        result.resize(ImplType::argSize);

    while (result.size() < ImplType::argSize)
        result.append(0x00);

    return result;
}

std::optional<Vector<uint8_t>> X25519(const Vector<uint8_t>& kArg, const Vector<uint8_t>& uArg)
{
    return xImpl<X25519Impl>(kArg, uArg);
}


} // namespace WebCore
