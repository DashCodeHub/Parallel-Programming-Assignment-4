/* qs_pthreads.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
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


typedef struct { int lo, hi; } task_t;

typedef struct {
    task_t *buf;
    int cap, head, tail, count;
    pthread_mutex_t m;
    pthread_cond_t  not_empty, not_full;
    int shutting_down;
} task_queue_t;

static inline double wtime() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

/* queue */
void tq_init(task_queue_t *q, int cap) {
    q->buf = (task_t*)malloc(sizeof(task_t)*cap);
    q->cap = cap; q->head = q->tail = q->count = 0; q->shutting_down = 0;
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}
void tq_destroy(task_queue_t *q) {
    free(q->buf);
    pthread_mutex_destroy(&q->m);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}
void tq_push(task_queue_t *q, task_t t) {
    pthread_mutex_lock(&q->m);
    while (!q->shutting_down && q->count == q->cap)
        pthread_cond_wait(&q->not_full, &q->m);
    if (!q->shutting_down) {
        q->buf[q->tail] = t;
        q->tail = (q->tail + 1) % q->cap;
        q->count++;
        pthread_cond_signal(&q->not_empty);
    }
    pthread_mutex_unlock(&q->m);
}
int tq_pop(task_queue_t *q, task_t *out) {
    pthread_mutex_lock(&q->m);
    while (!q->shutting_down && q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->m);
    int ok = 0;
    if (q->count > 0) {
        *out = q->buf[q->head];
        q->head = (q->head + 1) % q->cap;
        q->count--; ok = 1;
        pthread_cond_signal(&q->not_full);
    }
    pthread_mutex_unlock(&q->m);
    return ok;
}

static int *G;               /* global array */
static int THRESH = 1<<14;   /* parallel cutoff (tune in Part 2) */
static int SMALL = 64;       /* small-array insertion sort threshold */
static int USE_RANDOM_PIVOT = 0;

static task_queue_t Q;
static volatile int outstanding = 0;  /* #unsolved tasks */
static pthread_mutex_t out_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  out_c = PTHREAD_COND_INITIALIZER;

static inline void dec_outstanding() {
    pthread_mutex_lock(&out_m);
    outstanding--;
    if (outstanding == 0) pthread_cond_signal(&out_c);
    pthread_mutex_unlock(&out_m);
}
static inline void inc_outstanding_by(int k) {
    pthread_mutex_lock(&out_m);
    outstanding += k;
    pthread_mutex_unlock(&out_m);
}

/* push a new quicksort job */
static void enqueue_qs(int lo, int hi) {
    task_t t = {lo, hi};
    inc_outstanding_by(1);
    tq_push(&Q, t);
}

static void sequential_qs(int *A, int lo, int hi) {
    while (lo < hi) {
        int n = hi - lo + 1;
        if (n <= SMALL) { insertion_sort(A, lo, hi); return; }
        int pidx = USE_RANDOM_PIVOT
            ? (lo + rand() % n)
            : median3_index(A, lo, hi);
        int p = partition_with_pivot(A, lo, hi, pidx);
        if (p - 1 - lo < hi - (p + 1)) { /* tail recurse on larger side */
            sequential_qs(A, lo, p - 1);
            lo = p + 1;
        } else {
            sequential_qs(A, p + 1, hi);
            hi = p - 1;
        }
    }
}

static void process_segment(int lo, int hi) {
    int n = hi - lo + 1;
    if (n <= SMALL) {
        insertion_sort(G, lo, hi);
        return;
    }
    if (n <= THRESH) { /* fall back to sequential */
        sequential_qs(G, lo, hi);
        return;
    }
    int pidx = USE_RANDOM_PIVOT
        ? (lo + rand() % n)
        : median3_index(G, lo, hi);
    int p = partition_with_pivot(G, lo, hi, pidx);

    /* spawn children as separate tasks */
    int left_n  = p - 1 - lo + 1;
    int right_n = hi - (p + 1) + 1;
    if (left_n > 0)  enqueue_qs(lo, p - 1);
    if (right_n > 0) enqueue_qs(p + 1, hi);
}

static void *worker(void *arg) {
    (void)arg;
    task_t t;
    for (;;) {
        if (!tq_pop(&Q, &t)) break;
        if (t.hi < t.lo) { dec_outstanding(); continue; }
        process_segment(t.lo, t.hi);
        dec_outstanding();
    }
    return NULL;
}

int main(int argc, char **argv) {
    int n = (argc > 1 ? atoi(argv[1]) : (1<<26)); /* start smaller, scale up */
    int P = (argc > 2 ? atoi(argv[2]) : 12);
    THRESH = (argc > 3 ? atoi(argv[3]) : (1<<14));
    SMALL  = (argc > 4 ? atoi(argv[4]) : 64);
    USE_RANDOM_PIVOT = (argc > 5 ? atoi(argv[5]) : 0);

    G = (int*)malloc(sizeof(int)*n);
    srand(12345);
    int i;
    for (i=0;i<n;i++) G[i] = rand(); /* uniform */
    int  PRINT = (argc > 6 ? atoi(argv[6]):0);
    int *G_in = NULL;
    if (PRINT){
    G_in = (int*)malloc(sizeof(int)*n);
    for (i = 0; i<n;++i) G_in[i] = G[i];
    print_array("Input", G_in, n, 64);
    }
    /* Build thread pool and queue */
    tq_init(&Q, 1<<20); /* big enough queue */
    pthread_t *th = (pthread_t*)malloc(sizeof(pthread_t)*P);

    double t0 = wtime();
    /* seed one top-level job */
    outstanding = 0;
    enqueue_qs(0, n-1);

    for (i=0;i<P;i++) pthread_create(&th[i], NULL, worker, NULL);

    /* wait until all tasks finish */
    pthread_mutex_lock(&out_m);
    while (outstanding != 0) pthread_cond_wait(&out_c, &out_m);
    pthread_mutex_unlock(&out_m);

    /* stop workers */
    pthread_mutex_lock(&Q.m);
    Q.shutting_down = 1;
    pthread_cond_broadcast(&Q.not_empty);
    pthread_mutex_unlock(&Q.m);

    for (i=0;i<P;i++) pthread_join(th[i], NULL);
    double t1 = wtime();

    /* verify sorted */
    int ok = 1;
    for (i=1;i<n;i++) if (G[i-1] > G[i]) { ok = 0; break; }
    printf("[PThreads] n=%d P=%d THRESH=%d SMALL=%d RANDPIV=%d  time=%.3f  sorted=%s\n",
           n, P, THRESH, SMALL, USE_RANDOM_PIVOT, t1-t0, ok?"yes":"NO");
    if (PRINT){
    print_array("Output", G, n, 64);
    free(G_in);
    }

    free(th); tq_destroy(&Q); free(G);
    return ok?0:1;
}

