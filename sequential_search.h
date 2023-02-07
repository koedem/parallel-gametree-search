#pragma once

#include "chess.hpp"
#include "transposition_table.h"

template<bool Q_SEARCH>
class Search {

private:
    Board board;
    uint64_t nodes = 0;
    Transposition_Table& tt;

    /**
     *
     * @param move Should be NO_Move, will contain the TT move if existing.
     * @param alpha Bound passed in by reference, will be updated
     * @param beta  Bound passed in by reference, will be updated
     * @param depth
     * @return true if the TT probe produced a cutoff, i.e. the search can be skipped, and the TT entry value,
     * stored in alpha, can be returned.
     */
    bool tt_probe(Move& move, int& alpha, int& beta, int depth) {
        if (tt.contains(board.hashKey, depth)) {
            TT_Info tt_entry = tt.at(board.hashKey, depth);
            assert(tt_entry.depth == depth);
            if (tt_entry.type == EXACT) {
                alpha = tt_entry.eval;
                return true;
            }
            if (tt_entry.type == UPPER_BOUND) {
                beta = std::min(beta, tt_entry.eval);
            } else if (tt_entry.type == LOWER_BOUND) {
                alpha = std::max(alpha, tt_entry.eval);
            }

            if (alpha >= beta) { // Our window is empty due to the TT hit
                alpha = tt_entry.eval;
                return true;
            }
            move = tt_entry.move;
        }
        if (move == NO_MOVE) { // If we didn't find a TT move, try from one depth earlier instead
            if (tt.contains(board.hashKey, depth - 1)) {
                TT_Info tt_entry = tt.at(board.hashKey, depth - 1);
                assert(tt_entry.depth == depth - 1);
                move = tt_entry.move;
            }
        }
        return false;
    }

public:
    explicit Search(Board& board, Transposition_Table& table) : board(board), tt(table) {
    }

    int q_search(int alpha, int beta) {
        int q_eval = board.eval();
        nodes++;
        if constexpr (!Q_SEARCH) {
            return q_eval;
        }

        if (q_eval >= beta) {
            return q_eval;
        }
        if (q_eval > alpha) {
            alpha = q_eval;
        }

        Movelist captures;
        Movegen::legalmoves<CAPTURE>(board, captures);
        for (auto& capture : captures) {
            board.makeMove(capture.move);
            int inner_eval = -q_search(-beta, -alpha);
            board.unmakeMove(capture.move);
            if (inner_eval > q_eval) {
                q_eval = inner_eval;
                if (q_eval >= beta) {
                    break;
                }
                if (q_eval > alpha) {
                    alpha = q_eval;
                }
            }
        }

        return q_eval;
    }

    int nw_q_search(int beta) {
        int q_eval = board.eval();
        nodes++;
        if constexpr (!Q_SEARCH) {
            return q_eval;
        }

        if (q_eval >= beta) {
            return q_eval;
        }

        Movelist captures;
        Movegen::legalmoves<CAPTURE>(board, captures);
        for (auto& capture : captures) {
            board.makeMove(capture.move);
            int inner_eval = -nw_q_search(-beta + 1);
            board.unmakeMove(capture.move);
            if (inner_eval > q_eval) {
                q_eval = inner_eval;
                if (q_eval >= beta) {
                    break;
                }
            }
        }

        return q_eval;
    }

    int null_window_search(int beta, int depth) {
        int eval = INT32_MIN / 2;
        Move tt_move = NO_MOVE;
        int alpha = beta - 1;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        TT_Info entry{eval, NO_MOVE, (int8_t) depth, UPPER_BOUND};
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }
        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -null_window_search(-beta + 1, depth - 1);
            } else {
                inner_eval = -nw_q_search(-beta + 1);
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move.move;
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
            }
        }
        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    int pv_search(int alpha, int beta, int depth) {
        int eval = INT32_MIN / 2;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        TT_Info entry{eval, NO_MOVE, (int8_t) depth, UPPER_BOUND};
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        bool search_full_window = true;
        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha){
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                search_full_window = false;
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move.move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

            }
        }
        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    int nega_max(int alpha, int beta, int depth) {
        int eval = INT32_MIN / 2;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        TT_Info entry{eval, NO_MOVE, (int8_t) depth, UPPER_BOUND};
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);

        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -q_search(-beta, -alpha);
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move.move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

            }
        }
        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    template<class Search_Result, bool PV_Search>
    Search_Result root_max(int alpha, int beta, int depth, Search_Result& result) {
        auto start = std::chrono::high_resolution_clock::now();
        nodes = 0;
        assert(depth > 0);
        int eval = INT32_MIN / 2;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // This can probably never happen but maybe in parallel search
            return Search_Result{0, 0, tt_move, (int16_t) alpha, (uint16_t) depth};
        }

        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        Move best_move = NO_MOVE;

        bool search_full_window = true;
        for (auto& move_container : moves) {
            auto move = move_container.move;
            board.makeMove(move);
            int inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if constexpr (!PV_Search) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha){
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                //std::cout << convertMoveToUci(move) << " eval " << inner_eval << " nodes " << nodes << std::endl;
                search_full_window = false;
            }
            std::cout << convertMoveToUci(move) << " ";
            tt.print_pv(board, depth - 1);
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                best_move = move;
                if (eval >= beta) {
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                }
            }
        }
        tt.emplace(board.hashKey, {eval, best_move, (int8_t) depth, EXACT}, depth);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        result.nodes = nodes;
        result.duration = duration.count();
        result.move = best_move;
        result.eval = eval;
        result.depth = depth;
        return result;
    }
};