#include "SymbolTable.h"
#include <stdlib.h>
#include <string.h>

SymbolTable *createSymbolTable(void) {

    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (table == NULL) {
        return NULL;
    }

    table->first = NULL;

    return table;
}

static bool searchByName(Symbol *current, const char *name) {

    if (current == NULL) {
        return false;
    }

    if (strcmp(current->name, name) == 0) {
        return true;
    }

    return searchByName(current->next, name);
}

bool insertSymbol(SymbolTable *table, const char *name, SymbolType type,
                  void *definition) {

    if (table == NULL || name == NULL) {
        return false;
    }


    if(searchByName(table->first, name)) {
        return false;
    }

    Symbol *newSymbol = malloc(sizeof(Symbol));

    if (newSymbol == NULL) {
        return false;
    }

    newSymbol->name = malloc(strlen(name) + 1);

    if (newSymbol->name == NULL) {
        free(newSymbol);
        return false;
    }

    strcpy(newSymbol->name, name);

    newSymbol->type = type;
    newSymbol->definition = definition;
    newSymbol->next = table->first;

    table->first = newSymbol;

    return true;
}

Symbol *findSymbol(SymbolTable *table, const char *name, SymbolType type){

    if (table == NULL || name == NULL) {
        return NULL;
    }

    Symbol *current = table->first;

    while (current != NULL) {

        if (strcmp(current->name, name) == 0 &&
            current->type == type) {
            return current;
        }

        current = current->next;
    }

    return NULL;
};

static void freeSymbols(Symbol *current) {

    if (current == NULL) {
        return;
    }

    freeSymbols(current->next);

    free(current->name);
    free(current);
}

void freeSymbolTable(SymbolTable *table) {

    if (table == NULL) {
        return;
    }

    freeSymbols(table->first);

    free(table);
}