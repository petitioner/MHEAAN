/*
* Copyright (c) by CryptoLab inc.
* This program is licensed under a
* Creative Commons Attribution-NonCommercial 3.0 Unported License.
* You should have received a copy of the license along with this
* work.  If not, see <http://creativecommons.org/licenses/by-nc/3.0/>.
*/

#include "RingMultiplier.h"

#include <NTL/lip.h>
#include <NTL/sp_arith.h>
#include <NTL/tools.h>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "Primes.h"

RingMultiplier::RingMultiplier(long logN0, long nprimes) : logN0(logN0) {
	N0 = 1 << logN0;

	logN1 = 8;
	N1 = 1 << logN1;

	logN = logN0 + logN1;
	N = 1 << logN;

	long M0 = 1 << (logN0 + 1);

	pVec = new uint64_t[nprimes];
	prVec = new uint64_t[nprimes];
	pTwok = new long[nprimes];
	pInvVec = new uint64_t[nprimes];

	scaledRootM0Pows = new uint64_t*[nprimes];
	scaledRootM0PowsInv = new uint64_t*[nprimes];

	scaledRootN1Pows = new uint64_t*[nprimes];
	scaledRootN1PowsInv = new uint64_t*[nprimes];

	scaledN0Inv = new uint64_t[nprimes];
	scaledN1Inv = new uint64_t[nprimes];

	rootM1DFTPows = new uint64_t*[nprimes]();
	rootM1DFTPowsInv = new uint64_t*[nprimes]();
	rootM1Pows = new uint64_t*[nprimes]();
	rootM1PowsInv = new uint64_t*[nprimes]();

	red_ss_array = new _ntl_general_rem_one_struct*[nprimes];

	long M1 = N1 + 1;
	gM1Pows = new long[M1];
	gM1PowsInv = new long[M1];
	long g1 = 3;
	long gM1Pow = 1;
	for (long i = 0; i < N1; ++i) {
		gM1Pows[i] = gM1Pow;
		gM1PowsInv[gM1Pow] = i;
		gM1Pow *= g1;
		gM1Pow %= M1;
	}
	gM1Pows[N1] = 1;

	for (long i = 0; i < nprimes; ++i) {
		pVec[i] = pPrimesVec[i];
		red_ss_array[i] = _ntl_general_rem_one_struct_build(pVec[i]);
		pInvVec[i] = inv(pVec[i]);
		pTwok[i] = (2 * ((long) log2(pVec[i]) + 1));
		prVec[i] = (static_cast<unsigned __int128>(1) << pTwok[i]) / pVec[i];

		uint64_t NxInvModp = powMod(N0, pVec[i] - 2, pVec[i]);
		mulMod(scaledN0Inv[i], NxInvModp, (1ULL << 32), pVec[i]);
		mulMod(scaledN0Inv[i], scaledN0Inv[i],(1ULL << 32), pVec[i]);

		uint64_t NyInvModp = powMod(N1, pVec[i] - 2, pVec[i]);
		mulMod(scaledN1Inv[i], NyInvModp, (1ULL << 32), pVec[i]);
		mulMod(scaledN1Inv[i], scaledN1Inv[i],(1ULL << 32), pVec[i]);


		uint64_t rootM0 = findMthRootOfUnity(M0, pVec[i]);
		uint64_t rootM0inv = powMod(rootM0, pVec[i] - 2, pVec[i]);
		scaledRootM0Pows[i] = new uint64_t[N0]();
		scaledRootM0PowsInv[i] = new uint64_t[N0]();
		uint64_t power = 1;
		uint64_t powerInv = 1;
		for (long j = 0; j < N0; ++j) {
			uint32_t jprime = bitReverse(static_cast<uint32_t>(j)) >> (32 - logN0);
			uint64_t rootpow = power;
			mulMod(scaledRootM0Pows[i][jprime], rootpow, (1ULL << 32), pVec[i]);
			mulMod(scaledRootM0Pows[i][jprime], scaledRootM0Pows[i][jprime], (1ULL << 32), pVec[i]);
			uint64_t rootpowinv = powerInv;
			mulMod(scaledRootM0PowsInv[i][jprime], rootpowinv, (1ULL << 32), pVec[i]);
			mulMod(scaledRootM0PowsInv[i][jprime], scaledRootM0PowsInv[i][jprime], (1ULL << 32), pVec[i]);
			mulMod(power, power, rootM0, pVec[i]);
			mulMod(powerInv, powerInv, rootM0inv, pVec[i]);
		}

		uint64_t rootN1 = findMthRootOfUnity(N1, pVec[i]);
		uint64_t rootN1inv = powMod(rootN1, pVec[i] - 2, pVec[i]);
		scaledRootN1Pows[i] = new uint64_t[N1]();
		scaledRootN1PowsInv[i] = new uint64_t[N1]();
		power = 1;
		powerInv = 1;
		for (long j = 0; j < N1; ++j) {
			uint64_t rootpow = power;
			mulMod(scaledRootN1Pows[i][j], rootpow, (1ULL << 32), pVec[i]);
			mulMod(scaledRootN1Pows[i][j], scaledRootN1Pows[i][j], (1ULL << 32), pVec[i]);
			uint64_t rootpowinv = powerInv;
			mulMod(scaledRootN1PowsInv[i][j], rootpowinv, (1ULL << 32), pVec[i]);
			mulMod(scaledRootN1PowsInv[i][j], scaledRootN1PowsInv[i][j], (1ULL << 32), pVec[i]);
			mulMod(power, power, rootN1, pVec[i]);
			mulMod(powerInv, powerInv, rootN1inv, pVec[i]);
		}

		uint64_t rootM1 = findMthRootOfUnity(M1, pVec[i]);
		rootM1DFTPows[i] = new uint64_t[N1]();
		rootM1DFTPowsInv[i] = new uint64_t[N1]();
		rootM1Pows[i] = new uint64_t[N1]();
		rootM1PowsInv[i] = new uint64_t[N1]();
		for (long j = 0; j < N1; ++j) {
			rootM1DFTPows[i][j] = powMod(rootM1, gM1Pows[j], pVec[i]);
			rootM1Pows[i][j] = powMod(rootM1, gM1Pows[j], pVec[i]);
			rootM1PowsInv[i][j] = powMod(rootM1, M1 - gM1Pows[j], pVec[i]);
		}

		NTTPO2X1(rootM1DFTPows[i], i);

		for (long j = 0; j < N1; ++j) {
			rootM1DFTPowsInv[i][j] = powMod(rootM1DFTPows[i][j], pVec[i] - 2, pVec[i]);
		}
	}

	coeffpinv_array = new mulmod_precon_t*[nprimes];
	pProd = new ZZ[nprimes];
	pProdh = new ZZ[nprimes];
	pHat = new ZZ*[nprimes];
	pHatInvModp = new uint64_t*[nprimes];
	for (long i = 0; i < nprimes; ++i) {
		pProd[i] = (i == 0) ? to_ZZ((long) pVec[i]) : pProd[i - 1] * (long) pVec[i];
		pProdh[i] = pProd[i] / 2;
		pHat[i] = new ZZ[i + 1];
		pHatInvModp[i] = new uint64_t[i + 1];
		coeffpinv_array[i] = new mulmod_precon_t[i + 1];
		for (long j = 0; j < i + 1; ++j) {
			pHat[i][j] = ZZ(1);
			for (long k = 0; k < j; ++k) {
				pHat[i][j] *= (long) pVec[k];
			}
			for (long k = j + 1; k < i + 1; ++k) {
				pHat[i][j] *= (long) pVec[k];
			}
			pHatInvModp[i][j] = to_long(pHat[i][j] % (long) pVec[j]);
			pHatInvModp[i][j] = powMod(pHatInvModp[i][j], pVec[j] - 2, pVec[j]);
			coeffpinv_array[i][j] = PrepMulModPrecon(pHatInvModp[i][j], pVec[j]);
		}
	}
}

