#include <stdio.h>

#define ARENA_REGION_REALLOC_CAPACITY (128)
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

void ptrs_sort(Leaf **ptrs) {
    u64 i, j;

    for (i = 1; i < da_size(ptrs); ++i) {
        for (j = 0; j < da_size(ptrs) - i; ++j) {
            if (ptrs[j]->weight > ptrs[j + 1]->weight) {
                Leaf *temp;
                temp = ptrs[j];
                ptrs[j] = ptrs[j + 1];
                ptrs[j + 1] = temp;
            }
        }
    }
}

void leaves_print(Leaf *leaves) {
    u64 i;

    for (i = 0; i < da_size(leaves); ++i) {
        printf("[%c, %lu]\n", (c8)(leaves[i].value), leaves[i].weight);
    }
}

Leaf *tree_build(Arena *a, Leaf *leaves) {
    u64 i;
    Leaf **ptrs;

    ptrs = da_create(a, Leaf *, 0);

    for (i = 0; i < da_size(leaves); ++i) {
        da_push_back(ptrs, &(leaves[i]));
    }

    while (da_size(ptrs) > 1) {
        Leaf *lp;

        ptrs_sort(ptrs);

        lp = arena_alloc(a, sizeof(Leaf));

        lp->left = ptrs[0];
        lp->right = ptrs[1];
        lp->weight = ptrs[0]->weight + ptrs[1]->weight;
        lp->value = 0;

        da_pop(ptrs, 0);
        da_pop(ptrs, 0);
        da_push(ptrs, 0, lp);
    }

    DEBUG_INFO("tree_build", ("Built tree"));

    return ptrs[0];
}

void tree_print(Leaf *root) {
    if (root == NULL) return;

    if (root->left == NULL && root->right == NULL) {
        printf("%c", (char)root->value);
        return;
    }

    printf("[");
    tree_print(root->left);
    printf(", ");
    tree_print(root->right);
    printf("]");
}

void code_gen(Arena *a, c8 **codes, c8 *buf, Leaf *root) {
    c8 *buf_left, *buf_right;

    if (root->left == NULL && root->right == NULL) {
        codes[root->value] = da_clone(a, buf);
        return;
    }

    buf_left = da_clone(a, buf);
    buf_right = da_clone(a, buf);
    da_push_back(buf_left, '0');
    da_push_back(buf_right, '1');

    code_gen(a, codes, buf_left, root->left);
    code_gen(a, codes, buf_right, root->right);
}

c8 **codes_gen(Arena *a, Leaf *root) {
    c8 *buf;
    c8 **codes;

    codes = da_create(a, c8 *, 256);
    memset(codes, 0, 256 * sizeof(*codes));
    buf = da_create(a, c8, 0);

    code_gen(a, codes, buf, root);

    DEBUG_INFO("codes_gen", ("Generated codes"));

    return codes;
}

void codes_print(c8 **codes) {
    u64 i, j;

    for (i = 0; i < da_size(codes); ++i) {
        if (!codes[i]) continue;

        printf("%c ", (char)i);
        for (j = 0; j < da_size(codes[i]); ++j) {
            putchar(codes[i][j]);
        }
        printf("\n");
    }
}

void encode_test(u8 *data, c8 **codes) {
    u64 i, j;
    FILE *fp;

    fp = fopen("./encode_test.hz", "a");

    for (i = 0; i < da_size(data); ++i) {
        for (j = 0; j < da_size(codes[data[i]]); ++j) {
            fputc(codes[data[i]][j], fp);
        }
    }

    DEBUG_INFO("encode_test", ("Testing encoding to encode_test.hz"));

    fclose(fp);
}

void decode_test(u8 *data, Leaf *root) {
    u64 i;
    FILE *fp;
    Leaf *cur;

    fp = fopen("./decode_test", "a");

    cur = root;

    for (i = 0; i < da_size(data); ++i) {
        if (cur->left == NULL && cur->right == NULL) {
            fputc(cur->value, fp);
            cur = root;
        }

        if (data[i] == '0') {
            cur = cur->left;
        } else if (data[i] == '1') {
            cur = cur->right;
        } else {
            printf("Error in test decoding\n");
            exit(1);
        }
    }
    fputc(cur->value, fp);

    DEBUG_INFO("decode_test", ("Testing decoding to decode_test"));

    fclose(fp);
}

int main(int argc, char **argv) {
    Arena a = {0};
    u8 *data;
    u8 *encoded_data;
    Leaf *leaves, *root;
    c8 **codes;

    if (argc != 2) {
        printf("Incorrect usage.\n");
        exit(1);
    }

    data = fload(&a, argv[1]);
    leaves = leaves_get(&a, data);
    root = tree_build(&a, leaves);
    codes = codes_gen(&a, root);

    encode_test(data, codes);

    encoded_data = fload(&a, "./encode_test.hz");
    decode_test(encoded_data, root);

    arena_free(&a);
    return 0;
}
