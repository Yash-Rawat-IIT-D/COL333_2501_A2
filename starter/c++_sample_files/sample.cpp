#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <queue>
#include <set>
#include <utility>
#include <algorithm>

namespace py = pybind11;

/*
=========================================================
 STUDENT AGENT FOR STONES & RIVERS GAME (UPDATED)
---------------------------------------------------------
 Implements move generation to mirror the authoritative
 logic in gameEngine.py:
  - River flow via BFS across connected rivers
  - Valid targets for move/push (stone & river rules)
  - Flip (stone↔river) with flow safety check
  - Rotate (river) with flow safety check
=========================================================
*/

struct Move {
    std::string action;
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> pushed_to;
    std::string orientation;
};

class StudentAgent {
public:
    explicit StudentAgent(std::string side)
        : side_(std::move(side)), gen_(rd_()) {}

    Move choose(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                int rows, int cols,
                const std::vector<int>& score_cols,
                float /*current_player_time*/, float /*opponent_time*/) {
        // Generate all legal moves using the authoritative rules.
        std::vector<Move> moves;

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty()) continue;
                if (getOwner(cell) != side_) continue;

                // Per-piece targets (moves + pushes)
                auto info = compute_valid_targets(board, x, y, side_, rows, cols, score_cols);

                // Add moves
                for (const auto& d : info.moves) {
                    moves.push_back({"move", {x, y}, {d.first, d.second}, {}, ""});
                }
                // Add pushes
                for (const auto& pr : info.pushes) {
                    auto of = pr.first;   // own-final (where mover lands)
                    auto pf = pr.second;  // pushed_to (where target goes)
                    moves.push_back({"push", {x, y}, {of.first, of.second}, {pf.first, pf.second}, ""});
                }

                // Flip / Rotate
                if (isStone(cell)) {
                    // Try both orientations if safe
                    for (const std::string& ori : {"horizontal", "vertical"}) {
                        if (flip_safe(board, x, y, side_, rows, cols, score_cols, ori)) {
                            moves.push_back({"flip", {x, y}, {x, y}, {}, ori});
                        }
                    }
                } else { // river
                    // Flip river -> stone always allowed
                    moves.push_back({"flip", {x, y}, {x, y}, {}, ""});

                    // Rotate if safe
                    const std::string new_ori = (getOrientation(cell) == "horizontal" ? "vertical" : "horizontal");
                    if (rotate_safe(board, x, y, side_, rows, cols, score_cols, new_ori)) {
                        moves.push_back({"rotate", {x, y}, {x, y}, {}, ""});
                    }
                }
            }
        }

        // Fallback
        if (moves.empty()) {
            return {"move", {0, 0}, {0, 0}, {}, ""};
        }

        // Random pick among legal moves (policy placeholder)
        std::uniform_int_distribution<> dist(0, static_cast<int>(moves.size()) - 1);
        return moves[dist(gen_)];
    }