void RingMultiplier::arrayBitReverse(uint64_t* vals, long n) {
	for (long i = 1, j = 0; i < n; ++i) {
		long bit = n >> 1;
		for (; j >= bit; bit>>=1) {
			j -= bit;
		}
		j += bit;
		if(i < j) {
			swap(vals[i], vals[j]);
		}
	}
}

void RingMultiplier::butt1(uint64_t& a1, uint64_t& a2, uint64_t& p, uint64_t& pInv, uint64_t& W) {
	uint64_t U = a1 + a2;
	if (U > p) U -= p;
	uint64_t T = a1 < a2 ? a1 + p - a2 : a1 - a2;
	unsigned __int128 UU = static_cast<unsigned __int128>(T) * W;
	uint64_t U0 = static_cast<uint64_t>(UU);
	uint64_t U1 = UU >> 64;
	uint64_t Q = U0 * pInv;
	unsigned __int128 Hx = static_cast<unsigned __int128>(Q) * p;
	uint64_t H = Hx >> 64;
	a1 = U;
	a2 = (U1 < H) ? U1 + p - H : U1 - H;
}

void RingMultiplier::butt2(uint64_t& a1, uint64_t& a2, uint64_t& p, uint64_t& pInv, uint64_t& W) {
	uint64_t T = a2;
	unsigned __int128 U = static_cast<unsigned __int128>(T) * W;
	uint64_t U0 = static_cast<uint64_t>(U);
	uint64_t U1 = U >> 64;
	uint64_t Q = U0 * pInv;
	unsigned __int128 Hx = static_cast<unsigned __int128>(Q) * p;
	uint64_t H = Hx >> 64;
	uint64_t V = U1 < H ? U1 + p - H : U1 - H;
	a2 = a1 < V ? a1 + p - V : a1 - V;
	a1 += V;
	if (a1 > p) a1 -= p;
}

