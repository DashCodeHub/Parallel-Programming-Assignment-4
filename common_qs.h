#ifndef COMMON_QS_H
#define COMMON_QS_H
#include <stdlib.h>
#include <stdint.h>

static inline void iswap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* insertion sort for tiny segments */
static inline void insertion_sort(int *A, int lo, int hi) {
    int i;
    for (i = lo + 1; i <= hi; ++i) {
        int key = A[i], j = i - 1;
        while (j >= lo && A[j] > key) { A[j+1] = A[j]; --j; }
        A[j+1] = key;
    }
}

/* median-of-three pivot selection */
static inline int median3_index(int *A, int lo, int hi) {
    int mid = lo + ((hi - lo) >> 1);
    int a = A[lo], b = A[mid], c = A[hi];
    // order three and return index of median
    if (a > b) { iswap(&a, &b); int t=lo; lo=mid; mid=t; }
    if (a > c) { iswap(&a, &c); int t=lo; lo=hi; hi=t; }
    if (b > c) { iswap(&b, &c); int t=mid; mid=hi; hi=t; }
    return mid; /* original index of median value */
}

/* Lomuto-ish partition with chosen pivot index */
static inline int partition_with_pivot(int *A, int lo, int hi, int pidx) {
    iswap(&A[pidx], &A[hi]);
    int pivot = A[hi];
    int i = lo - 1;
    int j;
    for (j = lo; j < hi; ++j) {
        if (A[j] <= pivot) { ++i; iswap(&A[i], &A[j]); }
    }
    iswap(&A[i+1], &A[hi]);
    return i + 1;
}

#endif

