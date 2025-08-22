// Moore Machine
// shared library implementation by Agata Kopec
// March 2025

#include "ma.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define BITS_PER_UINT64 64

typedef struct moore_t_linked_list mt_list;
typedef struct bit_array bit_array_t;

static size_t bit_array_size(size_t bits);

static void identity_output(uint64_t *output,
                            uint64_t const *state, size_t m, size_t s);

static void disconnect_dependent_inputs(moore_t *a);
static void update_connected_inputs(const moore_t *a);

static bool is_on_the_list(mt_list *list, const moore_t *a);
static void remove_from_list(mt_list **list, const moore_t *a);

struct bit_array {
    uint64_t *bits;
    size_t length;
};

// moore machine representation

struct moore {
    
    bit_array_t X;                      // input signal bit array
    bit_array_t Y;                      // output signal bit array
    bit_array_t Q;                      // internal state bit array
    uint64_t const *q;                  // initial state bit array

    transition_function_t t;            // transition function
    output_function_t y;                // output function

    struct {                            // connections records for every input
        moore_t *connected_to;          // pointer to connected machine
        size_t connected_out;           // index of connected output
    } *connections;

    struct moore_t_linked_list {        // list of machines dependent on output
        moore_t *machine;               // dependent machine
        mt_list *next;                  // pointer to next
    } *dependants;
    
};


// Library functions' implementation


// creates a moore machine with custom:
// input, output and state sizes, initial state, transition and output functions
// in case of invalid arguments or memory allocation failure:
// frees all memory, sets errno to EINVAL or ENOMEM and returns NULL
// inputs default to 0s, user must set or connect

moore_t *ma_create_full(
    size_t n,
    size_t m,
    size_t s,
    transition_function_t t,
    output_function_t y,
    uint64_t const *q) {
    
    if (!m || !s || !t || !y || !q) {
        errno = EINVAL;
        return NULL;
    }

    moore_t *new_machine = malloc(sizeof(moore_t));

    if (!new_machine) {
        errno = ENOMEM;
        return NULL;
    }

    new_machine->X.bits = malloc(bit_array_size(n) * sizeof(uint64_t));
    new_machine->Y.bits = malloc(bit_array_size(m) * sizeof(uint64_t));
    new_machine->Q.bits = malloc(bit_array_size(s) * sizeof(uint64_t));
    new_machine->X.length = n;
    new_machine->Y.length = m;
    new_machine->Q.length = s;
    new_machine->t = t;
    new_machine->y = y;
    new_machine->q = q;
    new_machine->connections = malloc(n * sizeof(*new_machine->connections));
    new_machine->dependants = NULL;

    if (new_machine->connections) {
        for (size_t i = 0; i < n; i++) {
            new_machine->connections[i].connected_to = NULL;
            new_machine->connections[i].connected_out = 0;
        }
    }

    if (!new_machine->X.bits ||
        !new_machine->Y.bits ||
        !new_machine->Q.bits ||
        !new_machine->connections) {
        ma_delete(new_machine);
        errno = ENOMEM;
        return NULL;
    }

    memcpy(new_machine->Q.bits, q, bit_array_size(s) * sizeof(uint64_t));
    memset(new_machine->X.bits, 0, bit_array_size(n) * sizeof(uint64_t));
    new_machine->y(new_machine->Y.bits, new_machine->Q.bits, m, s);

    return new_machine;
}

// creates a simple moore machine with custom:
// input and state sizes and transition function,
// output size is equal to state size and output function is an identity
// in case of invalid arguments or memory allocation failure:
// frees all memory, sets errno to EINVAL or ENOMEM and returns NULL
// inputs default to 0s, user must set or connect

moore_t *ma_create_simple(size_t n, size_t s, transition_function_t t) {
    
    if (!s || !t) {
        errno = EINVAL;
        return NULL;
    }

    moore_t *new_machine = malloc(sizeof(moore_t));

    if (!new_machine) {
        errno = ENOMEM;
        return NULL;
    }

    new_machine->X.bits = malloc(bit_array_size(n) * sizeof(uint64_t));
    new_machine->Y.bits = malloc(bit_array_size(s) * sizeof(uint64_t));
    new_machine->Q.bits = malloc(bit_array_size(s) * sizeof(uint64_t));
    new_machine->X.length = n;
    new_machine->Y.length = s;
    new_machine->Q.length = s;
    new_machine->t = t;
    new_machine->y = identity_output;
    new_machine->q = NULL;
    new_machine->connections = malloc(n * sizeof(*new_machine->connections));
    new_machine->dependants = NULL;

    if (new_machine->connections) {
        for (size_t i = 0; i < n; i++) {
            new_machine->connections[i].connected_to = NULL;
            new_machine->connections[i].connected_out = 0;
        }
    }

    if (!new_machine->X.bits ||
        !new_machine->Y.bits ||
        !new_machine->Q.bits ||
        !new_machine->connections) {
        ma_delete(new_machine);
        errno = ENOMEM;
        return NULL;
    }

    memset(new_machine->Q.bits, 0, bit_array_size(s) * sizeof(uint64_t));
    memset(new_machine->X.bits, 0, bit_array_size(n) * sizeof(uint64_t));
    new_machine->y(new_machine->Y.bits, new_machine->Q.bits, s, s);

    return new_machine;
}