void RingMultiplier::divByN(uint64_t& a, uint64_t& p, uint64_t& pInv, uint64_t& NScaleInv) {
	uint64_t T = a;
	unsigned __int128 U = static_cast<unsigned __int128>(T) * NScaleInv;
	uint64_t U0 = static_cast<uint64_t>(U);
	uint64_t U1 = U >> 64;
	uint64_t Q = U0 * pInv;
	unsigned __int128 Hx = static_cast<unsigned __int128>(Q) * p;
	uint64_t H = Hx >> 64;
	a = (U1 < H) ? U1 + p - H : U1 - H;
}

void RingMultiplier::NTTX0(uint64_t* a, long index) {
	long t = N0;
	long logt1 = logN0 + 1;
	uint64_t p = pVec[index];
	uint64_t pInv = pInvVec[index];
	uint64_t* scaledRootM0Powsi = scaledRootM0Pows[index];
	for (long m = 1; m < N0; m <<= 1) {
		t >>= 1;
		logt1 -= 1;
		for (long i = 0; i < m; i++) {
			long j1 = i << logt1;
			long j2 = j1 + t - 1;
			uint64_t W = scaledRootM0Powsi[m + i];
			for (long j = j1; j <= j2; j++) {
				butt2(a[j], a[j + t], p, pInv, W);
			}
		}
	}
}

void RingMultiplier::INTTX0(uint64_t* a, long index) {
	uint64_t p = pVec[index];
	uint64_t pInv = pInvVec[index];
	uint64_t* scaledRootM0PowsInvi = scaledRootM0PowsInv[index];
	long t = 1;
	for (long m = N0; m > 1; m >>= 1) {
		long j1 = 0;
		long h = m >> 1;
		for (long i = 0; i < h; i++) {
			long j2 = j1 + t - 1;
			uint64_t W = scaledRootM0PowsInvi[h + i];
			for (long j = j1; j <= j2; j++) {
				butt1(a[j], a[j+t], p, pInv, W);
			}
			j1 += (t << 1);
		}
		t <<= 1;
	}

	uint64_t NxScale = scaledN0Inv[index];
	for (long i = 0; i < N0; i++) {
		divByN(a[i], p, pInv, NxScale);
	}
}

void RingMultiplier::NTTPO2X1(uint64_t* a, long index) {
	uint64_t p = pVec[index];
	uint64_t pInv = pInvVec[index];
	uint64_t* scaledRootN1Powsi = scaledRootN1Pows[index];

	arrayBitReverse(a, N1);
	for (long i = 0; i < logN1; ++i) {
		long ihpow = 1 << i;
		long ipow = 1 << (i + 1);
		for (long j = 0; j < N1; j += ipow) {
			for (long k = 0; k < ihpow; ++k) {
				long idx = k << (logN1 - i - 1);
				uint64_t W = scaledRootN1Powsi[idx];
				butt2(a[j + k], a[j + k + ihpow], p, pInv, W);
			}
		}
	}
}

void RingMultiplier::INTTPO2X1(uint64_t* a, long index) {
	uint64_t p = pVec[index];
	uint64_t pInv = pInvVec[index];
	uint64_t* scaledRootN1PowsInvi = scaledRootN1PowsInv[index];

	arrayBitReverse(a, N1);
	for (long i = 0; i < logN1; ++i) {
		long ihpow = 1 << i;
		long ipow = 1 << (i + 1);
		for (long j = 0; j < N1; j += ipow) {
			for (long k = 0; k < ihpow; ++k) {
				long idx = k << (logN1 - i - 1);
				uint64_t W = scaledRootN1PowsInvi[idx];
				butt2(a[j + k], a[j + k + ihpow], p, pInv, W);
			}
		}
	}

	uint64_t NyScale = scaledN1Inv[index];
	for (long i = 0; i < N1; i++) {
		divByN(a[i], p, pInv, NyScale);
	}
}

