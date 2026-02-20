#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

SymbolTable* create_table() {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    for (int i = 0; i < TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
    }
    return table;
}

//djb2
unsigned long hash_label(char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % TABLE_SIZE;
}

int insert_label(SymbolTable* table, char* name, uint64_t addr) {
    if (lookup_label(table, name) != (uint64_t)-1) return 1;
    unsigned int index = hash_label(name);
    SymbolEntry* new_entry = malloc(sizeof(SymbolEntry));
    
    strncpy(new_entry->label_name, name, 63);
    new_entry->label_name[63] = '\0';
    new_entry->address = addr;
    
    // Insert at head
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;

    return 0;
}

uint64_t lookup_label(SymbolTable* table, char* name) {
    unsigned int index = hash_label(name);
    SymbolEntry* entry = table->buckets[index];
    
    while (entry != NULL) {
        if (strcmp(entry->label_name, name) == 0) {
            return entry->address;
        }
        entry = entry->next;
    }
    // ERROR
    return (uint64_t)-1; 
}

void free_table(SymbolTable* table) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        SymbolEntry* entry = table->buckets[i];
        while (entry != NULL) {
            SymbolEntry* temp = entry;
            entry = entry->next;
            free(temp);
        }
    }
    free(table);
}