// deletes moore machine a:
// disconnects both ends of a's connections
// - both inputs and outputs
// frees all memory
// does not do anything for NULL arguments

void ma_delete(moore_t *a) {
    
    if (!a) return;

    if (a->connections) ma_disconnect(a, 0, a->X.length);
    disconnect_dependent_inputs(a);

    free(a->X.bits);
    free(a->Y.bits);
    free(a->Q.bits);
    free(a->connections);
    free(a);
    
}

// connects a_in inputs to a_out outputs
// saves a_in in a_out's dependants' list
// disconnects already connected inputs
// in case of invalid arguments or memory allocation failure:
// returns -1 and sets errno to EINVAL or ENOMEM
// otherwise returns 0

int ma_connect(
    moore_t *a_in,
    size_t in,
    moore_t *a_out,
    size_t out,
    size_t num) {
    
    if (!num ||
        !a_in ||
        !a_out ||
        in + num - 1 >= a_in->X.length ||
        out + num - 1 >= a_out->Y.length) {
        errno = EINVAL;
        return -1;
    }

    mt_list *new_dependant = malloc(sizeof(mt_list));
    if (!new_dependant) {
        errno = ENOMEM;
        return -1;
    }
    new_dependant->machine = a_in;
    new_dependant->next = NULL;

    ma_disconnect(a_in, in, num);

    if (is_on_the_list(a_out->dependants, a_in)) {
        
        free(new_dependant);
        
    } else {
        
        new_dependant->machine = a_in;
        new_dependant->next = a_out->dependants;
        a_out->dependants = new_dependant;
        
    }

    for (size_t i = in; i < in + num; i++) {
        
        a_in->connections[i].connected_to = a_out;
        a_in->connections[i].connected_out = out + i - in;
        
    }

    return 0;
}

// disconnects a_in's in to in + num inputs if connected
// removes a_in from soon to be fully disconnected hosts' records
// in case of invalid arguments returns -1 and sets errno to EINVAL
// otherwise returns 0

int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    
    if (!num ||
        !a_in ||
        in + num - 1 >= a_in->X.length) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = in; i < in + num; i++) {
        
        if (a_in->connections[i].connected_to) {
            
            bool remove_a_in_from_dependants = true;

            for (size_t j = 0; j < in; j++) {
                
                if (a_in->connections[j].connected_to ==
                    a_in->connections[i].connected_to) {
                    remove_a_in_from_dependants = false;
                }
                
            }
            
            for (size_t j = in + num; j < a_in->X.length; j++) {
                
                if (a_in->connections[j].connected_to ==
                    a_in->connections[i].connected_to) {
                    remove_a_in_from_dependants = false;
                }
                
            }

            if (remove_a_in_from_dependants) {
                remove_from_list(
                    &a_in->connections[i].connected_to->dependants,
                    a_in);
            }

            a_in->connections[i].connected_to = NULL;
            a_in->connections[i].connected_out = 0;
            
        }
        
    }

    return 0;
}

// sets unconnected machine a's inputs to given bits, ignores connected
// in case of invalid arguments sets errno to EINVAL and returns -1
// otherwise returns 0

int ma_set_input(moore_t *a, uint64_t const *input) {
    
    if (!a || !input || !a->X.length) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < a->X.length; i++) {
        
        if (!a->connections[i].connected_to) {
            
            const size_t chunk = i / BITS_PER_UINT64;
            const size_t bit = i % BITS_PER_UINT64;
            
            if (input[chunk] & 1ULL << bit) {
                a->X.bits[chunk] |= 1ULL << bit;
            } else {
                a->X.bits[chunk] &= ~(1ULL << bit);
            }
            
        }
        
    }

    return 0;
}

// sets machine a's state to given bits
// calls output function immediately to update the output bits
// in case of invalid arguments sets errno to EINVAL and returns -1
// otherwise returns 0

int ma_set_state(moore_t *a, uint64_t const *state) {
    
    if (!a || !state) {
        errno = EINVAL;
        return -1;
    }

    memcpy(a->Q.bits, state, bit_array_size(a->Q.length) * sizeof(uint64_t));

    a->y(a->Y.bits, a->Q.bits, a->Y.length, a->Q.length);

    return 0;
}

// returns machine a's output
// in case of invalid arguments returns NULL and sets errno to EINVAL

uint64_t const *ma_get_output(moore_t const *a) {
    
    if (!a) {
        errno = EINVAL;
        return NULL;
    }

    return a->Y.bits;
}