void RingMultiplier::NTTX1(uint64_t* a, long index) {
	uint64_t pi = pVec[index];
	uint64_t pri = prVec[index];
	uint64_t pti = pTwok[index];
	uint64_t* rootM1DFTPowsi = rootM1DFTPows[index];
	uint64_t* rootM1PowsInvi = rootM1PowsInv[index];

	uint64_t* tmp = new uint64_t[N1];
	for (long i = 0; i < N1; ++i) tmp[i] = a[gM1Pows[N1 - i] - 1];
	for (long i = 0; i < N1; ++i) a[i] = tmp[i];
	delete[] tmp;

	NTTPO2X1(a, index);
	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], rootM1DFTPowsi[i], pi, pri, pti);
	}
	INTTPO2X1(a, index);

	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], rootM1PowsInvi[i], pi, pri, pti);
	}
}

void RingMultiplier::INTTX1(uint64_t* a, long index) {
	uint64_t pi = pVec[index];
	uint64_t pri = prVec[index];
	uint64_t pti = pTwok[index];
	uint64_t* omegaPowsi = rootM1Pows[index];
	uint64_t* dftomegaPowsInvi = rootM1DFTPowsInv[index];

	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], omegaPowsi[i], pi, pri, pti);
	}

	NTTPO2X1(a, index);
	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], dftomegaPowsInvi[i], pi, pri, pti);
	}
	INTTPO2X1(a, index);

	uint64_t* tmp = new uint64_t[N1]();
	for (long i = 0; i < N1; ++i) tmp[gM1Pows[N1 - i] - 1] = a[i];
	for (long i = 0; i < N1; ++i) a[i] = tmp[i];
	delete[] tmp;
}

void RingMultiplier::NTTX1Lazy(uint64_t* a, long index) {
	uint64_t pi = pVec[index];
	uint64_t pri = prVec[index];
	uint64_t pti = pTwok[index];
	uint64_t* dftomegaPowsi = rootM1DFTPows[index];

	uint64_t* tmp = new uint64_t[N1];
	for (long i = 0; i < N1; ++i) tmp[i] = a[gM1Pows[N1 - i] - 1];
	for (long i = 0; i < N1; ++i) a[i] = tmp[i];
	delete[] tmp;

	NTTPO2X1(a, index);
	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], dftomegaPowsi[i], pi, pri, pti);
	}
	INTTPO2X1(a, index);

}

void RingMultiplier::INTTX1Lazy(uint64_t* a, long index) {
	uint64_t pi = pVec[index];
	uint64_t pri = prVec[index];
	uint64_t pti = pTwok[index];
	uint64_t* dftomegaPowsInvi = rootM1DFTPowsInv[index];

	NTTPO2X1(a, index);

	for (long i = 0; i < N1; ++i) {
		mulModBarrett(a[i], a[i], dftomegaPowsInvi[i], pi, pri, pti);
	}

	INTTPO2X1(a, index);

	uint64_t* tmp = new uint64_t[N1]();
	for (long i = 0; i < N1; ++i) tmp[gM1Pows[N1 - i] - 1] = a[i];
	for (long i = 0; i < N1; ++i) a[i] = tmp[i];
	delete[] tmp;
}

void RingMultiplier::NTT(uint64_t* a, long index) {
	for (long j = 0; j < N1; ++j) {
		uint64_t* aj = a + (j << logN0);
		NTTX0(aj, index);
	}
	uint64_t* tmp = new uint64_t[N1];
	for (long j = 0; j < N0; ++j) {
		for (long k = 0; k < N1; ++k) {
			tmp[k] = a[j + (k << logN0)];
		}
		NTTX1(tmp, index);
		for (long k = 0; k < N1; ++k) {
			a[j + (k << logN0)] = tmp[k];
		}
	}
	delete[] tmp;
}

void RingMultiplier::INTT(uint64_t* a, long index) {
	uint64_t* tmp = new uint64_t[N1];
	for (long j = 0; j < N0; ++j) {
		for (long k = 0; k < N1; ++k) {
			tmp[k] = a[j + (k << logN0)];
		}
		INTTX1(tmp, index);
		for (long k = 0; k < N1; ++k) {
			a[j + (k << logN0)] = tmp[k];
		}
	}
	delete[] tmp;

	for (long j = 0; j < N1; ++j) {
		uint64_t* aj = a + (j << logN0);
		INTTX0(aj, index);
	}
}

