#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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
void encode(const char *input_path, const char *output_path);
void create_table(leaf *root, character_code (*table)[], character_code accum);
void decode(const char *input_path, const char *output_path);
string compile_tree(leaf *root);
leaf *decompile_tree(string tree_string);

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Incorrect usage.\n");
        exit(1);
    }

    if (strcmp("enc", argv[1]) == 0) {
        encode(argv[2], argv[3]);
    } else if (strcmp("dec", argv[1]) == 0) {
        decode(argv[2], argv[3]);
    } else {
        printf("Incorrect usage.\n");
        exit(1);
    }

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

void encode(const char *input_path, const char *output_path) {
    int input_char;
    char8_t encoded_char;
    uint8_t offset, encoded_char_pos;
    character_code table[256] = { 0 };

    FILE *input_file = fopen(input_path, "rb");
    FILE *output_file = fopen(output_path, "a");

    if (input_file == NULL || output_file == NULL) {
        printf("Can't open files.\n");
        exit(1);
    }

    leaf **leaves;
    uint32_t leaves_count = get_leaves(&leaves, input_path);
    leaf *root = create_tree(leaves, leaves_count);

    character_code accum;
    create_table(root, &table, accum);

    /* <tree string size 4 bytes><tree string n bytes><offset 1 byte><file contents> */

    string s = compile_tree(root);
    offset = (8 - (s.len % 8)) % 8;

    fwrite(&(s.len), 4, 1, output_file);
    fprintf(output_file, "%s", s.data);
    fwrite(&offset, 1, 1, output_file);

    encoded_char = 0;
    encoded_char_pos = offset;

    input_char = fgetc(input_file);
    while (input_char != EOF) {
        if (input_char == '0') {
            switch (encoded_char_pos) {
                case 0:
                    encoded_char &= 0b01111111;
                    break;
                case 1:
                    encoded_char &= 0b10111111;
                    break;
                case 2:
                    encoded_char &= 0b11011111;
                    break;
                case 3:
                    encoded_char &= 0b11101111;
                    break;
                case 4:
                    encoded_char &= 0b11110111;
                    break;
                case 5:
                    encoded_char &= 0b11111011;
                    break;
                case 6:
                    encoded_char &= 0b11111101;
                    break;
                case 7:
                    encoded_char &= 0b11111110;
                    break;
            }
        } else if (input_char == '1') {
            switch (encoded_char_pos) {
                case 0:
                    encoded_char |= 0b10000000;
                    break;
                case 1:
                    encoded_char |= 0b01000000;
                    break;
                case 2:
                    encoded_char |= 0b00100000;
                    break;
                case 3:
                    encoded_char |= 0b00010000;
                    break;
                case 4:
                    encoded_char |= 0b00001000;
                    break;
                case 5:
                    encoded_char |= 0b00000100;
                    break;
                case 6:
                    encoded_char |= 0b00000010;
                    break;
                case 7:
                    encoded_char |= 0b00000001;
                    break;
            }
        }
        encoded_char_pos++;

        if (encoded_char_pos == 8) {
            encoded_char_pos = 0;
            fwrite(&encoded_char, 1, 1, output_file);
            encoded_char = 0;
        }

        input_char = fgetc(input_file);
    }

    fclose(input_file);
    fclose(output_file);
}

void decode(const char *input_path, const char *output_path) {
    int input_file_char;
    FILE *input_file = fopen(input_path, "rb");
    FILE *output_file = fopen(output_path, "a");

    uint8_t offset, encoded_char_pos;
    uint32_t tree_string_size;
    string tree_string;

    /* <tree string size 4 bytes><tree string n bytes><offset 1 byte><file contents> */

    if (input_file == NULL || output_file == NULL) {
        printf("Can't open files.\n");
        exit(1);
    }

    fread(&tree_string_size, 4, 1, input_file);
    fread(tree_string.data, tree_string_size, 1, input_file);
    tree_string.len = tree_string_size;
    fread(&offset, 1, 1, input_file);

    leaf *root = decompile_tree(tree_string);
    leaf *leaf_ptr = root;

    encoded_char_pos = offset;

    do {
        input_file_char = fgetc(input_file);

        while (encoded_char_pos < 8) {
            if (leaf_ptr->left == NULL && leaf_ptr->right == NULL) {
                fputc(leaf_ptr->symbol, output_file);
                leaf_ptr = root;
                continue;
            }

            uint8_t mask = 0b10000000;
            mask >>= encoded_char_pos;

            if (input_file_char & mask) {
                leaf_ptr = leaf_ptr->right;
            } else {
                leaf_ptr = leaf_ptr->left;
            }

            encoded_char_pos++;
        }
        encoded_char_pos = 0;
    } while (input_file_char != EOF);

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
