#include "larpack.h"

void LARPACK(chegst)(const int *itype, const char *uplo, const int *n,
        float *A, const int *ldA, const float *B, const int *ldB, int *info) {

    // Check arguments
    *info = 0;
    int lower = LAPACK(lsame)(uplo, "L");
    int upper = LAPACK(lsame)(uplo, "U");
    if (*itype < 1 || *itype > 3)
        *info = -1;
    else if (!lower && !upper)
        *info = -2;
    else if (*n < 0)
        *info = -3;
    else if (*ldA < MAX(1, *n))
        *info = -5;
    else if (*ldB < MAX(1, *n))
        *info = -7;
    if (*info) {
        const int minfo = -*info;
        LAPACK(xerbla)("CHEGST", &minfo);
        return;
    }

    if (*n <= LARPACK_CROSSOVER) {
        // Unblocked
        LAPACK(chegs2)(itype, uplo, n, A, ldA, B, ldB, info);
        return;
    }

    // Recursive
    const int n1 = (*n >= 16) ? ((*n + 8) / 16) * 8 : *n / 2;
    const int n2 = *n - n1;

    // A_TL A_TR
    // A_BL A_BR
    float *const A_TL = A;
    float *const A_TR = A + 2 * *ldA * n1;
    float *const A_BL = A                 + 2 * n1;
    float *const A_BR = A + 2 * *ldA * n1 + 2 * n1;

    // B_TL B_TR
    // B_BL B_BR
    const float *const B_TL = B;
    const float *const B_TR = B + 2 * *ldB * n1;
    const float *const B_BL = B                 + 2 * n1;
    const float *const B_BR = B + 2 * *ldB * n1 + 2 * n1;

    // 1, -1, 1/2, -1/2
   	const float c1[] = {1, 0}, cm1[] = {-1, 0}, cp5[] = {.5, 0}, cmp5[] = {-.5, 0};

    // recursion(A_TL, B_TL)
    LARPACK(chegst)(itype, uplo, &n1, A_TL, ldA, B_TL, ldB, info);

    if (*itype == 1)
        if (lower) {
            // A_BL = A_BL / B_TL'
            BLAS(ctrsm)("R", "L", "C", "N", &n2, &n1, c1, B_TL, ldB, A_BL, ldA);
            // A_BL = A_BL - 1/2 B_BL * A_TL
            BLAS(chemm)("R", "L", &n2, &n1, cmp5, A_TL, ldA, B_BL, ldB, c1, A_BL, ldA);
            // A_BR = A_BR - A_BL * B_BL' - B_BL * A_BL'
            BLAS(cher2k)("L", "N", &n2, &n1, cm1, A_BL, ldA, B_BL, ldB, c1, A_BR, ldA);
            // A_BL = A_BL - 1/2 B_BL * A_TL
            BLAS(chemm)("R", "L", &n2, &n1, cmp5, A_TL, ldA, B_BL, ldB, c1, A_BL, ldA);
            // A_BL = B_BR \ A_BL
            BLAS(ctrsm)("L", "L", "N", "N", &n2, &n1, c1, B_BR, ldB, A_BL, ldA);
        } else {
            // A_TR = B_TL' \ A_TR
            BLAS(ctrsm)("L", "U", "C", "N", &n1, &n2, c1, B_TL, ldB, A_TR, ldA);
            // A_TR = A_TR - 1/2 A_TL * B_TR
            BLAS(chemm)("L", "U", &n1, &n2, cmp5, A_TL, ldA, B_TR, ldB, c1, A_TR, ldA);
            // A_BR = A_BR - A_TR' * B_TR - B_TR' * A_TR
            BLAS(cher2k)("U", "C", &n2, &n1, cm1, A_TR, ldA, B_TR, ldB, c1, A_BR, ldA);
            // A_TR = A_TR - 1/2 A_TL * B_TR
            BLAS(chemm)("L", "U", &n1, &n2, cmp5, A_TL, ldA, B_TR, ldB, c1, A_TR, ldA);
            // A_TR = A_TR / B_BR
            BLAS(ctrsm)("R", "U", "N", "N", &n1, &n2, c1, B_BR, ldB, A_TR, ldA);
        }
    else
        if (lower) {
            // A_BL = A_BL * B_TL
            BLAS(ctrmm)("R", "L", "N", "N", &n2, &n1, c1, B_TL, ldB, A_BL, ldA);
            // A_BL = A_BL + 1/2 A_BR * B_BL
            BLAS(chemm)("L", "L", &n2, &n1, cp5, A_BR, ldA, B_BL, ldB, c1, A_BL, ldA);
            // A_TL = A_TL + A_BL' * B_BL + B_BL' * A_BL
            BLAS(cher2k)("L", "C", &n1, &n2, c1, A_BL, ldA, B_BL, ldB, c1, A_TL, ldA);
            // A_BL = A_BL + 1/2 A_BR * B_BL
            BLAS(chemm)("L", "L", &n2, &n1, cp5, A_BR, ldA, B_BL, ldB, c1, A_BL, ldA);
            // A_BL = B_BR * A_BL
            BLAS(ctrmm)("L", "L", "C", "N", &n2, &n1, c1, B_BR, ldB, A_BL, ldA);
        } else {
            // A_TR = B_TL * A_TR
            BLAS(ctrmm)("L", "U", "N", "N", &n1, &n2, c1, B_TL, ldB, A_TR, ldA);
            // A_TR = A_TR + 1/2 B_TR A_BR
            BLAS(chemm)("R", "U", &n1, &n2, cp5, A_BR, ldA, B_TR, ldB, c1, A_TR, ldA);
            // A_TL = A_TL + A_TR * B_TR' + B_TR * A_TR'
            BLAS(cher2k)("U", "N", &n1, &n2, c1, A_TR, ldA, B_TR, ldB, c1, A_TL, ldA);
            // A_TR = A_TR + 1/2 B_TR * A_BR
            BLAS(chemm)("R", "U", &n1, &n2, cp5, A_BR, ldA, B_TR, ldB, c1, A_TR, ldA);
            // A_TR = A_TR * B_BR
            BLAS(ctrmm)("R", "U", "C", "N", &n1, &n2, c1, B_BR, ldB, A_TR, ldA);
        }

    // recursion(A_BR, B_BR)
    LARPACK(chegst)(itype, uplo, &n2, A_BR, ldA, B_BR, ldB, info);
}