void RingMultiplier::NTTLazy(uint64_t* a, long index) {
	for (long j = 0; j < N1; ++j) {
		uint64_t* aj = a + (j << logN0);
		NTTX0(aj, index);
	}
	uint64_t* tmp = new uint64_t[N1];
	for (long j = 0; j < N0; ++j) {
		for (long k = 0; k < N1; ++k) {
			tmp[k] = a[j + (k << logN0)];
		}
		NTTX1Lazy(tmp, index);
		for (long k = 0; k < N1; ++k) {
			a[j + (k << logN0)] = tmp[k];
		}
	}
	delete[] tmp;
}

void RingMultiplier::INTTLazy(uint64_t* a, long index) {
	uint64_t* tmp = new uint64_t[N1];
	for (long j = 0; j < N0; ++j) {
		for (long k = 0; k < N1; ++k) {
			tmp[k] = a[j + (k << logN0)];
		}
		INTTX1Lazy(tmp, index);
		for (long k = 0; k < N1; ++k) {
			a[j + (k << logN0)] = tmp[k];
		}
	}
	delete[] tmp;

	for (long j = 0; j < N1; ++j) {
		uint64_t* aj = a + (j << logN0);
		INTTX0(aj, index);
	}
}

uint64_t* RingMultiplier::toNTTX0(ZZ* a, long np) {

	uint64_t* ra = new uint64_t[np << logN0];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN0);
		for (long n = 0; n < N0; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTX0(rai, i);
	}
	NTL_EXEC_RANGE_END;
	return ra;
}

uint64_t* RingMultiplier::toNTTX1(ZZ* a, long np) {
	uint64_t* ra = new uint64_t[np << logN1];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN1);
		for (long n = 0; n < N1; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTX1(rai, i);
	}
	NTL_EXEC_RANGE_END;
	return ra;
}

uint64_t* RingMultiplier::toNTTX1Lazy(ZZ* a, long np) {
	uint64_t* ra = new uint64_t[np << logN1];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN1);
		for (long n = 0; n < N1; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTX1Lazy(rai, i);
	}
	NTL_EXEC_RANGE_END;
	return ra;
}

uint64_t* RingMultiplier::toNTT(ZZ* a, long np) {
	uint64_t* ra = new uint64_t[np << logN];
	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTT(rai, i);
	}
	NTL_EXEC_RANGE_END;
	return ra;
}

uint64_t* RingMultiplier::toNTTLazy(ZZ* a, long np) {
	uint64_t* ra = new uint64_t[np << logN];
	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t pi = pVec[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTLazy(rai, i);
	}
	NTL_EXEC_RANGE_END;
	return ra;
}

void RingMultiplier::addNTTAndEqual(uint64_t* ra, uint64_t* rb, long np) {
	for (long i = 0; i < np; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		uint64_t pi = pVec[i];
		for (long n = 0; n < N; ++n) {
			rai[n] += rbi[n];
			if(rai[n] > pi) rai[n] -= pi;
		}
	}
}

void RingMultiplier::reconstruct(ZZ* x, uint64_t* rx, long np, ZZ& q) {
	ZZ* pHatnp = pHat[np - 1];
	uint64_t* pHatInvModpnp = pHatInvModp[np - 1];
	mulmod_precon_t* coeffpinv_arraynp = coeffpinv_array[np - 1];
	ZZ& pProdnp = pProd[np - 1];
	ZZ& pProdhnp = pProdh[np - 1];

	NTL_EXEC_RANGE(N, first, last);
	for (long n = first; n < last; ++n) {
		ZZ& acc = x[n];
		QuickAccumBegin(acc, pProdnp.size());
		for (long i = 0; i < np; i++) {
			long p = pVec[i];
			long tt = pHatInvModpnp[i];
			mulmod_precon_t ttpinv = coeffpinv_arraynp[i];
			long s = MulModPrecon(rx[n + (i << logN)], tt, p, ttpinv);
			QuickAccumMulAdd(acc, pHatnp[i], s);
		}
		QuickAccumEnd(acc);
		rem(x[n], x[n], pProdnp);
		if (x[n] > pProdhnp) x[n] -= pProdnp;
		x[n] %= q;
	}
	NTL_EXEC_RANGE_END;
}

