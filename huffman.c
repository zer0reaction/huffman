#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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

void sort_leaves(leaf **leaves, uint32_t len);
void sort_string(char8_t *str, uint32_t len);
uint32_t get_leaves(leaf ***l, const char *path);
void print_leaves(leaf **leaves, uint32_t leaves_count);
leaf *create_tree(leaf **leaves, uint32_t leaves_count);
void display_all_leaves(leaf *root);
void encode(const char *input_path, const char *output_path, leaf *root);
void create_table(leaf *root, character_code (*table)[], character_code accum);

int main(int argc, char **argv) {
    leaf *root;
    leaf **leaves = NULL;
    uint32_t leaves_count;

    if (argc != 3) {
        printf("Invalid use.\n");
        exit(1);
    }

    leaves_count = get_leaves(&leaves, argv[1]);

    root = create_tree(leaves, leaves_count);    
    encode(argv[1], argv[2], root);

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

/* TODO: quick sort */
void sort_string(char8_t *str, uint32_t len) {
    uint32_t i, j;
    char8_t c;

    for (i = 1; i < len; i++) {
        for (j = 0; j < len - i; j++) {
            if (str[j] > str[j + 1]) {
                c = str[j];
                str[j] = str[j + 1];
                str[j + 1] = c;
            }
        }
        printf("Sorting progress: %d / %d\n", i, len - 1);
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
    sort_string(buffer, file_size);

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

void print_leaves(leaf **leaves, uint32_t leaves_count) {
    uint32_t i;

    for (i = 0; i < leaves_count; i++) {
        printf("%d. weight: %d, symbol: %c\n", i, leaves[i]->weight, leaves[i]->symbol);
    }
}

leaf *create_tree(leaf **leaves, uint32_t leaves_count) {
    while (leaves_count > 1) {
        uint32_t i;
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

void display_all_leaves(leaf *root) {
    if (root->symbol != 0) {
        if (root->symbol == '\n') {
            printf("(\\n)");
        } else {
            printf("(%c)", root->symbol);
        } 
        return;
    }

    display_all_leaves(root->left);
    display_all_leaves(root->right);
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
        printf("Encoding progress: %ld / %ld\n", i, file_size);
    }

    fclose(input_file);
    fclose(output_file);
}

void create_table(leaf *root, character_code (*table)[], character_code accum) {
    /* this happens if we are reading binary files */
    if (root == NULL) {
        return;
    }

    if (root->symbol != 0) {
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
