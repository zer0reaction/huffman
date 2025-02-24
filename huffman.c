#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct leaf {
    struct leaf *left;
    struct leaf *right;
    uint32_t weight;
    char symbol;
} leaf;

void sort_leaves(leaf **leaves, uint32_t len);
void sort_string(char *str, uint32_t len);
uint32_t string_length(const char *str);
uint32_t get_leaves(leaf ***l, const char *path);
void print_leaves(leaf **leaves, uint32_t leaves_count);
leaf *create_tree(leaf **leaves, uint32_t leaves_count);
void display_all_leaves(leaf *root);

int main(int argc, char **argv) {
    leaf *root;
    leaf **leaves = NULL;
    uint32_t leaves_count = get_leaves(&leaves, argv[1]);

    if (argc == 1) {
        printf("No file specified.\n");
        exit(1);
    }

    root = create_tree(leaves, leaves_count);

    display_all_leaves(root);

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

void sort_string(char *str, uint32_t len) {
    uint32_t i, j;
    char c;

    for (i = 1; i < len; i++) {
        for (j = 0; j < len - i; j++) {
            if (str[j] > str[j + 1]) {
                c = str[j];
                str[j] = str[j + 1];
                str[j + 1] = c;
            }
        }
    }
}

uint32_t string_length(const char *str) {
    uint32_t size = 0;

    while (str[size] != '\0') {
        size++;
    }

    return size;
}

uint32_t get_leaves(leaf ***l, const char *path) {
    char chars_count[256] = { 0 };
    uint32_t i, j, leaves_count;
    FILE *file = fopen(path, "r");
    uint64_t file_size;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size == 0) {
        fprintf(stderr, "File is empty.\n");
        fclose(file);
        exit(1);
    }

    char *buffer = (char*) malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory.\n");
        fclose(file);
        exit(1);
    }

    fread(buffer, file_size, 1, file);
    sort_string(buffer, string_length(buffer));

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