void RingMultiplier::multX0(ZZ* x, ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN0];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN0);
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long iy = 0; iy < N; iy += N0) {
			uint64_t* raij = rai + iy;
			NTTX0(raij, i);
		}

		for (int ix = 0; ix < N0; ++ix) {
			rbi[ix] = _ntl_general_rem_one_struct_apply(b[ix].rep, pi, red_ss);
		}
		NTTX0(rbi, i);

		uint64_t* rxi = rx + (i << logN);
		for (long ix = 0; ix < N0; ++ix) {
			for (long iy = 0; iy < N; iy += N0) {
				long n = ix + iy;
				mulModBarrett(rxi[n], rai[n], rbi[ix], pi, pri, pTwoki);
			}
		}

		for (long iy = 0; iy < N; iy += N0) {
			uint64_t* rxij = rxi + iy;
			INTTX0(rxij, i);
		}
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rb;
	delete[] rx;
}

void RingMultiplier::multX0AndEqual(ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN0];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN0);
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long j = 0; j < N1; ++j) {
			uint64_t* raij = rai + (j << logN0);
			NTTX0(raij, i);
		}

		for (long ix = 0; ix < N0; ++ix) {
			rbi[ix] = _ntl_general_rem_one_struct_apply(b[ix].rep, pi, red_ss);
		}
		NTTX0(rbi, i);

		for (long ix = 0; ix < N0; ++ix) {
			for (long iy = 0; iy < N; iy += N0) {
				long n = ix + iy;
				mulModBarrett(rai[n], rai[n], rbi[ix], pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N1; ++j) {
			uint64_t* raij = rai + (j << logN0);
			INTTX0(raij, i);
		}

	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
	delete[] rb;
}

void RingMultiplier::multNTTX0(ZZ* x, ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN0);
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long j = 0; j < N1; ++j) {
			uint64_t* raij = rai + (j << logN0);
			NTTX0(raij, i);
		}

		uint64_t* rxi = rx + (i << logN);
		for (long ix = 0; ix < N0; ++ix) {
			for (long n = ix; n < N; n += N0) {
				mulModBarrett(rxi[n], rai[n], rbi[ix], pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N1; ++j) {
			uint64_t* rxij = rxi + (j << logN0);
			INTTX0(rxij, i);
		}

	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
}

void RingMultiplier::multNTTX0AndEqual(ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN0);
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long j = 0; j < N1; ++j) {
			uint64_t* raij = rai + (j << logN0);
			NTTX0(raij, i);
		}

		for (long ix = 0; ix < N0; ++ix) {
			for (long iy = 0; iy < N; iy += N0) {
				long n = ix + iy;
				mulModBarrett(rai[n], rai[n], rbi[ix], pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N1; ++j) {
			uint64_t* raij = rai + (j << logN0);
			INTTX0(raij, i);
		}

	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
}

void RingMultiplier::multDNTTX0(ZZ* x, uint64_t* ra, uint64_t* rb, long np, ZZ& q) {

}


void RingMultiplier::multX1(ZZ* x, ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN1];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* tmp = new uint64_t[1 << logN1];
		uint64_t* rai = ra + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			NTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}

		uint64_t* rbi = rb + (i << logN1);
		for (int iy = 0; iy < N1; ++iy) {
			rbi[iy] = _ntl_general_rem_one_struct_apply(b[iy].rep, pi, red_ss);
		}
		NTTX1(rbi, i);

		uint64_t* rxi = rx + (i << logN);
		for (long iy = 0; iy < N1; ++iy) {
			uint64_t rbiy = rbi[iy];
			uint64_t* rxiy = rxi + (iy << logN0);
			uint64_t* raiy = rai + (iy << logN0);
			for (long ix = 0; ix < N0; ++ix) {
				mulModBarrett(rxiy[ix], raiy[ix], rbiy, pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rxi[j + (k << logN0)];
			}
			INTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rxi[j + (k << logN0)] = tmp[k];
			}
		}
		delete[] tmp;
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rb;
	delete[] rx;
}

