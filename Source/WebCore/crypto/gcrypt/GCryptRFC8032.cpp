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
#include "GCryptRFC8032.h"

namespace WebCore {

bool validateEd25519KeyPair(const Vector<uint8_t>& privateKey, const Vector<uint8_t>& publicKey)
{
    if (privateKey.size() != 32 || publicKey.size() != 32)
        return false;

    gcry_error_t error = GPG_ERR_NO_ERROR;

    std::array<uint8_t, 32> hddata;
    {
        std::array<uint8_t, 32> ddata;
        memcpy(ddata.data(), privateKey.data(), 32);

        std::array<uint8_t, 64> digest;
        gcry_md_hash_buffer(GCRY_MD_SHA512, digest.data(), ddata.data(), ddata.size());

        memcpy(hddata.data(), digest.data(), 32);
        std::reverse(hddata.begin(), hddata.end());
    }

    PAL::GCrypt::Handle<gcry_mpi_t> hdmpi;
    {
        error = gcry_mpi_scan(&hdmpi, GCRYMPI_FMT_USG, hddata.data(), hddata.size(), nullptr);
        if (error != GPG_ERR_NO_ERROR)
            return false;

        gcry_mpi_clear_bit(hdmpi, 0);
        gcry_mpi_clear_bit(hdmpi, 1);
        gcry_mpi_clear_bit(hdmpi, 2);
        gcry_mpi_set_bit(hdmpi, 254);
        gcry_mpi_clear_bit(hdmpi, 255);
    }

    PAL::GCrypt::Handle<gcry_ctx_t> context;
    error = gcry_mpi_ec_new(&context, nullptr, "Ed25519");
    if (error != GPG_ERR_NO_ERROR)
        return false;

    std::array<uint8_t, 32> qdata;
    {
        PAL::GCrypt::Handle<gcry_mpi_point_t> G(gcry_mpi_ec_get_point("g", context, 1));
        PAL::GCrypt::Handle<gcry_mpi_point_t> Q(gcry_mpi_point_new(0));

        gcry_mpi_ec_mul(Q, hdmpi, G, context);
        PAL::GCrypt::Handle<gcry_mpi_t> xmpi(gcry_mpi_new(0));
        PAL::GCrypt::Handle<gcry_mpi_t> ympi(gcry_mpi_new(0));
        int ret = gcry_mpi_ec_get_affine(xmpi, ympi, Q, context);

        Vector<uint8_t> result;
        if (!ret) {
            if (gcry_mpi_test_bit(xmpi, 0))
                gcry_mpi_set_bit(ympi, 255);
            else
                gcry_mpi_clear_bit(ympi, 255);

            size_t ylength = 0;
            error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &ylength, ympi);
            if (error != GPG_ERR_NO_ERROR)
                return false;

            result = Vector<uint8_t>(ylength, uint8_t(0));
            error = gcry_mpi_print(GCRYMPI_FMT_USG, result.data(), result.size(), nullptr, ympi);
            if (error != GPG_ERR_NO_ERROR)
                return false;
        } else
            result = Vector<uint8_t>(32, uint8_t(0));

        std::reverse(result.begin(), result.end());
        if (result.size() > 32)
            result.resize(32);
        while (result.size() < 32)
            result.append(0x00);
        ASSERT(result.size() == 32);

        memcpy(qdata.data(), result.data(), 32);
    }

    return !memcmp(qdata.begin(), publicKey.data(), 32);
}

} // namespace WebCore

