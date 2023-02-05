#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include "chess-library/src/chess.hpp"

enum Bound_Type : uint8_t {
    UPPER_BOUND, LOWER_BOUND, EXACT
};

struct TT_Info {
    int32_t eval;
    Chess::Move move;
    int8_t depth;
    Bound_Type type;

    bool operator<(TT_Info& other) const {
        if (type == EXACT && other.type != EXACT) {
            return false;
        } else if (type != EXACT && other.type == EXACT) {
            return true;
        }
        return depth < other.depth;
    }
};

class Transposition_Table {

private:
    static constexpr uint32_t entries_per_bucket = 4;

    struct Entry {
        uint64_t key;
        TT_Info value;
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };



public:
    Transposition_Table() : table(size) {
    }

    void print_size() const {
        uint64_t num_elements = 0;
        for (const Bucket& bucket : table) {
            for (auto & entry : bucket.entries) {
                if (entry.key != 0) {
                    num_elements++;
                }
            }
        }
        std::cout << "Table elements: " << num_elements << ", missed writes: " << missed_writes << " bucket count "
                  << table.size() << ", bucket capacity: " << table.capacity() << std::endl;
    }

    /*void emplace(uint64_t key, uint64_t value) {
        auto & entries = table[pos(key)].entries;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value || i == 3) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }*/

    void emplace(uint64_t key, TT_Info value, int32_t depth) {
        auto & entries = table[pos(key, depth)].entries;
        for (auto & entry : entries) { // Check if the entry already exists
            if (entry.key == key) {
                assert(entry.value.depth == depth);
                assert(value.depth == depth);
                entry.value = value;
                return;
            }
        }

        bool swapped = false;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
                swapped = true;
            }
        }
        /*if (!swapped) {
            missed_writes++;
            auto & entry = entries[2 + (missed_writes & 1)];
            std::swap(entry.value, value);
            std::swap(entry.key, key);
        }*/
    }

    /*void emplace(uint64_t key, uint64_t value) {
        auto & entries = table[pos(key)].entries;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
        missed_writes += value == 0 ? 0 : 1;
    }*/

    [[nodiscard]] TT_Info at(uint64_t key, int32_t depth) const {
        for (auto& entry : table[pos(key, depth)].entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return TT_Info{};
    }

    bool contains(uint64_t key, int32_t depth) {
        for (auto& entry : table[pos(key, depth)].entries) { // NOLINT(readability-use-anyofallof)
            if (entry.key == key) {
                return true;
            }
        }
        return false;
    }

    void print_pv(Board& board, int depth) {
        Board copy(board);
        while (depth > 0) {
            if (contains(copy.hashKey, depth)) {
                Move move = at(copy.hashKey, depth).move;
                std::cout << convertMoveToUci(move) << " ";
                copy.makeMove(move);
                depth--;
            } else {
                break;
            }
        }
        std::cout << std::endl;
    }

    static inline uint64_t pos(uint64_t key, int32_t depth) {
        return (key + depth) % size; // this is a compile-time constant and gets compiled to either a bit and or an efficient version of this
    }

private:
    static constexpr uint32_t size = 1 << 27;
    std::vector<Bucket> table;

    uint64_t missed_writes = 0;
};