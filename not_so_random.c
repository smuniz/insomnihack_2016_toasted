#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {

    unsigned int seed;
    unsigned int outputs = 0;
    unsigned int i;

    if (argc != 3) {
        printf("Usage: %s <SEED> <OUTPUTS>", argv[0]);
        return 0;
    }
    
    seed = atoi(argv[1]);
    outputs = atoi(argv[2]);

    srandom(seed);

    printf("[+] Using seed : 0x%08X\n", seed);

    for (i=0; i < outputs; i++) {
        printf("\t%03d : 0x%08lX\n", i, random());
    }
    return -1;
}
