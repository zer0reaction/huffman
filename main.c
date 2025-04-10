#include <stdio.h>

#define ARENA_IMPLEMENTATION
#define DA_IMPLEMENTATION
#include "../util/da.h"

#define DEBUG
#include "../util/debuginfo.h"

typedef struct Leaf Leaf;
struct Leaf {
    Leaf *left, *right;
    u64 weight;
    u8 value;
};

u8 *fload(Arena *a, const char *path) {
    FILE *fp;
    u8 *data;
    u64 size;

    fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    data = da_create(a, u8, size);
    fread(data, size, 1, fp);

    DEBUG_INFO("fload", ("Loaded %s, %lu bytes", path, size));

    fclose(fp);
    return data;
}

Leaf *leaves_get(Arena *a, u8 *data) {
    u64 i;
    Leaf *leaves;
    u64 count[256] = {0};

    leaves = da_create(a, Leaf, 0);

    for (i = 0; i < da_size(data); ++i) {
        count[data[i]]++;
    }

    for (i = 0; i < 256; ++i) {
        if (count[i]) {
            Leaf l;
            l.left = l.right = NULL;
            l.weight = count[i];
            l.value = i;
            da_push_back(leaves, l);
        }
    }

    DEBUG_INFO("leaves_get", ("Got %lu leaves", da_size(leaves)));

    return leaves;
}

int main(int argc, char **argv) {
    Arena a = {0};
    u8 *data;
    Leaf *leaves;

    if (argc != 2) {
        printf("Incorrect usage.\n");
        exit(1);
    }

    data = fload(&a, argv[1]);
    leaves = leaves_get(&a, data);

    arena_free(&a);
    return 0;
}
