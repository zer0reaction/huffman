#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef unsigned char char8_t;

typedef struct leaf {
    struct leaf *left;
    struct leaf *right;
    uint32_t weight;
    char8_t symbol;
} leaf;

typedef struct {
    char8_t code[256];
    uint32_t len;
} character_code;

typedef struct {
    char8_t data[1024];
    uint32_t len;
} string;

void sort_leaves(leaf **leaves, uint32_t len);
uint32_t get_leaves(leaf ***l, const char *path);
leaf *create_tree(leaf **leaves, uint32_t leaves_count);
void encode(const char *input_path, const char *output_path, leaf *root);
void create_table(leaf *root, character_code (*table)[], character_code accum);
void decode(const char *input_path, const char *output_path, leaf *root);
string compile_tree(leaf *root);
leaf *decompile_tree(string tree_string);

int main(int argc, char **argv) {
    leaf *root, *decompiled;
    leaf **leaves = NULL;
    uint32_t leaves_count;
    string tree_string, new_tree_string;

    leaves_count = get_leaves(&leaves, argv[1]);
    root = create_tree(leaves, leaves_count);

    tree_string = compile_tree(root);
    printf("%s\n\n\n", (char*)(tree_string.data));

    decompiled = decompile_tree(tree_string);
    new_tree_string = compile_tree(decompiled);
    printf("%s\n\n\n", (char*)(new_tree_string.data));

    return 0;
}

void sort_leaves(leaf **leaves, uint32_t len) {
    uint32_t i, j;
    leaf *temp;

    for (i = 1; i < len; i++) {
        for (j = 0; j < len - i; j++) {
            if (leaves[j]->weight > leaves[j + 1]->weight) {
                temp = leaves[j];
                leaves[j] = leaves[j + 1];
                leaves[j + 1] = temp;
            }
        }
    }
}

uint32_t get_leaves(leaf ***l, const char *path) {
    uint32_t chars_count[256] = { 0 };
    uint32_t i, j, leaves_count;
    FILE *file = fopen(path, "rb");
    uint64_t file_size;
    char8_t *buffer;

    if (file == NULL) {
        printf("Can't open file %s.\n", path);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size == 0) {
        fprintf(stderr, "File is empty.\n");
        fclose(file);
        exit(1);
    }

    buffer = (char8_t*) malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory.\n");
        fclose(file);
        exit(1);
    }

    fread(buffer, file_size, 1, file);
    buffer[file_size] = 0;

    for (i = 0; i < file_size; i++) {
        chars_count[(uint32_t)buffer[i]]++;
    }

    leaves_count = 0;
    for (i = 0; i < 256; i++) {
        if (chars_count[i] > 0) {
            leaves_count++;
        }
    }

    j = 0;
    *l = calloc(leaves_count, sizeof(leaf*));
    for (i = 0; i < leaves_count; i++) {
        (*l)[i] = malloc(sizeof(leaf));

        (*l)[i]->left = NULL;
        (*l)[i]->right = NULL;

        for (; chars_count[j] == 0; j++);
        (*l)[i]->weight = chars_count[j];

        (*l)[i]->symbol = j++;
    }

    free(buffer);
    return leaves_count;
}

leaf *create_tree(leaf **leaves, uint32_t leaves_count) {
    uint32_t i;
    while (leaves_count > 1) {
        leaf *new_leaf;
        sort_leaves(leaves, leaves_count);

        new_leaf = malloc(sizeof(leaf));
        new_leaf->left = leaves[0];
        new_leaf->right = leaves[1];
        new_leaf->weight = leaves[0]->weight + leaves[1]->weight;
        new_leaf->symbol = 0;

        leaves[0] = new_leaf;
        for (i = 1; i < leaves_count - 1; i++) {
            leaves[i] = leaves[i + 1];
        }
        leaves_count--;
    }

    return leaves[0];
}

/* TODO: make compression like 8 times better */
void encode(const char *input_path, const char *output_path, leaf *root) {
    int c;
    uint64_t i;
    uint64_t file_size;
    character_code table[256] = { 0 };

    character_code accum;
    create_table(root, &table, accum);

    FILE *input_file = fopen(input_path, "rb");
    FILE *output_file = fopen(output_path, "a");

    if (input_file == NULL || output_file == NULL) {
        printf("Can't open files.\n");
        exit(1);
    }

    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    i = 0;
    c = fgetc(input_file);
    while (c != EOF) {
        fprintf(output_file, "%s", table[c].code);
        c = fgetc(input_file);
        i++;
        printf("Encoding progress: %ld / %ld\r", i, file_size);
    }
    printf("\n");

    fclose(input_file);
    fclose(output_file);
}

void decode(const char *input_path, const char *output_path, leaf *root) {
    int c;
    FILE *input_file = fopen(input_path, "rb");
    FILE *output_file = fopen(output_path, "a");
    leaf *leaf_ptr = root;

    if (input_file == NULL || output_file == NULL) {
        printf("Can't open files.\n");
        exit(1);
    }

    c = 1;
    while (c != EOF) {
        if (leaf_ptr->left == NULL && leaf_ptr->right == NULL) {
            fputc(leaf_ptr->symbol, output_file);
            leaf_ptr = root;
            continue;
        }

        c = fgetc(input_file);
        if (c == '0') {
            leaf_ptr = leaf_ptr->left;
        } else if (c == '1') {
            leaf_ptr = leaf_ptr->right;
        }
    }

    fclose(input_file);
    fclose(output_file);
}