void RingMultiplier::multX1AndEqual(ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN1];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		uint64_t* tmp = new uint64_t[1 << logN1];
		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			NTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}

		uint64_t* rbi = rb + (i << logN1);
		for (long iy = 0; iy < N1; ++iy) {
			rbi[iy] = _ntl_general_rem_one_struct_apply(b[iy].rep, pi, red_ss);
		}
		NTTX1(rbi, i);

		for (long iy = 0; iy < N1; ++iy) {
			uint64_t rbiy = rbi[iy];
			uint64_t* raiy = rai + (iy << logN0);
			for (long ix = 0; ix < N0; ++ix) {
				mulModBarrett(raiy[ix], raiy[ix], rbiy, pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			INTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}
		delete[] tmp;
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
	delete[] rb;
}

void RingMultiplier::multNTTX1(ZZ* x, ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* tmp = new uint64_t[1 << logN1];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN1);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			NTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}

		uint64_t* rxi = rx + (i << logN);
		for (long iy = 0; iy < N1; ++iy) {
			uint64_t rbiy = rbi[iy];
			uint64_t* rxiy = rxi + (iy << logN0);
			uint64_t* raiy = rai + (iy << logN0);
			for (long ix = 0; ix < N0; ++ix) {
				mulModBarrett(rxiy[ix], raiy[ix], rbiy, pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rxi[j + (k << logN0)];
			}
			INTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rxi[j + (k << logN0)] = tmp[k];
			}
		}
		delete[] tmp;
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rx;
}

void RingMultiplier::multNTTX1AndEqual(ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN1);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		uint64_t* tmp = new uint64_t[1 << logN1];
		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			NTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}

		for (long iy = 0; iy < N1; ++iy) {
			uint64_t rbiy = rbi[iy];
			uint64_t* raiy = rai + (iy << logN0);
			for (long ix = 0; ix < N0; ++ix) {
				mulModBarrett(raiy[ix], raiy[ix], rbiy, pi, pri, pTwoki);
			}
		}

		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rai[j + (k << logN0)];
			}
			INTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rai[j + (k << logN0)] = tmp[k];
			}
		}
		delete[] tmp;
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
}

void RingMultiplier::multDNTTX1(ZZ* x, uint64_t* ra, uint64_t* rb, long np, ZZ& q) {
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN1);
		uint64_t* rxi = rx + (i << logN);
		for (long iy = 0; iy < N1; ++iy) {
			uint64_t rbiy = rbi[iy];
			uint64_t* rxiy = rxi + (iy << logN0);
			uint64_t* raiy = rai + (iy << logN0);
			for (long ix = 0; ix < N0; ++ix) {
				mulModBarrett(rxiy[ix], raiy[ix], rbiy, pi, pri, pTwoki);
			}
		}

		uint64_t* tmp = new uint64_t[1 << logN1];
		for (long j = 0; j < N0; ++j) {
			for (long k = 0; k < N1; ++k) {
				tmp[k] = rxi[j + (k << logN0)];
			}
			INTTX1(tmp, i);
			for (long k = 0; k < N1; ++k) {
				rxi[j + (k << logN0)] = tmp[k];
			}
		}
		delete[] tmp;
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] rx;
}

void RingMultiplier::mult(ZZ* x, ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
			rbi[n] = _ntl_general_rem_one_struct_apply(b[n].rep, pi, red_ss);
		}
		NTT(rai, i);
		NTTLazy(rbi, i);

		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rbi[n], pi, pri, pTwoki);
		}
		INTTLazy(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rb;
	delete[] rx;
}

