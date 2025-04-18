#include <stdio.h>
#include <assert.h>
#include <string.h>

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

u8 *file_load(Arena *a, const char *path) {
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

    DEBUG_INFO("file_load", ("Loaded %s, %lu bytes", path, size));

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
        accum = 0;
        c = 0;
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

/* ts len (8 bytes), ts, initial size (8 bytes), data */
void encode(const char *input_path, const char *output_path) {
    Arena a = {0};
    Arena temp = {0};
    FILE *fp;
    u64 i, j, ts_len, init_size;
    u8 *data, *ts;
    Leaf *leaves, *root;
    c8 **codes;

    fp = fopen(output_path, "wb");
    assert (fp);

    data = file_load(&a, input_path);

    leaves = leaves_get(&temp, data);
    root = tree_build(&temp, leaves);
    ts = ts_build(&temp, root);

    ts_len = da_size(ts);
    fwrite(&ts_len, sizeof(ts_len), 1, fp);
    fwrite(ts, ts_len, 1, fp);

    codes = codes_gen(&a, root);

    arena_free(&temp);
    
    init_size = da_size(data);
    fwrite(&init_size, sizeof(init_size), 1, fp);

    for (i = 0; i < init_size; ++i) {
        u8 c = data[i];

        for (j = 0; j < da_size(codes[c]); ++j) {
            write_bit(fp, codes[c][j], util_false);
        }

        if (i % 1000000 == 0) {
            fprintf(stdout, "\33[2K\rEncoding progress: %f%%", (float) i / init_size * 100.0);
            fflush(stdout);
        }
    }
    write_bit(fp, 0, util_true);

    fprintf(stdout, "\33[2K\rEncoding progress: 100%%\n");

    DEBUG_INFO("encode", ("Encoded file %s -> %s", input_path, output_path));

    fclose(fp);
    arena_free(&a);
}

c8 read_bit(u8 *data, u64 pos) {
    c8 c;
    u8 off;

    c = data[pos / 8];
    off = pos % 8;

    switch (off) {
        case 0: return (c & 0x80) ? '1' : '0';
        case 1: return (c & 0x40) ? '1' : '0';
        case 2: return (c & 0x20) ? '1' : '0';
        case 3: return (c & 0x10) ? '1' : '0';
        case 4: return (c & 0x08) ? '1' : '0';
        case 5: return (c & 0x04) ? '1' : '0';
        case 6: return (c & 0x02) ? '1' : '0';
        case 7: return (c & 0x01) ? '1' : '0';
        default: assert(0);
    }
}

/* ts len (8 bytes), ts, initial size (8 bytes), data */
void decode(const char *input_path, const char *output_path) {
    Arena a = {0};
    Arena temp = {0};
    FILE *fp;
    u8 *data, *enc, *ts;
    u64 i, written, ts_len, init_size;
    Leaf *cur, *root;

    fp = fopen(output_path, "ab");
    assert(fp);

    data = file_load(&temp, input_path);

    memcpy(&ts_len, data, 8);
    ts = da_create(&temp, u8, ts_len);
    memcpy(ts, data + 8, ts_len);

    memcpy(&init_size, data + 8 + ts_len, 8);

    enc = da_create(&a, u8, da_size(data) - 8 - ts_len - 8);
    memcpy(enc, data + 8 + ts_len + 8, da_size(enc));

    root = ts_decode(&a, ts + 1, ts + da_size(ts) - 1);

    arena_free(&temp);

    cur = root;

    written = 0;
    i = 0;

    while (1) {
        c8 bit;

        if (written == init_size) break;

        if (cur->left == NULL && cur->right == NULL) {
            fwrite(&(cur->value), 1, 1, fp);
            cur = root;
            written++;
        }

        bit = read_bit(enc, i);
        i++;

        if (bit == '0') cur = cur->left;
        else if (bit == '1') cur = cur->right;

        if (i % 1000000 == 0) {
            fprintf(stdout, "\33[2K\rDecoding progress: %f%%", (float) written / init_size * 100.0);
            fflush(stdout);
        }
    }

    fprintf(stdout, "\33[2K\rDecoding progress: 100%%\n");

    DEBUG_INFO("decode", ("Decoded file %s -> %s", input_path, output_path));

    arena_free(&a);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Incorrect usage.\n");
        exit(1);
    }

    if (strcmp(argv[1], "-c") == 0) {
        encode(argv[2], argv[3]);
    } else if (strcmp(argv[1], "-x") == 0) {
        decode(argv[2], argv[3]);
    } else {
        printf("Incorrect usage.\n");
        exit(1);
    }

    return 0;
}
