#ifndef AMGCL_INTERP_SA_EMIN_HPP
#define AMGCL_INTERP_SA_EMIN_HPP

/*
The MIT License

Copyright (c) 2012 Denis Demidov <ddemidov@ksu.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   interp_sa_emin.hpp
 * \author Denis Demidov <ddemidov@ksu.ru>
 * \brief  Interpolation scheme based on smoothed aggregation with energy minimization.
 */

#include <vector>
#include <algorithm>
#include <functional>

#include <boost/typeof/typeof.hpp>

#include <amgcl/spmat.hpp>
#include <amgcl/aggr_connect.hpp>
#include <amgcl/tictoc.hpp>

namespace amgcl {

namespace interp {

/// Interpolation scheme based on smoothed aggregation with energy minimization.
/**
 * See \ref Sala_2008 "Sala (2008)"
 *
 * \param aggr_type \ref aggregation "Aggregation scheme".
 *
 * \ingroup interpolation
 */
template <class aggr_type>
struct sa_emin {

/// Parameters controlling aggregation.
struct params {
    /// Parameter \f$\varepsilon_{str}\f$ defining strong couplings.
    /**
     * Variable \f$i\f$ is defined to be strongly coupled to another variable,
     * \f$j\f$, if \f[|a_{ij}| \geq \varepsilon\sqrt{a_{ii} a_{jj}}\quad
     * \text{with fixed} \quad \varepsilon = \varepsilon_{str} \left(
     * \frac{1}{2} \right)^l,\f]
     * where \f$l\f$ is level number (finest level is 0).
     */
    mutable float eps_strong;

