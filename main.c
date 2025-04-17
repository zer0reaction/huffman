#include <stdio.h>
#include <assert.h>

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
    assert(fp);

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
    assert(fp);

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
    assert(fp);

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

void ts_print(u8 *ts) {
    u64 i;

    for (i = 0; i < da_size(ts); ++i) {
        if (ts[i] == '\n') {
            printf("\\n");
        } else if (ts[i] < 128) {
            putchar(ts[i]);
        }
    }
    printf("\n\n");
}

u8 *ts_build(Arena *a, Leaf *root) {
    u8 *ret;
    u8 *l, *r;

    if (root->left == NULL && root->right == NULL) {
        u8 *ch;
        u8 val = root->value;

        if (val == '[' || val == ']' || val == '\\') {
            ch = da_create(a, u8, 2);
            ch[0] = '\\';
            ch[1] = val;
        } else {
            ch = da_create(a, u8, 1);
            ch[0] = val;
        }

        return ch;
    }

    l = ts_build(a, root->left);
    r = ts_build(a, root->right);

    ret = da_create(a, u8, 0);

    da_push_back(ret, '[');
    da_append(ret, l);
    da_append(ret, r);
    da_push_back(ret, ']');

    return ret;
}

Leaf *leaf_new(Arena *a, u8 value) {
    Leaf *l;

    l = arena_alloc(a, sizeof(Leaf));
    l->left = l->right = NULL;
    l->weight = 0;
    l->value = value;

    return l;
}

Leaf *ts_decode(Arena *a, u8 *begin, u8 *end) {
    u8 *cur;
    Leaf *left, *right, *ret;

    if (end - begin == 1) {
        return leaf_new(a, begin[0]);
    } else if (end - begin == 2 && begin[0] == '\\') {
        return leaf_new(a, begin[1]);
    }

    cur = begin;
    left = right = NULL;

    while (cur < end) {
        if (cur[0] == '\\') {
            Leaf *temp;

            temp = ts_decode(a, cur, cur + 2);
            left == NULL ? (left = temp) : (right = temp);

            cur += 2;
        } else if (cur[0] != '[' && cur[0] != ']') {
            Leaf *temp;

            temp = ts_decode(a, cur, cur + 1);
            left == NULL ? (left = temp) : (right = temp);

            cur += 1;
        } else if (cur[0] == '[') {
            Leaf *temp;
            u8 *l, *r;
            u64 count;

            count = 0;
            l = cur + 1;

            do {
                if (cur[0] == '\\') cur++;
                else if (cur[0] == '[') count++;
                else if (cur[0] == ']') count--;
                cur++;
            } while (count > 0);

            r = cur - 1;

            temp = ts_decode(a, l, r);
            left == NULL ? (left = temp) : (right = temp);
        } else assert(0);
    }

    ret = arena_alloc(a, sizeof(Leaf));
    ret->left = left;
    ret->right = right;
    ret->weight = 0;
    ret->value = 0;

    return ret;
}

void write_bit(FILE *fp, c8 bit, util_bool force) {
    static u8 accum = 0;
    static c8 c = 0;
    c8 mask;

    if (force && accum > 0) {
        fwrite(&c, 1, 1, fp);
        return;
    }

    if (accum == 8) {
        fwrite(&c, 1, 1, fp);
        accum = 0;
        c = 0;
    }

    if (bit == '0') {
        accum++;
        return;
    }

    mask = 0x80; /* 0b10000000 */
    mask >>= accum;
    c |= mask;
    accum++;
}

void encode(u8 *data, c8 **codes, u8 *ts) {
    FILE *fp;
    u64 i, j, ts_len;

    fp = fopen("./encoded.hz", "ab");
    assert(fp);

    /* ts len, ts, encoded */
    ts_len = da_size(ts);

    fwrite(&ts_len, sizeof(ts_len), 1, fp);
    fwrite(ts, ts_len, 1, fp);

    for (i = 0; i < da_size(data); ++i) {
        u8 c = data[i];

        for (j = 0; j < da_size(codes[c]); ++j) {
            write_bit(fp, codes[c][j], util_false);
        }
    }
    write_bit(fp, 0, util_true);

    fclose(fp);
}

int main(int argc, char **argv) {
    Arena a = {0};
    u8 *data;
    u8 *encoded_data;
    u8 *ts;
    Leaf *leaves, *root, *dec;
    c8 **codes;

    if (argc != 2) {
        printf("Incorrect usage.\n");
        exit(1);
    }

    data = fload(&a, argv[1]);
    leaves = leaves_get(&a, data);
    root = tree_build(&a, leaves);
    codes = codes_gen(&a, root);
    ts = ts_build(&a, root);

    encode(data, codes, ts);

    arena_free(&a);
    return 0;
}
