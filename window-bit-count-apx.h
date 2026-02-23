#ifndef _WINDOW_BIT_COUNT_APX_
#define _WINDOW_BIT_COUNT_APX_

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

uint64_t N_MERGES = 0; // keep track of how many bucket merges occur

typedef struct {
    uint64_t timestamp;
    uint64_t size;
    int32_t prev;
    int32_t next;
} Bucket;

typedef struct {
    uint32_t wnd_size;
    uint32_t k;
    uint64_t current_time;
    
    Bucket* buckets;
    int32_t head;
    int32_t tail;
    int32_t free_head;
    
    uint32_t max_buckets;
    uint64_t total_sum;
    
    int32_t last_of_size[64];
    uint32_t count[64];
} StateApx;

// k = 1/eps
// if eps = 0.01 (relative error 1%) then k = 100
// if eps = 0.001 (relative error 0.1%) the k = 1000
uint64_t wnd_bit_count_apx_new(StateApx* self, uint32_t wnd_size, uint32_t k) {
    assert(wnd_size >= 1);
    assert(k >= 1);

    self->wnd_size = wnd_size;
    self->k = k;
    self->current_time = 0;
    self->total_sum = 0;
    
    for (int i = 0; i < 64; i++) {
        self->last_of_size[i] = -1;     // -1 indicates no bucket of this size
        self->count[i] = 0;     // count of buckets of size 2^i
    }
    
    // Calculate max buckets based on the asymptotic bound O(k * log(W))
    // The maximum bucket size is bounded by wnd_size.
    // Since sizes are powers of 2, there are at most floor(log2(wnd_size)) + 1 different sizes.
    // For each size, we can have at most k + 1 buckets (since we merge at k + 2).
    uint32_t max_sizes = (uint32_t)floor(log2((double)wnd_size)) + 1;
    uint64_t max_b = (uint64_t)max_sizes * (k + 2);
    
    if (wnd_size + 1ULL < max_b) {
        max_b = wnd_size + 1ULL; // We never need more buckets than the window size itself
    }
    self->max_buckets = (uint32_t)max_b;
    
    // allocate memory for buckets
    uint64_t memory = ((uint64_t)self->max_buckets) * sizeof(Bucket);
    self->buckets = (Bucket*)malloc(memory);
    assert(self->buckets != NULL);
    
    self->head = -1;
    self->tail = -1;
    self->free_head = 0;
    
    // connect free buckets in a linked list
    for (uint32_t i = 0; i < self->max_buckets; i++) {
        self->buckets[i].next = (i == self->max_buckets - 1) ? -1 : (i + 1);
    }
    
    return memory;
}

void wnd_bit_count_apx_destruct(StateApx* self) {
    free(self->buckets);
    self->buckets = NULL;
}

void wnd_bit_count_apx_print(StateApx* self) {
    printf("StateApx (wnd_size=%u, k=%u, current_time=%llu, total_sum=%llu):\n", 
           self->wnd_size, self->k, self->current_time, self->total_sum);
    
    if (self->head == -1) {
        printf("  [Empty]\n");
        return;
    }

    int32_t curr = self->head;
    int count = 0;
    while (curr != -1) {
        Bucket* b = &self->buckets[curr];
        printf("  Bucket %d: size=%llu, timestamp=%llu\n", count, b->size, b->timestamp);
        curr = b->next;
        count++;
    }
    printf("  Total buckets: %d\n", count);
}

uint32_t wnd_bit_count_apx_next(StateApx* self, bool item) {
    self->current_time++;

    // remove expired buckets
    while (self->tail != -1) {
        Bucket* oldest = &self->buckets[self->tail];
        // check if the oldest bucket is outside the window
        if (self->current_time - oldest->timestamp >= self->wnd_size) {
            self->total_sum -= oldest->size;
            
            int i = __builtin_ctzll(oldest->size); // i such that size = 2^i
            self->count[i]--;
            if (self->count[i] == 0) {
                self->last_of_size[i] = -1;
            } else {
                self->last_of_size[i] = oldest->prev;
            }
            
            // update the next of the previous bucket and the tail pointer
            int32_t prev = oldest->prev;
            if (prev != -1) {
                self->buckets[prev].next = -1;
            } else {
                self->head = -1;
            }
            //update the tail pointer
            int32_t old_tail = self->tail;
            self->tail = prev;
            
            // add the removed bucket to the free list
            self->buckets[old_tail].next = self->free_head;
            self->free_head = old_tail;
        } else {
            break;
        }
    }

    // if item == 1, add a new bucket of size 1 at the head of the list
    if (item) {
        self->total_sum += 1;
        assert(self->free_head != -1);  // this line prevents the algorithm from crashing if we run out of buckets
        int32_t new_idx = self->free_head;
        self->free_head = self->buckets[new_idx].next;
        
        Bucket* new_bucket = &self->buckets[new_idx];
        new_bucket->timestamp = self->current_time;
        new_bucket->size = 1;
        new_bucket->prev = -1;
        new_bucket->next = self->head;
        
        // insert the new bucket at the head of the list
        if (self->head != -1) {
            self->buckets[self->head].prev = new_idx;
        } else {
            self->tail = new_idx;
        }
        self->head = new_idx;
        
        self->count[0]++;
        if (self->last_of_size[0] == -1) {
            self->last_of_size[0] = new_idx;
        }
        
        // merge buckets
        int i = 0;
        while (self->count[i] == self->k + 2) {
            int32_t oldest_idx = self->last_of_size[i];
            int32_t second_oldest_idx = self->buckets[oldest_idx].prev;
            
            Bucket* b_oldest = &self->buckets[oldest_idx];
            Bucket* b_second = &self->buckets[second_oldest_idx];
            
            b_second->size *= 2;
            
            int32_t b_oldest_prev = b_oldest->prev; // this is second_oldest_idx
            int32_t b_oldest_next = b_oldest->next;
            
            b_second->next = b_oldest_next;
            if (b_oldest_next != -1) {
                self->buckets[b_oldest_next].prev = second_oldest_idx; //if b_oldest was not the tail, update next bucket's prev pointer
            } else {
                self->tail = second_oldest_idx; //if b_oldest was the tail, update tail pointer
            }
            
            b_oldest->next = self->free_head;
            self->free_head = oldest_idx;
            
            self->count[i] -= 2;
            if (self->count[i] == 0) { // only k == 1 can cause count[i] to become 0
                self->last_of_size[i] = -1; 
            } else {
                self->last_of_size[i] = b_second->prev;
            }
            
            self->count[i+1]++;
            if (self->last_of_size[i+1] == -1) { // if it is the first bucket of this size, set last_of_size
                self->last_of_size[i+1] = second_oldest_idx;
            }
            
            N_MERGES++;
            i++;
        }
    }

    // return the approximate count of 1s in the last wnd_size items
    if (self->tail == -1) {
        return 0;
    } else {
        return (uint32_t)(self->total_sum - self->buckets[self->tail].size + 1);
    }
}

#endif // _WINDOW_BIT_COUNT_APX_
