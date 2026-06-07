#include "hashtable.hpp"

#include <cassert>
#include <cstdlib>

namespace {
    constexpr std::size_t kRehashingWork = 128;
    constexpr std::size_t kMaxLoadFactor = 8;

    void hInit(HTab* htab, std::size_t n) {
        assert(n > 0);
        assert((n & (n - 1)) == 0); // n must be power of two

        htab->tab = static_cast<HNode**>(std::calloc(n, sizeof(HNode*)));
        assert(htab->tab != nullptr);
        htab->mask = n - 1;
        htab->size = 0;
    }

    void hInsert(HTab* htab, HNode* node) {
        const std::size_t pos = node->hcode & htab->mask;
        node->next = htab->tab[pos];
        htab->tab[pos] = node;
        ++htab->size;
    }

    // Return the address of the pointer that owns the found node.
    // This makes delete O(1) after lookup because we can rewrite that pointer.
    HNode** hLookup(HTab* htab, HNode* key, HNodeEqual eq) {
        if (!htab->tab) {
            return nullptr;
        }

        const std::size_t pos = key->hcode & htab->mask;
        HNode** from = &htab->tab[pos];

        for (HNode* current; (current = *from) != nullptr; from = &current->next) {
            if (current->hcode == key->hcode && eq(current, key)) {
                return from;
            }
        }

        return nullptr;
    }

    HNode* hDetach(HTab* htab, HNode** from) {
        HNode* node = *from;
        *from = node->next;
        node->next = nullptr;
        --htab->size;
        return node;
    }

    void hmHelpRehashing(HMap* hmap) {
        std::size_t workDone = 0;

        while (workDone < kRehashingWork && hmap->older.size > 0) {
            HNode** from = &hmap->older.tab[hmap->migrate_pos];

            if (!*from) {
                ++hmap->migrate_pos;
                continue;
            }

            hInsert(&hmap->newer, hDetach(&hmap->older, from));
            ++workDone;
        }

        if (hmap->older.size == 0 && hmap->older.tab) {
            std::free(hmap->older.tab);
            hmap->older = HTab{};
        }
    }

    void hmTriggerRehashing(HMap* hmap) {
        assert(hmap->older.tab == nullptr);

        hmap->older = hmap->newer;
        hInit(&hmap->newer, (hmap->newer.mask + 1) * 2);
        hmap->migrate_pos = 0;
    }
}

HNode* hm_lookup(HMap* hmap, HNode* key, HNodeEqual eq) {
    hmHelpRehashing(hmap);

    HNode** from = hLookup(&hmap->newer, key, eq);
    if (!from) {
        from = hLookup(&hmap->older, key, eq);
    }

    return from ? *from : nullptr;
}

void hm_insert(HMap* hmap, HNode* node) {
    if (!hmap->newer.tab) {
        hInit(&hmap->newer, 4);
    }

    hInsert(&hmap->newer, node);

    if (!hmap->older.tab) {
        const std::size_t threshold = (hmap->newer.mask + 1) * kMaxLoadFactor;
        if (hmap->newer.size >= threshold) {
            hmTriggerRehashing(hmap);
        }
    }

    hmHelpRehashing(hmap);
}

HNode* hm_delete(HMap* hmap, HNode* key, HNodeEqual eq) {
    hmHelpRehashing(hmap);

    if (HNode** from = hLookup(&hmap->newer, key, eq)) {
        return hDetach(&hmap->newer, from);
    }

    if (HNode** from = hLookup(&hmap->older, key, eq)) {
        return hDetach(&hmap->older, from);
    }

    return nullptr;
}

void hm_clear(HMap* hmap) {
    std::free(hmap->newer.tab);
    std::free(hmap->older.tab);
    *hmap = HMap{};
}

std::size_t hm_size(HMap* hmap) {
    return hmap->newer.size + hmap->older.size;
}

void hm_foreach(HMap* hmap, HNodeCallback callback, void* arg) {
    hmHelpRehashing(hmap);

    auto foreachTable = [&](HTab* htab) -> bool {
        if (!htab->tab) {
            return true;
        }

        const std::size_t capacity = htab->mask + 1;

        for (std::size_t i = 0; i < capacity; ++i) {
            for (HNode* node = htab->tab[i]; node; node = node->next) {
                if (!callback(node, arg)) {
                    return false;
                }
            }
        }

        return true;
    };

    if (!foreachTable(&hmap->newer)) {
        return;
    }

    foreachTable(&hmap->older);
}