// performs one computation step for num machines in at[]
// firstly it allocates all memory needed to ensure no errors while computing
// then, in order to ensure that machine work simultaneously
// it's divided into three loops:
// the first one updates connected input's values and calls transition function
// the second updates just computed steps
// the third one calls output functions and updates outputs
// in case of invalid arguments or memory allocation failure
// sets errno to EINVAL or ENOMEM and returns -1
// otherwise returns 0

int ma_step(moore_t *at[], size_t num) {
    
    if (!at || !num) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        if (at[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }

    uint64_t **next_states = malloc(num * sizeof(uint64_t *));

    if (!next_states) {
        errno = ENOMEM;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        
        next_states[i] = malloc(bit_array_size(at[i]->Q.length) * sizeof(uint64_t));

        if (!next_states[i]) {
            
            for (size_t j = 0; j < i; j++) {
                free(next_states[j]);
            }

            free(next_states);
            errno = ENOMEM;
            return -1;
            
        }
        
    }

    for (size_t i = 0; i < num; i++) {
        
        update_connected_inputs(at[i]);

        at[i]->t(next_states[i],
                 at[i]->X.bits,
                 at[i]->Q.bits,
                 at[i]->X.length,
                 at[i]->Q.length);
    }

    for (size_t i = 0; i < num; i++) {
        
        memcpy(at[i]->Q.bits,
               next_states[i],
               bit_array_size(at[i]->Q.length) * sizeof(uint64_t));

        free(next_states[i]);
    }

    free(next_states);

    for (size_t i = 0; i < num; i++) {
        
        at[i]->y(at[i]->Y.bits,
                 at[i]->Q.bits,
                 at[i]->Y.length,
                 at[i]->Q.length);
    }

    return 0;
}


// Helper functions' implementation


// calculates 64-bit pieces needed for given bits

static size_t bit_array_size(size_t bits) {
    
    assert(bits < SIZE_MAX - (BITS_PER_UINT64 - 1));
    return (bits + BITS_PER_UINT64 - 1) / BITS_PER_UINT64;
    
}

// identity output function
// needed to define a simple machine in ma_create_simple

static void identity_output(uint64_t *output,
                            uint64_t const *state,
                            size_t m,
                            size_t s) {
    assert(m == s);
    memcpy(output, state, bit_array_size(m) * sizeof(uint64_t));
}

// ensures that there is no undefined behaviour after deleting a machine
// by nullifying all connections made to its outputs
// called by ma_delete

static void disconnect_dependent_inputs(moore_t *a) {
    
    mt_list *dependant = a->dependants;

    int counter = 0;

    while (dependant) {
        
        for (size_t i = 0; i < dependant->machine->X.length; i++) {
            
            if (dependant->machine->connections[i].connected_to == a) {
                dependant->machine->connections[i].connected_to = NULL;
                dependant->machine->connections[i].connected_out = 0;
                counter++;
            }
            
        }

        mt_list *buffer = dependant;
        dependant = dependant->next;
        free(buffer);
    }

    a->dependants = NULL;
}

// updates machine a's input bits that are connected to outputs of other machines
// called by ma_step()

static void update_connected_inputs(const moore_t *a) {
    
    for (size_t j = 0; j < a->X.length; j++) {
        
        moore_t *output_machine = a->connections[j].connected_to;

        if (output_machine) {
            
            size_t output_index = a->connections[j].connected_out;

            size_t in_chunk = j / BITS_PER_UINT64;
            size_t in_bit = j % BITS_PER_UINT64;
            size_t out_chunk = output_index / BITS_PER_UINT64;
            size_t out_bit = output_index % BITS_PER_UINT64;

            if (output_machine->Y.bits[out_chunk] & 1ULL << out_bit) {
                a->X.bits[in_chunk] |= 1ULL << in_bit;
            } else {
                a->X.bits[in_chunk] &= ~(1ULL << in_bit);
            }
            
        }
        
    }
    
}

// mt_list implementation functions

// checks whether an element is on the mt_list

static bool is_on_the_list(mt_list *list, const moore_t *a) {
    
    mt_list *pivot = list;
    
    while (pivot) {
        
        if (pivot->machine == a) return true;
        pivot = pivot->next;
        
    }
    
    return false;
}

// removes an element form a mt_list
// if the element is not part of the list then does nothing

static void remove_from_list(mt_list **list, const moore_t *a) {
    
    if (!list || !*list) return;
    if (!is_on_the_list(*list, a)) return;

    mt_list *pivot1 = *list;
    mt_list *pivot2 = pivot1;

    if ((*list)->machine == a) {
        
        *list = (*list)->next;
        free(pivot1);
        
    } else {
        
        while (pivot1) {
            if (pivot1->machine == a) break;
            pivot2 = pivot1;
            pivot1 = pivot1->next;
        }
        
        assert(pivot1 && pivot2);
        pivot2->next = pivot1->next;
        free(pivot1);
        
    }
    
}