void RingMultiplier::multAndEqual(ZZ* a, ZZ* b, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rb = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
			rbi[n] = _ntl_general_rem_one_struct_apply(b[n].rep, pi, red_ss);
		}
		NTT(rai, i);
		NTTLazy(rbi, i);

		for (long n = 0; n < N; ++n) {
			mulModBarrett(rai[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTTLazy(rai, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
	delete[] rb;
}

void RingMultiplier::multNTT(ZZ* x, ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTLazy(rai, i);

		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTTLazy(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rx;
}

void RingMultiplier::multNTTAndEqual(ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTTLazy(rai, i);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rai[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTTLazy(rai, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
}

void RingMultiplier::multNTTLazy(ZZ* x, ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTT(rai, i);

		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTTLazy(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rx;
}

void RingMultiplier::multNTTLazyAndEqual(ZZ* a, uint64_t* rb, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTT(rai, i);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rai[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTTLazy(rai, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
}

void RingMultiplier::multDNTT(ZZ* x, uint64_t* ra, uint64_t* rb, long np, ZZ& q) {
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];

		uint64_t* rai = ra + (i << logN);
		uint64_t* rbi = rb + (i << logN);
		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rbi[n], pi, pri, pTwoki);
		}

		INTT(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] rx;
}

void RingMultiplier::square(ZZ* x, ZZ* a, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTT(rai, i);

		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rai[n], pi, pri, pTwoki);
		}

		INTT(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] ra;
	delete[] rx;
}

void RingMultiplier::squareAndEqual(ZZ* a, long np, ZZ& q) {
	uint64_t* ra = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = 0; i < np; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		_ntl_general_rem_one_struct* red_ss = red_ss_array[i];

		uint64_t* rai = ra + (i << logN);
		for (long n = 0; n < N; ++n) {
			rai[n] = _ntl_general_rem_one_struct_apply(a[n].rep, pi, red_ss);
		}
		NTT(rai, i);

		for (long n = 0; n < N; ++n) {
			mulModBarrett(rai[n], rai[n], rai[n], pi, pri, pTwoki);
		}

		INTT(rai, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(a, ra, np, q);

	delete[] ra;
}

void RingMultiplier::squareNTT(ZZ* x, uint64_t* ra, long np, ZZ& q) {
	uint64_t* rx = new uint64_t[np << logN];

	NTL_EXEC_RANGE(np, first, last);
	for (long i = first; i < last; ++i) {
		uint64_t pi = pVec[i];
		uint64_t pri = prVec[i];
		long pTwoki = pTwok[i];
		uint64_t* rai = ra + (i << logN);
		uint64_t* rxi = rx + (i << logN);
		for (long n = 0; n < N; ++n) {
			mulModBarrett(rxi[n], rai[n], rai[n], pi, pri, pTwoki);
		}
		INTT(rxi, i);
	}
	NTL_EXEC_RANGE_END;

	reconstruct(x, rx, np, q);

	delete[] rx;
}

void RingMultiplier::mulMod(uint64_t &r, uint64_t a, uint64_t b, uint64_t m) {
	unsigned __int128
	mul = static_cast<unsigned __int128>(a) * b;
	mul %= static_cast<unsigned __int128>(m);
	r = static_cast<uint64_t>(mul);
}

void RingMultiplier::mulModBarrett(uint64_t& r, uint64_t a, uint64_t b, uint64_t p, uint64_t pr, long twok) {
	unsigned __int128
	mul = static_cast<unsigned __int128>(a) * b;
	uint64_t atop, abot;
	abot = static_cast<uint64_t>(mul);
	atop = static_cast<uint64_t>(mul >> 64);
	unsigned __int128
	tmp = static_cast<unsigned __int128>(abot) * pr;
	tmp >>= 64;
	tmp += static_cast<unsigned __int128>(atop) * pr;
	tmp >>= twok - 64;
	tmp *= p;
	tmp = mul - tmp;
	r = static_cast<uint64_t>(tmp);
	if (r >= p) {
		r -= p;
	}
}

uint64_t RingMultiplier::powMod(uint64_t x, uint64_t y, uint64_t modulus) {
	uint64_t res = 1;
	while (y > 0) {
		if (y & 1) {
			mulMod(res, res, x, modulus);
		}
		y = y >> 1;
		mulMod(x, x, x, modulus);
	}
	return res;
}

uint64_t RingMultiplier::inv(uint64_t x) {
	uint64_t res = 1;
	for (long i = 0; i < 62; ++i) {
		res *= x;
		x *= x;
	}
	return res;
}

uint32_t RingMultiplier::bitReverse(uint32_t x) {
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
	return ((x >> 16) | (x << 16));
}

void RingMultiplier::findPrimeFactors(vector<uint64_t> &s, uint64_t number) {
	while (number % 2 == 0) {
		s.push_back(2);
		number /= 2;
	}
	for (uint64_t i = 3; i < sqrt(number); i++) {
		while (number % i == 0) {
			s.push_back(i);
			number /= i;
		}
	}
	if (number > 2) {
		s.push_back(number);
	}
}

uint64_t RingMultiplier::findPrimitiveRoot(uint64_t modulus) {
	vector<uint64_t> s;
	uint64_t phi = modulus - 1;
	findPrimeFactors(s, phi);
	for (uint64_t r = 2; r <= phi; r++) {
		bool flag = false;
		for (uint64_t i : s) {
			if(powMod(r, phi / i, modulus) == 1) {
				flag = true;
				break;
			}
		}
		if (flag == false) {
			return r;
		}
	}
	return -1;
}

// Algorithm to find m-th primitive root in Z_mod
uint64_t RingMultiplier::findMthRootOfUnity(uint64_t M, uint64_t mod) {
	uint64_t res = findPrimitiveRoot(mod);
	uint64_t factor = (mod - 1) / M;
	res = powMod(res, factor, mod);
	return res;
}