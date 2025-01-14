/*
    Copyright (C) 2019-Present SKALE Labs

    This file is part of sgxwallet.

    sgxwallet is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sgxwallet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with sgxwallet. If not, see <https://www.gnu.org/licenses/>.

    @file DKGUtils.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SGXWALLET_DKGUTILS_H
#define SGXWALLET_DKGUTILS_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#ifdef USER_SPACE

#include <gmp.h>
#else
#include <../tgmp-build/include/sgx_tgmp.h>
#endif

EXTERNC int gen_dkg_poly( char* secret, unsigned _t);

EXTERNC void calc_secret_shares(const char* decrypted_coeffs, char * secret_shares,
                        unsigned _t, unsigned _n);

EXTERNC int calc_secret_share(const char* decrypted_coeffs, char * s_share,
                               unsigned _t, unsigned _n, unsigned ind);

EXTERNC int calc_public_shares(const char* decrypted_coeffs, char * public_shares, unsigned _t);

EXTERNC int Verification ( char * public_shares, mpz_t decr_secret_share, int _t, int ind);

EXTERNC int calc_bls_public_key(char* skey, char* pub_key);

EXTERNC int calc_secret_shareG2(const char* s_share, char * s_shareG2);
#endif

