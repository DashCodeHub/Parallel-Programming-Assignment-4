// Sequential quicksort baseline timing (C90-compatible)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

static double wtime(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void iswap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

static void insertion_sort(int *A, int lo, int hi) {
    int i, j, key;
    for (i = lo + 1; i <= hi; ++i) {
        key = A[i]; j = i - 1;
        while (j >= lo && A[j] > key) { A[j + 1] = A[j]; --j; }
        A[j + 1] = key;
    }
}

/* median-of-three: return index among lo, mid, hi of median value */
static int median3_index(int *A, int lo, int hi) {
    int mid = lo + ((hi - lo) >> 1);
    int a = A[lo], b = A[mid], c = A[hi];
    int lo_i = lo, mid_i = mid, hi_i = hi;

    if (a > b) { int ta=a; a=b; b=ta; {int ti=lo_i; lo_i=mid_i; mid_i=ti;} }
    if (a > c) { int ta=a; a=c; c=ta; {int ti=lo_i; lo_i=hi_i; hi_i=ti;} }
    if (b > c) { int tb=b; b=c; c=tb; {int ti=mid_i; mid_i=hi_i; hi_i=ti;} }
    return mid_i;
}

/* Lomuto partition using chosen pivot index */
static int partition_with_pivot(int *A, int lo, int hi, int pidx) {
    int i = lo - 1, j, pivot;
    iswap(&A[pidx], &A[hi]);
    pivot = A[hi];
    for (j = lo; j < hi; ++j) {
        if (A[j] <= pivot) { ++i; iswap(&A[i], &A[j]); }
    }
    iswap(&A[i + 1], &A[hi]);
    return i + 1;
}

/* Sequential quicksort with insertion cutoff + tail recursion elimination */
static void quicksort_seq(int *A, int lo, int hi) {
    const int SMALL = 32;
    while (lo < hi) {
        int n = hi - lo + 1;
        if (n <= SMALL) { insertion_sort(A, lo, hi); return; }

        /* choose pivot (median-of-three) and partition */
        {
            int pidx = median3_index(A, lo, hi);
            int p = partition_with_pivot(A, lo, hi, pidx);

            /* recurse on smaller side first; loop on larger side */
            if ((p - 1) - lo < hi - (p + 1)) {
                quicksort_seq(A, lo, p - 1);
                lo = p + 1;
            } else {
                quicksort_seq(A, p + 1, hi);
                hi = p - 1;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n> [seed=12345] [verify=1]\n", argv[0]);
        return 1;
    }

    {
        int n = atoi(argv[1]);
        int seed = (argc > 2 ? atoi(argv[2]) : 12345);
        int verify = (argc > 3 ? atoi(argv[3]) : 1);

        int *A = (int*)malloc(sizeof(int) * n);
        if (!A) { fprintf(stderr, "alloc failed for n=%d\n", n); return 1; }

        srand(seed);
        {
            int i;
            for (i = 0; i < n; ++i) A[i] = rand();
        }

        double t0 = wtime();
        quicksort_seq(A, 0, n - 1);
        double t1 = wtime();
        printf("[Quicksort-SEQ] n=%d time=%.6f sec\n", n, t1 - t0);

        if (verify) {
            int ok = 1, i;
            for (i = 1; i < n; ++i) { if (A[i - 1] > A[i]) { ok = 0; break; } }
            printf("sorted=%s\n", ok ? "yes" : "NO");
        }

        free(A);
    }
    return 0;
}
