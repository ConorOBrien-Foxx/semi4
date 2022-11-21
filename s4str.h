#ifndef S4STR_INCL
#define S4STR_INCL
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memcpy */

typedef struct s4str {
    unsigned char* data;
    size_t cap;
    size_t size;
} s4str;

#define MIN_CAPACITY    (10)
#define GROW_FACTOR     (2)

s4str* s4str_new(size_t capacity) {
    // min capacity for guesses
    if(capacity < MIN_CAPACITY) {
        capacity = MIN_CAPACITY;
    }
    s4str* res = malloc(sizeof(s4str));
    res->data = calloc(capacity, sizeof(*res->data));
    res->cap = capacity;
    res->size = 0;
    return res;
}

void s4str_free(s4str* str) {
    free(str->data);
    free(str);
}

s4str* s4str_from(const char* str) {
    size_t s = strlen(str);
    s4str* inst = s4str_new(s);
    
    memcpy(inst->data, str, s * sizeof(*str));
    inst->size = s;
    
    return inst;
}

void s4str_growToInclude(s4str* str, int index) {
    if(index < 0) {
        fprintf(stderr, "Index out of bounds\n");
        exit(1);
    }
    if(index >= str->cap) {
        size_t newCap = str->cap;
        while(index >= newCap) {
            newCap *= GROW_FACTOR;
        }
        void* newMem = realloc(str->data, newCap);
        if(newMem == NULL) {
            fprintf(stderr, "Memory allocation failure\n");
            exit(2);
        }
        str->data = newMem;
        for(size_t i = str->cap; i < newCap; i++) {
            str->data[i] = 0;
        }
        str->cap = newCap;
    }
}

void s4str_resize(s4str* str, int index) {
    if(index < str->size) {
        str->data[index] = 0;
        str->size = index;
    }
    else {
        s4str_growToInclude(str, index);
        str->size = index;
    }
}

void s4str_set(s4str* str, int index, int value) {
    s4str_growToInclude(str, index);
    str->data[index] = value;
    if(index >= str->size) {
        str->size = index + 1;
    }
}

void s4str_appendChar(s4str* str, int value) {
    s4str_set(str, str->size, value);
}

void s4str_appendString(s4str* str, s4str* other) {
    s4str_growToInclude(str, str->size + other->size);
    for(size_t i = 0; i < other->size; i++) {
        str->data[str->size + i] = other->data[i];
    }
    str->size += other->size;
}

void s4str_copyTo(s4str* to, s4str* from) {
    free(to->data);
    to->data = calloc(*to->data, from->cap);
    memcpy(to->data, from->data, from->cap);
    to->size = from->size;
    to->cap = from->cap;
}

void s4str_puts(s4str* str) {
    puts((char*) str->data);
}

void s4str_puts_to(s4str* str, FILE* output) {
    fprintf(output, "%s\n", (char*) str->data);
}

unsigned char s4str_get(s4str* str, int index) {
    /*
    if(index < 0 || index >= str->size) {
        fprintf(stderr, "Index out of bounds\n");
        exit(1);
    }
    */
    s4str_growToInclude(str, index);
    return str->data[index];
}

char* fileModeNumber(int no) {
    switch(no) {
        case 1: return "r";
        default:
        case 2: return "w";
        case 3: return "a";
        case 4: return "r+";
        case 5: return "w+";
        case 6: return "a+";
    }
}

#endif