private:
    // ---------- Board helpers ----------
    static inline bool in_bounds(int x, int y, int rows, int cols) {
        return 0 <= x && x < cols && 0 <= y && y < rows;
    }

    static inline std::string opponent_of(const std::string& p) {
        return (p == "circle" ? "square" : "circle");
    }

    static inline int top_score_row() { return 2; }
    static inline int bottom_score_row(int rows) { return rows - 3; }

    static bool in_vector(const std::vector<int>& v, int x) {
        return std::find(v.begin(), v.end(), x) != v.end();
    }

    static bool is_opponent_score_cell(int x, int y, const std::string& player,
                                       int rows, int cols, const std::vector<int>& score_cols) {
        if (player == "circle") {
            return (y == bottom_score_row(rows)) && in_vector(score_cols, x);
        } else {
            return (y == top_score_row()) && in_vector(score_cols, x);
        }
    }

    static const std::string& getFieldOrDefault(const std::map<std::string, std::string>& m,
                                                const std::string& k,
                                                const std::string& def) {
        auto it = m.find(k);
        if (it == m.end()) return def_ref(def);
        return it->second;
    }

    static const std::string& def_ref(const std::string& s) {
        static thread_local std::string tmp;
        tmp = s;
        return tmp;
    }

    static std::string getOwner(const std::map<std::string, std::string>& cell) {
        static const std::string empty = "";
        const auto& v = getFieldOrDefault(cell, "owner", empty);
        return v.empty() ? empty : v;
    }

    static std::string getSide(const std::map<std::string, std::string>& cell) {
        static const std::string stone = "stone";
        const auto& v = getFieldOrDefault(cell, "side", stone);
        return v.empty() ? stone : v;
    }

    static std::string getOrientation(const std::map<std::string, std::string>& cell) {
        static const std::string horiz = "horizontal";
        const auto& v = getFieldOrDefault(cell, "orientation", horiz);
        return v.empty() ? horiz : v;
    }

    static bool isStone(const std::map<std::string, std::string>& cell) {
        return !cell.empty() && getSide(cell) == "stone";
    }

    static bool isRiver(const std::map<std::string, std::string>& cell) {
        return !cell.empty() && getSide(cell) == "river";
    }

    // ---------- River flow (authoritative) ----------
    using Pos = std::pair<int, int>;

    static std::vector<Pos> get_river_flow_destinations(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int rx, int ry, int sx, int sy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols, bool river_push = false
    ) {
        std::vector<Pos> destinations;
        std::set<Pos> visited;
        std::queue<Pos> q;
        q.push({rx, ry});

        auto at = [&](int x, int y) -> const std::map<std::string, std::string>& {
            return board[y][x];
        };

        while (!q.empty()) {
            auto [x, y] = q.front(); q.pop();
            if (!in_bounds(x, y, rows, cols)) continue;
            if (visited.count({x, y})) continue;
            visited.insert({x, y});

            std::map<std::string, std::string> cell = at(x, y);
            if (river_push && x == rx && y == ry) {
                cell = at(sx, sy); // treat entry cell as the mover for push rules
            }

            if (cell.empty()) {
                if (!is_opponent_score_cell(x, y, player, rows, cols, score_cols)) {
                    destinations.push_back({x, y});
                }
                continue;
            }

            if (!isRiver(cell)) continue;

            // flow directions from river orientation
            std::vector<Pos> dirs = (getOrientation(cell) == "horizontal")
                                        ? std::vector<Pos>{{1, 0}, {-1, 0}}
                                        : std::vector<Pos>{{0, 1}, {0, -1}};
            for (auto [dx, dy] : dirs) {
                int nx = x + dx, ny = y + dy;
                while (in_bounds(nx, ny, rows, cols)) {
                    if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) break;

                    const auto& next = at(nx, ny);
                    if (next.empty()) {
                        destinations.push_back({nx, ny});
                        nx += dx; ny += dy;
                        continue;
                    }
                    if (nx == sx && ny == sy) {
                        nx += dx; ny += dy;
                        continue;
                    }
                    if (isRiver(next)) {
                        q.push({nx, ny});
                        break;
                    }
                    break;
                }
            }
        }

        // unique
        std::vector<Pos> out;
        std::set<Pos> seen;
        for (const auto& d : destinations) {
            if (!seen.count(d)) { seen.insert(d); out.push_back(d); }
        }
        return out;
    }

    // ---------- Valid targets (authoritative) ----------
    struct Targets {
        std::set<Pos> moves;
        std::vector<std::pair<Pos, Pos>> pushes; // ((own_final), (pushed_to))
    };

    static Targets compute_valid_targets(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int sx, int sy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols
    ) {
        Targets tgs;
        if (!in_bounds(sx, sy, rows, cols)) return tgs;

        const auto& p = board[sy][sx];
        if (p.empty() || getOwner(p) != player) return tgs;

        std::vector<Pos> dirs = {{1,0},{-1,0},{0,1},{0,-1}};

        for (auto [dx, dy] : dirs) {
            int tx = sx + dx, ty = sy + dy;
            if (!in_bounds(tx, ty, rows, cols)) continue;

            if (is_opponent_score_cell(tx, ty, player, rows, cols, score_cols)) continue;

            const auto& target = board[ty][tx];

            if (target.empty()) {
                tgs.moves.insert({tx, ty});
            } else if (isRiver(target)) {
                auto flow = get_river_flow_destinations(board, tx, ty, sx, sy, player,
                                                        rows, cols, score_cols, false);
                for (const auto& d : flow) tgs.moves.insert(d);
            } else {
                // target is a stone
                if (isStone(p)) {
                    int px = tx + dx, py = ty + dy;
                    if (in_bounds(px, py, rows, cols) &&
                        board[py][px].empty() &&
                        !is_opponent_score_cell(px, py, getOwner(p), rows, cols, score_cols)) {
                        tgs.pushes.push_back({{tx, ty}, {px, py}});
                    }
                } else {
                    // mover is a river: river-push logic
                    std::string pushed_player = getOwner(target);
                    auto flow = get_river_flow_destinations(board, tx, ty, sx, sy, pushed_player,
                                                            rows, cols, score_cols, true);
                    for (const auto& d : flow) {
                        if (!is_opponent_score_cell(d.first, d.second, pushed_player, rows, cols, score_cols)) {
                            tgs.pushes.push_back({{tx, ty}, d});
                        }
                    }
                }
            }
        }
        return tgs;
    }

    // ---------- Flip/Rotate safety checks ----------
    static bool flip_safe(
        std::vector<std::vector<std::map<std::string, std::string>>> board,
        int fx, int fy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols, const std::string& ori
    ) {
        auto& piece = board[fy][fx];
        if (piece.empty() || getOwner(piece) != player) return false;
        if (!isStone(piece)) return false;

        // Temporarily make it a river and test flow
        piece["side"] = "river";
        piece["orientation"] = ori;

        auto flow = get_river_flow_destinations(board, fx, fy, fx, fy, player,
                                                rows, cols, score_cols, false);
        for (const auto& d : flow) {
            if (is_opponent_score_cell(d.first, d.second, player, rows, cols, score_cols)) {
                return false;
            }
        }
        return true;
    }

    static bool rotate_safe(
        std::vector<std::vector<std::map<std::string, std::string>>> board,
        int fx, int fy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols, const std::string& new_ori
    ) {
        auto& piece = board[fy][fx];
        if (piece.empty() || getOwner(piece) != player) return false;
        if (!isRiver(piece)) return false;

        piece["orientation"] = new_ori;
        auto flow = get_river_flow_destinations(board, fx, fy, fx, fy, player,
                                                rows, cols, score_cols, false);
        for (const auto& d : flow) {
            if (is_opponent_score_cell(d.first, d.second, player, rows, cols, score_cols)) {
                return false;
            }
        }
        return true;
    }

private:
    std::string side_;
    std::random_device rd_;
    std::mt19937 gen_;
};

// ---- PyBind11 bindings ----
PYBIND11_MODULE(student_agent_module, m) {
    py::class_<Move>(m, "Move")
        .def_readonly("action", &Move::action)
        .def_readonly("from_pos", &Move::from)
        .def_readonly("to_pos", &Move::to)
        .def_readonly("pushed_to", &Move::pushed_to)
        .def_readonly("orientation", &Move::orientation);

    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<std::string>())
        .def("choose", &StudentAgent::choose);
}