void create_table(leaf *root, character_code (*table)[], character_code accum) {
    /* this happens if we are reading binary files */
    if (root == NULL) {
        return;
    }

    if (root->left == NULL && root->right == NULL) {
        (*table)[(int)(root->symbol)] = accum;
        return;
    }

    character_code accum_left = accum;
    accum_left.code[(accum_left.len)++] = '0';

    character_code accum_right = accum;
    accum_right.code[(accum_right.len)++] = '1';

    create_table(root->left, table, accum_left);
    create_table(root->right, table, accum_right);
}

string compile_tree(leaf *root) {
    if (root->left == NULL && root->right == NULL) {
        if (root->symbol == '(' || root->symbol == ')' || root->symbol == '\\') {
            string s;
            s.len = 2;
            (s.data)[0] = '\\';
            (s.data)[1] = root->symbol;
            (s.data)[2] = 0;
            return s;
        } else {
            string s;
            s.len = 1;
            (s.data)[0] = root->symbol;
            (s.data)[1] = 0;
            return s;
        }
    }

    uint32_t i;

    string left = compile_tree(root->left);
    string right = compile_tree(root->right);

    string s;
    s.len = left.len + right.len + 2;

    for (i = 0; i < s.len; i++)
        (s.data)[i] = 0;

    if (s.len >= 1024) {
        exit(1);
    }

    (s.data)[0] = '(';
    (s.data)[s.len - 1] = ')';

    for (i = 0; i < left.len; i++) {
        (s.data)[1 + i] = (left.data)[i];
    }
    for (i = 0; i < right.len; i++) {
        (s.data)[1 + left.len + i] = (right.data)[i];
    }

    (s.data)[s.len] = 0;

    return s;
}

leaf *decompile_tree(string tree_string) {
    bool flag; /* 0 - left, 1 - right */
    uint32_t i;
    string l_str, r_str;

    if (tree_string.len == 1) {
        leaf *l = malloc(sizeof(leaf));

        l->left = NULL;
        l->right = NULL;
        l->weight = 1;
        l->symbol = (tree_string.data)[0];

        return l;
    } else if (tree_string.len == 2 && (tree_string.data)[0] == '\\') {
        leaf *l = malloc(sizeof(leaf));

        l->left = NULL;
        l->right = NULL;
        l->weight = 1;
        l->symbol = (tree_string.data)[1];

        return l;
    }

    for (i = 0; i < tree_string.len - 1; i++) {
        (tree_string.data)[i] = (tree_string.data)[i + 1];
    }
    tree_string.len -= 2;

    l_str.len = r_str.len = 0;
    flag = false;

    for (i = 0; i < tree_string.len;) {
        if ((tree_string.data)[i] == '\\') {
            if (!flag) {
                (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                (l_str.data)[(l_str.len)++] = (tree_string.data)[i + 1];
                (l_str.data)[l_str.len] = 0;
            } else {
                (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                (r_str.data)[(r_str.len)++] = (tree_string.data)[i + 1];
                (r_str.data)[r_str.len] = 0;
            }

            i += 2;
        } else if ((tree_string.data)[i] == '(') {
            uint32_t count = 0;

            do {
                if ((tree_string.data)[i] == '\\') {
                    if (!flag) {
                        (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                        (l_str.data)[(l_str.len)++] = (tree_string.data)[i + 1];
                        (l_str.data)[l_str.len] = 0;
                    } else {
                        (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                        (r_str.data)[(r_str.len)++] = (tree_string.data)[i + 1];
                        (r_str.data)[r_str.len] = 0;
                    }

                    i += 2;
                } else if ((tree_string.data)[i] == '(') {
                    if (!flag) {
                        (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                        (l_str.data)[l_str.len] = 0;
                    } else {
                        (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                        (r_str.data)[r_str.len] = 0;
                    }
                    count++;
                    i++;
                } else if ((tree_string.data)[i] == ')') {
                    if (!flag) {
                        (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                        (l_str.data)[l_str.len] = 0;
                    } else {
                        (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                        (r_str.data)[r_str.len] = 0;
                    }
                    count--;
                    i++;
                } else {
                    if (!flag) {
                        (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                        (l_str.data)[l_str.len] = 0;
                    } else {
                        (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                        (r_str.data)[r_str.len] = 0;
                    }
                    i++;
                }
            } while (count);
        } else {
            if (!flag) {
                (l_str.data)[(l_str.len)++] = (tree_string.data)[i];
                (l_str.data)[l_str.len] = 0;
            } else {
                (r_str.data)[(r_str.len)++] = (tree_string.data)[i];
                (r_str.data)[r_str.len] = 0;
            }
            i++;
        }

        flag = true;
    }

    leaf *l = malloc(sizeof(leaf));

    l->left = decompile_tree(l_str);
    l->right = decompile_tree(r_str);
    l->weight = 1;
    l->symbol = 0;

    return l;
}
