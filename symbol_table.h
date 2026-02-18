#ifndef SYMBOL_TABLEH
#define SYMBOL_TABLEH

#include <stdint.h>
#include <stdbool.h>

#define TABLE_SIZE 127
#define MAX_LINE 512
#define MAX_LABEL 257

typedef struct SymbolEntry {
    char label_name[MAX_LABEL];
    uint64_t address;
    struct SymbolEntry* next;
} SymbolEntry;

typedef struct SymbolTable {
    SymbolEntry* buckets[TABLE_SIZE];
} SymbolTable;

SymbolTable* create_table();
unsigned long hash_label(char *str);
int insert_label(SymbolTable* table, char* name, uint64_t addr);
uint64_t lookup_label(SymbolTable* table, char* name);
void free_table(SymbolTable* table);

#endif