    params() : eps_strong(0.08f) {}
};

/// Constructs coarse level by aggregation.
/**
 * Returns interpolation operator, which is enough to construct system matrix
 * at coarser level.
 *
 * \param A   system matrix.
 * \param prm parameters.
 *
 * \returns interpolation operator.
 */
template < class value_t, class index_t >
static std::pair<
    sparse::matrix<value_t, index_t>,
    sparse::matrix<value_t, index_t>
    >
interp(const sparse::matrix<value_t, index_t> &A, const params &prm) {
    TIC("aggregates");
    BOOST_AUTO(aggr, aggr_type::aggregates(A, aggr::connect(A, prm.eps_strong)));
    prm.eps_strong *= 0.5;
    TOC("aggregates");

    const index_t n = sparse::matrix_rows(A);

    index_t nc = std::max(
            static_cast<index_t>(0),
            *std::max_element(aggr.begin(), aggr.end()) + static_cast<index_t>(1)
            );

    BOOST_AUTO(Dinv, sparse::diagonal(A));
    for(index_t i = 0; i < n; ++i) Dinv[i] = 1 / Dinv[i];

    // Compute smoothed nterpolation and restriction operators.
    static std::pair<
        sparse::matrix<value_t, index_t>,
        sparse::matrix<value_t, index_t>
    > PR;

    TIC("smoothed interpolation");
    improve_tentative_interp(A, Dinv, aggr, nc).swap(PR.first);
    TOC("smoothed interpolation");

    TIC("smoothed restriction");
    sparse::transpose(
            improve_tentative_interp(sparse::transpose(A), Dinv, aggr, nc)
            ).swap(PR.second);
    TOC("smoothed restriction");

    return PR;
}

private:

template <class spmat>
static std::vector<typename sparse::matrix_value<spmat>::type>
colwise_inner_prod(const spmat &A, const spmat &B) {
    typedef typename sparse::matrix_value<spmat>::type value_t;
    typedef typename sparse::matrix_index<spmat>::type index_t;

    const index_t n = sparse::matrix_rows(A);
    const index_t m = sparse::matrix_cols(A);

    assert(n == sparse::matrix_rows(B));
    assert(m == sparse::matrix_cols(B));

    BOOST_AUTO(Arow, sparse::matrix_outer_index(A));
    BOOST_AUTO(Acol, sparse::matrix_inner_index(A));
    BOOST_AUTO(Aval, sparse::matrix_values(A));

    BOOST_AUTO(Brow, sparse::matrix_outer_index(B));
    BOOST_AUTO(Bcol, sparse::matrix_inner_index(B));
    BOOST_AUTO(Bval, sparse::matrix_values(B));

    std::vector<value_t> sum(m, static_cast<value_t>(0));

    for(index_t i = 0; i < n; ++i) {
        for(
                index_t ja = Arow[i], ea = Arow[i + 1],
                        jb = Brow[i], eb = Brow[i + 1];
                ja < ea && jb < eb;
           )
        {
            index_t ca = Acol[ja];
            index_t cb = Bcol[jb];

            if (ca < cb)
                ++ja;
            else if (cb < ca)
                ++jb;
            else /*ca == cb*/ {
                sum[ca] += Aval[ja] * Bval[jb];
                ++ja;
                ++jb;
            }
        }
    }

    return sum;
}

template <class spmat>
static std::vector<typename sparse::matrix_value<spmat>::type>
colwise_norm(const spmat &A) {
    typedef typename sparse::matrix_value<spmat>::type value_t;
    typedef typename sparse::matrix_index<spmat>::type index_t;

    const index_t n = sparse::matrix_rows(A);
    const index_t m = sparse::matrix_cols(A);

    BOOST_AUTO(Arow, sparse::matrix_outer_index(A));
    BOOST_AUTO(Acol, sparse::matrix_inner_index(A));
    BOOST_AUTO(Aval, sparse::matrix_values(A));

    std::vector<value_t> sum(m, static_cast<value_t>(0));

    for(index_t i = 0; i < n; ++i)
        for(index_t j = Arow[i], e = Arow[i + 1]; j < e; ++j)
            sum[Acol[j]] += Aval[j] * Aval[j];

    return sum;
}

template <typename value_t, typename index_t>
static sparse::matrix<value_t, index_t>
improve_tentative_interp(const sparse::matrix<value_t, index_t> &A,
        const std::vector<value_t> &Dinv, const std::vector<index_t> &aggr,
        index_t nc)
{
    const index_t n = sparse::matrix_rows(A);

    sparse::matrix<value_t, index_t> AP(n, nc);
    std::fill(AP.row.begin(), AP.row.end(), static_cast<index_t>(0));

    sparse::matrix<value_t, index_t> ADAP(n, nc);
    std::fill(ADAP.row.begin(), ADAP.row.end(), static_cast<index_t>(0));

#pragma omp parallel
    {
#ifdef _OPENMP
        int nt  = omp_get_num_threads();
        int tid = omp_get_thread_num();

        index_t chunk_size  = (n + nt - 1) / nt;
        index_t chunk_start = tid * chunk_size;
        index_t chunk_end   = std::min(n, chunk_start + chunk_size);
#else
        index_t chunk_start = 0;
        index_t chunk_end   = n;
#endif

        std::vector<index_t> marker(nc, static_cast<index_t>(-1));

        // Compute A * P_tent product. P_tent is stored implicitly in aggr.
        // 1. Structure of the product result:
        for(index_t i = chunk_start; i < chunk_end; ++i) {
            for(index_t j = A.row[i], e = A.row[i + 1]; j < e; ++j) {
                index_t g = aggr[A.col[j]];
                if (g < 0) continue;

                if (marker[g] != i) {
                    marker[g] = i;
                    ++AP.row[i + 1];
                }
            }
        }

        std::fill(marker.begin(), marker.end(), static_cast<index_t>(-1));

#pragma omp barrier
#pragma omp single
        {
            std::partial_sum(AP.row.begin(), AP.row.end(), AP.row.begin());
            AP.reserve(AP.row.back());
        }

        // 2. Compute the product result.
        for(index_t i = chunk_start; i < chunk_end; ++i) {
            index_t row_beg = AP.row[i];
            index_t row_end = row_beg;
            for(index_t j = A.row[i], e = A.row[i + 1]; j < e; ++j) {
                index_t g = aggr[A.col[j]];
                if (g < 0) continue;

                if (marker[g] < row_beg) {
                    marker[g] = row_end;
                    AP.col[row_end] = g;
                    AP.val[row_end] = A.val[j];
                    ++row_end;
                } else {
                    AP.val[marker[g]] += A.val[j];
                }
            }
        }

        std::fill(marker.begin(), marker.end(), static_cast<index_t>(-1));

#pragma omp barrier

        // Compute A * Dinv * AP
        for(index_t ia = chunk_start; ia < chunk_end; ++ia) {
            for(index_t ja = A.row[ia], ea = A.row[ia + 1]; ja < ea; ++ja) {
                index_t ca = A.col[ja];
                for(index_t jb = AP.row[ca], eb = AP.row[ca + 1]; jb < eb; ++jb) {
                    index_t cb = AP.col[jb];

                    if (marker[cb] != ia) {
                        marker[cb] = ia;
                        ++ADAP.row[ia + 1];
                    }
                }
            }
        }

        std::fill(marker.begin(), marker.end(), static_cast<index_t>(-1));

#pragma omp barrier
#pragma omp single
        {
            std::partial_sum(ADAP.row.begin(), ADAP.row.end(), ADAP.row.begin());
            ADAP.reserve(ADAP.row.back());
        }

        for(index_t ia = chunk_start; ia < chunk_end; ++ia) {
            index_t row_beg = ADAP.row[ia];
            index_t row_end = row_beg;

            for(index_t ja = A.row[ia], ea = A.row[ia + 1]; ja < ea; ++ja) {
                index_t ca = A.col[ja];
                value_t va = A.val[ja];
                value_t di = Dinv[ca];

                for(index_t jb = AP.row[ca], eb = AP.row[ca + 1]; jb < eb; ++jb) {
                    index_t cb = AP.col[jb];
                    value_t vb = AP.val[jb] * di;

                    if (marker[cb] < row_beg) {
                        marker[cb] = row_end;
                        ADAP.col[row_end] = cb;
                        ADAP.val[row_end] = va * vb;
                        ++row_end;
                    } else {
                        ADAP.val[marker[cb]] += va * vb;
                    }
                }
            }
        }
    }

    sparse::sort_rows(AP);
    sparse::sort_rows(ADAP);

    std::vector<value_t> omega, denum;

#pragma omp parallel sections
    {
#pragma omp section
        {
            colwise_inner_prod(AP, ADAP).swap(omega);
        }
#pragma omp section
        {
            colwise_norm(ADAP).swap(denum);
        }
    }

    std::transform(omega.begin(), omega.end(), denum.begin(), omega.begin(),
            std::divides<value_t>());

    // Update AP to obtain P.
#pragma omp parallel for schedule(dynamic, 1024)
    for(index_t i = 0; i < n; ++i) {
        value_t di = Dinv[i];
        for(index_t j = AP.row[i], e = AP.row[i + 1]; j < e; ++j) {
            index_t c = AP.col[j];
            AP.val[j] *= -omega[c] * di;
            if (c == aggr[i]) AP.val[j] += 1;
        }
    }

    return AP;
}

};

} // namespace interp
} // namespace amgcl



#endif
