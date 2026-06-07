#pragma once

#include <cstddef>
#include <cstdint>

struct HNode {
    HNode* next{nullptr};
    uint64_t hcode{0};
};

struct HTab { // Hash Table.
    HNode** tab{nullptr}; // Bucket Arrays. - each array contains multiple keys.
    /* 
    To Look up for bucket, you will need to calculate:
    hcode = hash(key)
    bucket = hcode % capacity. Modulo operation slow down the system.
    -> mask = capacity - 1 or (2^n - 1)
    -> Now, bucket = hcode & mask (bit manipulation) -> a faster version of modulo.
    */
    std::size_t mask{0};
    std::size_t size{0}; // total amount of nodes being stored.
};

struct HMap { // Hashmap, which manages progressive re-harshing.
    HTab newer; 
    HTab older;  
    std::size_t migrate_pos{0}; // migrate position - the next bucket in the old table that needs to be migrated.
};

using HNodeEqual = bool (*)(HNode*, HNode*);

HNode* hm_lookup(HMap* hmap, HNode* key, HNodeEqual eq);
void hm_insert(HMap* hmap, HNode* node);
HNode* hm_delete(HMap* hmap, HNode* key, HNodeEqual eq);
void hm_clear(HMap* hmap);
std::size_t hm_size(HMap* hmap);

using HNodeCallback = bool (*)(HNode*, void*);

void hm_foreach(HMap* hmap, HNodeCallback callback, void* arg);