#include <stdio.h>
#include "debug.h"
#include "sfmm.h"
#include "heap.h"
#include  "mem.h"

int main(int argc, char const *argv[]) {
    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    debug("%f\n", *ptr);

    sf_free(ptr);

    return EXIT_SUCCESS;
}
