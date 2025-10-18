#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <omp.h>
#include "common_qs.h"


static void print_array(const char *label, const int *A, int n, int limit) {
    int i, m = (n < limit) ? n : limit;
    printf("%s [n=%d]: ", label, n);
    for (i = 0; i < m; ++i) {
        printf("%d", A[i]);
        if (i + 1 < m) printf(", ");
    }
    if (n > limit) printf(" ...");
    printf("\n");
}


static inline double wtime(void) { return omp_get_wtime(); }


static void quicksort_seq(int *A, int lo, int hi, int SMALL) {
    while (lo < hi) {
        int n = hi - lo + 1;
        if (n <= SMALL) { insertion_sort(A, lo, hi); return; }

        
        {
            int pidx = median3_index(A, lo, hi);
            int p = partition_with_pivot(A, lo, hi, pidx);

            
            if ((p - 1) - lo < hi - (p + 1)) {
                quicksort_seq(A, lo, p - 1, SMALL);
                lo = p + 1;
            } else {
                quicksort_seq(A, p + 1, hi, SMALL);
                hi = p - 1;
            }
        }
    }
}


static inline int choose_pivot_index(int *A, int lo, int hi, int use_random) {
    if (!use_random) return median3_index(A, lo, hi);
    
    {
        unsigned int n = (unsigned int)(hi - lo + 1);
        unsigned int h = (unsigned int)lo * 2654435761u ^ (unsigned int)hi * 97461u;
        int offset = (int)(h % n);
        return lo + offset;
    }
}


static void quicksort_par(int *A, int lo, int hi, int THRESH, int SMALL, int USE_RANDOM_PIVOT) {
    int n = hi - lo + 1;

    if (n <= SMALL) { insertion_sort(A, lo, hi); return; }
    if (n <= THRESH) { quicksort_seq(A, lo, hi, SMALL); return; }

    {
        int pidx = choose_pivot_index(A, lo, hi, USE_RANDOM_PIVOT);
        int p = partition_with_pivot(A, lo, hi, pidx);

        
        #pragma omp task shared(A) firstprivate(lo, p, THRESH, SMALL, USE_RANDOM_PIVOT)
        {
            if (lo < p - 1)
                quicksort_par(A, lo, p - 1, THRESH, SMALL, USE_RANDOM_PIVOT);
        }
        #pragma omp task shared(A) firstprivate(p, hi, THRESH, SMALL, USE_RANDOM_PIVOT)
        {
            if (p + 1 < hi)
                quicksort_par(A, p + 1, hi, THRESH, SMALL, USE_RANDOM_PIVOT);
        }
        #pragma omp taskwait
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n> [THRESH=16384] [SMALL=64] [RANDPIV=0] [PRINT=0]\n", argv[0]);
        return 1;
    }

    
    int n = atoi(argv[1]);
    int THRESH = (argc > 2 ? atoi(argv[2]) : (1<<14));
    int SMALL  = (argc > 3 ? atoi(argv[3]) : 64);
    int USE_RANDOM_PIVOT = (argc > 4 ? atoi(argv[4]) : 0);
    int PRINT  = (argc > 5 ? atoi(argv[5]) : 0);

    
    int *A = (int*)malloc(sizeof(int) * n);
    if (!A) { fprintf(stderr, "alloc failed for n=%d\n", n); return 1; }

    srand(12345); 
    {
        int i;
        for (i = 0; i < n; ++i) A[i] = rand();
    }

    int *A_in = NULL;
    if (PRINT) {
        int i;
        A_in = (int*)malloc(sizeof(int) * n);
        for (i = 0; i < n; ++i) A_in[i] = A[i];
        print_array("Input", A_in, n, 64);
    }

    
    double t0 = wtime();
    #pragma omp parallel
    {
        #pragma omp single nowait
        {
            quicksort_par(A, 0, n - 1, THRESH, SMALL, USE_RANDOM_PIVOT);
        }
    }
    double t1 = wtime();

    
    {
        int ok = 1, i;
        for (i = 1; i < n; ++i) { if (A[i - 1] > A[i]) { ok = 0; break; } }
        printf("[OpenMP] n=%d THRESH=%d SMALL=%d RANDPIV=%d  time=%.6f  sorted=%s\n",
               n, THRESH, SMALL, USE_RANDOM_PIVOT, (t1 - t0), ok ? "yes" : "NO");
    }

    if (PRINT) {
        print_array("Output", A, n, 64);
        free(A_in);
    }

    free(A);
    return 0;
}
