#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <functional>
#include <iostream>
#include <climits>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <deque>
#include <unordered_set>
#include <set>
#include<queue>

namespace py = pybind11;

// Forward declarations
struct Move;
const int MAX_DEPTH = 1;

// ==================== AI DECISION LOGGING SYSTEM ====================

enum class LogLevel {
    DEBUG = 0,    // Detailed debug information
    INFO = 1,     // General information
    SEARCH = 2,   // Search-specific information
    DECISION = 3, // Major decision points
    ERROR = 4     // Errors and warnings
};

class AILogger {
private:
    std::string log_directory;
    std::string game_session_id;
    std::ofstream main_log_file;
    std::ofstream moves_log_file;
    std::ofstream search_log_file;
    LogLevel min_log_level;
    int move_number;
    bool logging_enabled;
    
public:
    AILogger() : min_log_level(LogLevel::DEBUG), move_number(0), logging_enabled(true) {
        initializeLogging();
    }
    
    ~AILogger() {
        if (main_log_file.is_open()) main_log_file.close();
        if (moves_log_file.is_open()) moves_log_file.close();
        if (search_log_file.is_open()) search_log_file.close();
    }
    
    void initializeLogging() {
        if (!logging_enabled) return;
        
        // Create logs directory relative to current working directory
        // Try multiple possible locations for logs
        std::vector<std::string> possible_log_dirs = {
            "logs",                                    // Current directory
            "../c++_sample_files/logs",               // From client_server to c++ dir
            "./c++_sample_files/logs"                 // Alternative path
        };
        
        log_directory = "";
        for (const auto& dir : possible_log_dirs) {
            try {
                std::filesystem::create_directories(dir);
                if (std::filesystem::exists(dir)) {
                    log_directory = dir;
                    break;
                }
            } catch (...) {
                continue; // Try next directory
            }
        }
        
        // Fallback to current directory if none worked
        if (log_directory.empty()) {
            log_directory = "logs";
            std::filesystem::create_directories(log_directory);
        }
        
        // Generate unique game session ID
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
        game_session_id = ss.str();
        
        // Open log files
        std::string main_log_path = log_directory + "/game_" + game_session_id + ".log";
        std::string moves_log_path = log_directory + "/moves_" + game_session_id + ".log";
        std::string search_log_path = log_directory + "/search_" + game_session_id + ".log";
        
        main_log_file.open(main_log_path, std::ios::out | std::ios::trunc);
        moves_log_file.open(moves_log_path, std::ios::out | std::ios::trunc);
        search_log_file.open(search_log_path, std::ios::out | std::ios::trunc);
        
        // Write headers
        if (main_log_file.is_open()) {
            main_log_file << "========================================\n";
            main_log_file << "AI AGENT DECISION LOG - Session " << game_session_id << "\n";
            main_log_file << "========================================\n";
            main_log_file << "Timestamp: " << getCurrentTimestamp() << "\n";
            main_log_file << "Log Level: " << static_cast<int>(min_log_level) << "\n\n";
        }
        
        if (moves_log_file.is_open()) {
            moves_log_file << "Move#,Timestamp,Generated,Ordered,Capture,Quiet,Aggressive,BestMove,Score\n";
        }
        
        if (search_log_file.is_open()) {
            search_log_file << "Move#,Depth,Nodes,AlphaCutoffs,BetaCutoffs,TTHits,TTMisses,TimeUsed,NodesPerSec,BestScore\n";
        }
    }
    
    void setLogLevel(LogLevel level) {
        min_log_level = level;
    }
    
    void enableLogging(bool enable) {
        logging_enabled = enable;
    }

    void nextMove() {
        move_number++;
    }
    
    void log(LogLevel level, const std::string& message) {
        if (!logging_enabled || level < min_log_level || !main_log_file.is_open()) return;
        
        std::string level_str;
        switch (level) {
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO: level_str = "INFO"; break;
            case LogLevel::SEARCH: level_str = "SEARCH"; break;
            case LogLevel::DECISION: level_str = "DECISION"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
        }
        
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << "[" << level_str << "] " 
                     << "[Move " << move_number << "] " 
                     << message << "\n";
        main_log_file.flush();
    }
    
    void logMoveGeneration(int total_moves, int ordered_moves, int capture_moves, 
                          int quiet_moves, int aggressive_moves, 
                          const std::string& best_move_desc, float best_score) {
        if (!logging_enabled || !moves_log_file.is_open()) return;
        
        moves_log_file << move_number << ","
                      << getCurrentTimestamp() << ","
                      << total_moves << ","
                      << ordered_moves << ","
                      << capture_moves << ","
                      << quiet_moves << ","
                      << aggressive_moves << ","
                      << "\"" << best_move_desc << "\","
                      << best_score << "\n";
        moves_log_file.flush();
    }
    
    void logSearchResults(int depth, size_t nodes, size_t alpha_cutoffs, size_t beta_cutoffs,
                         size_t tt_hits, size_t tt_misses, float time_used, 
                         float nodes_per_second, float best_score) {
        if (!logging_enabled || !search_log_file.is_open()) return;
        
        search_log_file << move_number << ","
                       << depth << ","
                       << nodes << ","
                       << alpha_cutoffs << ","
                       << beta_cutoffs << ","
                       << tt_hits << ","
                       << tt_misses << ","
                       << std::fixed << std::setprecision(3) << time_used << ","
                       << std::fixed << std::setprecision(0) << nodes_per_second << ","
                       << std::fixed << std::setprecision(2) << best_score << "\n";
        search_log_file.flush();
    }
    
    void logDecision(const std::string& chosen_move, const std::string& reasoning) {
        log(LogLevel::DECISION, "FINAL DECISION: " + chosen_move + " | Reasoning: " + reasoning);
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    // ==================== HIERARCHICAL SEARCH LOGGING ====================
private:
    int current_search_depth = 0;    // Current depth in search tree
    int current_node_count = 0;      // Node counter at current depth
    std::vector<int> nodes_at_depth; // Track nodes explored per depth
    std::vector<int> cutoffs_at_depth; // Track cutoffs per depth
    std::vector<int> moves_at_depth;  // Track move numbers per depth
    
public:
    // Enter a new depth level in search tree
    void enterDepth(int depth, int move_index, int total_moves, const std::string& move_desc = "") {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        // Ensure vectors are large enough
        while (nodes_at_depth.size() <= depth) {
            nodes_at_depth.push_back(0);
            cutoffs_at_depth.push_back(0);
            moves_at_depth.push_back(0);
        }
        
        moves_at_depth[depth] = move_index;
        nodes_at_depth[depth]++;
        current_search_depth = depth;
        
        // Create indentation based on depth
        std::string indent(depth * 2, ' ');
        
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " ";
        
        if (depth == 0) {
            main_log_file << "ROOT: ";
        } else {
            main_log_file << "N" << nodes_at_depth[depth] << ": ";
        }
        
        main_log_file << "Move " << (move_index + 1) << "/" << total_moves;
        
        if (!move_desc.empty()) {
            main_log_file << " " << move_desc;
        }
        
        main_log_file << "\n";
        main_log_file.flush();
    }
    
    // Log hierarchical search information with proper indentation
    void logHierarchical(int depth, const std::string& message) {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        std::string indent(depth * 2, ' ');
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " " << message << "\n";
        main_log_file.flush();
    }
    
    // Log alpha-beta bounds at current depth
    void logAlphaBeta(int depth, float alpha, float beta, const std::string& context = "") {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        std::string indent(depth * 2, ' ');
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " α=" << std::fixed << std::setprecision(2) << alpha 
                     << " β=" << std::fixed << std::setprecision(2) << beta;
        
        if (!context.empty()) {
            main_log_file << " " << context;
        }
        
        main_log_file << "\n";
        main_log_file.flush();
    }
    
    // Log evaluation result at leaf nodes
    void logEvaluation(int depth, float eval, const std::string& position_info = "") {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        std::string indent(depth * 2, ' ');
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " EVAL=" << std::fixed << std::setprecision(2) << eval;
        
        if (!position_info.empty()) {
            main_log_file << " " << position_info;
        }
        
        main_log_file << "\n";
        main_log_file.flush();
    }
    
    // Log beta cutoff with statistics
    void logCutoff(int depth, int moves_searched, int total_moves, float alpha, float beta) {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        if (depth < cutoffs_at_depth.size()) {
            cutoffs_at_depth[depth]++;
        }
        
        std::string indent(depth * 2, ' ');
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " CUTOFF! β=" << std::fixed << std::setprecision(2) << beta 
                     << " ≤ α=" << std::fixed << std::setprecision(2) << alpha
                     << " (searched " << moves_searched << "/" << total_moves << " moves)\n";
        main_log_file.flush();
    }
    
    // Log best move change with evaluation improvement
    void logBestMoveChange(int depth, const std::string& move_desc, float old_eval, float new_eval) {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        std::string indent(depth * 2, ' ');
        main_log_file << "[" << getCurrentTimestamp() << "] " 
                     << indent << "D" << depth << " NEW BEST: " << move_desc
                     << " (eval: " << std::fixed << std::setprecision(2) << old_eval 
                     << " → " << std::fixed << std::setprecision(2) << new_eval << ")\n";
        main_log_file.flush();
    }
    
    // Log search statistics at end of iteration
    void logDepthStatistics() {
        if (!logging_enabled || !main_log_file.is_open()) return;
        
        main_log_file << "[" << getCurrentTimestamp() << "] SEARCH STATS:\n";
        for (size_t d = 0; d < nodes_at_depth.size(); ++d) {
            if (nodes_at_depth[d] > 0) {
                float cutoff_rate = (cutoffs_at_depth.size() > d && nodes_at_depth[d] > 0) ? 
                    (100.0f * cutoffs_at_depth[d] / nodes_at_depth[d]) : 0.0f;
                    
                main_log_file << "  Depth " << d << ": " << nodes_at_depth[d] 
                             << " nodes, " << cutoffs_at_depth[d] 
                             << " cutoffs (" << std::fixed << std::setprecision(1) 
                             << cutoff_rate << "%)\n";
            }
        }
        main_log_file << "\n";
        main_log_file.flush();
    }
    
    // Reset search statistics for new search
    void resetSearchStats() {
        nodes_at_depth.clear();
        cutoffs_at_depth.clear(); 
        moves_at_depth.clear();
        current_search_depth = 0;
        current_node_count = 0;
    }
    
    std::string moveToString(const Move& move) const;
    
    std::string getSessionId() const {
        return game_session_id;
    }
};

// Global logger instance
static AILogger g_logger;


/*
=========================================================
 STUDENT AGENT FOR STONES & RIVERS GAME
---------------------------------------------------------
 The Python game engine passes the BOARD state into C++.
 Each board cell is represented as a dictionary in Python:

    {
        "owner": "circle" | "square",          // which player owns this piece
        "side": "stone" | "river",             // piece type
        "orientation": "horizontal" | "vertical"  // only relevant if side == "river"
    }

 In C++ with pybind11, this becomes:

    std::vector<std::vector<std::map<std::string, std::string>>>

 Meaning:
   - board[y][x] gives the cell at (x, y).
   - board[y][x].empty() → true if the cell is empty (no piece).
   - board[y][x].at("owner") → "circle" or "square".
   - board[y][x].at("side") → "stone" or "river".
   - board[y][x].at("orientation") → "horizontal" or "vertical".

=========================================================
*/

// ==================== PIECE ENCODING ====================
/*
 * Fast integer encoding for pieces (cache-friendly, efficient comparisons):
 * 0 = empty
 * 1 = circle_stone
 * 2 = square_stone  
 * 3 = circle_river_horizontal
 * 4 = circle_river_vertical
 * 5 = square_river_horizontal
 * 6 = square_river_vertical
 */

enum PieceType : uint8_t {
    EMPTY = 0,
    CIRCLE_STONE = 1,
    SQUARE_STONE = 2,
    CIRCLE_RIVER_H = 3,
    CIRCLE_RIVER_V = 4,
    SQUARE_RIVER_H = 5,
    SQUARE_RIVER_V = 6
};

// ==================== PIECE UTILITIES ====================
// High-performance piece manipulation using bitwise operations
// Encoding: Circle=odd numbers (1,3,4,5), Square=even numbers (2,5,6)
// Stone: 1-2, River: 3-6, Horizontal rivers: 3,5, Vertical rivers: 4,6

// Basic type checking (existing functions)
inline bool isEmpty(uint8_t piece) { return piece == EMPTY; }
inline bool isCircle(uint8_t piece) { return piece == CIRCLE_STONE || piece == CIRCLE_RIVER_H || piece == CIRCLE_RIVER_V; }
inline bool isSquare(uint8_t piece) { return piece == SQUARE_STONE || piece == SQUARE_RIVER_H || piece == SQUARE_RIVER_V; }
inline bool isStone(uint8_t piece) { return piece == CIRCLE_STONE || piece == SQUARE_STONE; }
inline bool isRiver(uint8_t piece) { return piece >= CIRCLE_RIVER_H && piece <= SQUARE_RIVER_V; }
inline bool isHorizontal(uint8_t piece) { return piece == CIRCLE_RIVER_H || piece == SQUARE_RIVER_H; }
inline bool isVertical(uint8_t piece) { return piece == CIRCLE_RIVER_V || piece == SQUARE_RIVER_V; }

// ========== NEW HIGH-PERFORMANCE PIECE UTILITIES ==========

// Fast ownership check - optimized for frequent calls during move generation
inline bool isPieceOwner(uint8_t piece, const std::string& player) {
    if (piece == EMPTY) return false;
    
    // Bitwise optimization: circle pieces are {1,3,4}, square pieces are {2,5,6}
    // Circle: 1(001), 3(011), 4(100) - inconsistent bit pattern, use explicit check
    // Square: 2(010), 5(101), 6(110) - inconsistent bit pattern, use explicit check
    
    bool isCirclePiece = (piece == CIRCLE_STONE || piece == CIRCLE_RIVER_H || piece == CIRCLE_RIVER_V);
    return (player == "circle") ? isCirclePiece : !isCirclePiece;
}

// Alternative faster version using lookup table for critical paths
inline bool isPieceOwnerFast(uint8_t piece, bool isCirclePlayer) {
    // Lookup table: [empty, circle, square, circle, circle, square, square]
    static const bool CIRCLE_OWNERSHIP[7] = {false, true, false, true, true, false, false};
    return (piece < 7) && (CIRCLE_OWNERSHIP[piece] == isCirclePlayer);
}

// Type checking functions - optimized for minimax evaluation
inline bool isPieceStone(uint8_t piece) { 
    return piece == CIRCLE_STONE || piece == SQUARE_STONE; 
}

inline bool isPieceRiver(uint8_t piece) { 
    return piece >= CIRCLE_RIVER_H && piece <= SQUARE_RIVER_V; 
}

// River orientation extraction - used in flow computation
inline std::string getRiverOrientation(uint8_t piece) {
    if (!isPieceRiver(piece)) return "";
    
    // Horizontal rivers: 3(CIRCLE_RIVER_H), 5(SQUARE_RIVER_H) - both odd when > 2
    // Vertical rivers: 4(CIRCLE_RIVER_V), 6(SQUARE_RIVER_V) - both even when > 2
    return (piece == CIRCLE_RIVER_H || piece == SQUARE_RIVER_H) ? "horizontal" : "vertical";
}

// Fast orientation check without string allocation
inline bool isRiverHorizontal(uint8_t piece) {
    return piece == CIRCLE_RIVER_H || piece == SQUARE_RIVER_H;
}

// Stone ↔ River conversion - critical for flip move generation
inline uint8_t flipPiece(uint8_t piece, const std::string& orientation = "horizontal") {
    if (piece == EMPTY) return EMPTY;
    
    bool isHoriz = (orientation == "horizontal");
    
    // Stone to River conversion
    if (isPieceStone(piece)) {
        if (piece == CIRCLE_STONE) {
            return isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V;
        } else { // SQUARE_STONE
            return isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V;
        }
    }
    
    // River to Stone conversion  
    if (isPieceRiver(piece)) {
        return isCircle(piece) ? CIRCLE_STONE : SQUARE_STONE;
    }
    
    return piece; // shouldn't happen
}

// River rotation - used for rotate moves
inline uint8_t rotatePiece(uint8_t piece) {
    if (!isPieceRiver(piece)) return piece;
    
    // Rotate river pieces: horizontal ↔ vertical
    switch (piece) {
        case CIRCLE_RIVER_H: return CIRCLE_RIVER_V;
        case CIRCLE_RIVER_V: return CIRCLE_RIVER_H;
        case SQUARE_RIVER_H: return SQUARE_RIVER_V;
        case SQUARE_RIVER_V: return SQUARE_RIVER_H;
        default: return piece;
    }
}

// Utility for getting piece owner as boolean (for performance-critical code)
inline bool getPieceOwnerFlag(uint8_t piece) {
    // Returns true for circle pieces, false for square pieces
    return piece == CIRCLE_STONE || piece == CIRCLE_RIVER_H || piece == CIRCLE_RIVER_V;
}

// Get piece type as integer for array indexing in evaluation
inline int getPieceTypeIndex(uint8_t piece) {
    // Returns: 0=empty, 1=stone, 2=river_h, 3=river_v
    if (piece == EMPTY) return 0;
    if (isPieceStone(piece)) return 1;
    if (isRiverHorizontal(piece)) return 2;
    return 3; // vertical river
}

// Convert between string and piece type
inline uint8_t encodePiece(const std::string& owner, const std::string& side, const std::string& orientation = "horizontal") {
    if (owner.empty()) return EMPTY;
    
    bool isCircleOwner = (owner == "circle");
    
    if (side == "stone") {
        return isCircleOwner ? CIRCLE_STONE : SQUARE_STONE;
    } else { // river
        bool isHoriz = (orientation == "horizontal");
        if (isCircleOwner) {
            return isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V;
        } else {
            return isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V;
        }
    }
}

// ==================== GAME STATE CLASS ====================

class GameState {
private:
    std::vector<std::vector<uint8_t>> board;
    int rows, cols;
    std::vector<int> score_cols;
    
    // Position tracking sets for O(log n) operations
    mutable std::set<std::pair<int,int>> circle_piece_positions;
    mutable std::set<std::pair<int,int>> square_piece_positions;
    
    // Game constants (from gameEngine.py analysis)
    static constexpr int WIN_COUNT = 4;
    static constexpr int TOP_SCORE_ROW = 2;
    
    int getBottomScoreRow() const { return rows - 3; }
    
    // Position tracking helpers
    void addPiecePosition(int x, int y, uint8_t piece) {
        if (piece == EMPTY) return;
        std::pair<int,int> pos(x, y);
        if (::isCircle(piece)) {
            circle_piece_positions.insert(pos);
        } else if (::isSquare(piece)) {
            square_piece_positions.insert(pos);
        }
    }
    
    void removePiecePosition(int x, int y, uint8_t piece) {
        if (piece == EMPTY) return;
        std::pair<int,int> pos(x, y);
        if (::isCircle(piece)) {
            circle_piece_positions.erase(pos);
        } else if (::isSquare(piece)) {
            square_piece_positions.erase(pos);
        }
    }
    
    void initializePositionTracking() {
        circle_piece_positions.clear();
        square_piece_positions.clear();
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                uint8_t piece = board[y][x];
                if (piece != EMPTY) {
                    addPiecePosition(x, y, piece);
                }
            }
        }
    }
    
public:
    // Constructor
    GameState(int r, int c) : rows(r), cols(c) {
        board.resize(rows, std::vector<uint8_t>(cols, EMPTY));
        
        // Initialize score columns (from gameEngine.py: score_cols_for())
        int w = 4;
        int start = std::max(0, (cols - w) / 2);
        for (int i = start; i < start + w; ++i) {
            score_cols.push_back(i);
        }
        
        initializePositionTracking();
        // computeHash();
    }
    
    // Fast copy constructor for minimax simulations
    GameState(const GameState& other) 
        : board(other.board), rows(other.rows), cols(other.cols), 
          score_cols(other.score_cols), //hash_value(other.hash_value),
          circle_piece_positions(other.circle_piece_positions),
          square_piece_positions(other.square_piece_positions) {}

    // Deep copy for search tree
    GameState clone() const {
        return GameState(*this);
    }
    
    // Assignment operator
    GameState& operator=(const GameState& other) {
        if (this != &other) {
            board = other.board;
            rows = other.rows;
            cols = other.cols;
            score_cols = other.score_cols;
            // hash_value = other.hash_value;
            circle_piece_positions = other.circle_piece_positions;
            square_piece_positions = other.square_piece_positions;
        }
        return *this;
    }
    
    // Load from Python board format
    void loadFromPython(const std::vector<std::vector<std::map<std::string, std::string>>>& python_board) {
        for (int y = 0; y < rows && y < (int)python_board.size(); ++y) {
            for (int x = 0; x < cols && x < (int)python_board[y].size(); ++x) {
                const auto& cell = python_board[y][x];
                if (cell.empty()) {
                    board[y][x] = EMPTY;
                } else {
                    std::string owner = cell.at("owner");
                    std::string side = cell.at("side");
                    std::string orientation = cell.count("orientation") ? cell.at("orientation") : "horizontal";
                    board[y][x] = encodePiece(owner, side, orientation);
                }
            }
        }
        initializePositionTracking();
        // computeHash();
    }
    
    // Accessors
    inline uint8_t getPiece(int x, int y) const { 
        return (inBounds(x, y)) ? board[y][x] : EMPTY; 
    }
    
    inline void setPiece(int x, int y, uint8_t piece) {
        if (inBounds(x, y)) {
            uint8_t oldPiece = board[y][x];
            removePiecePosition(x, y, oldPiece);
            board[y][x] = piece;
            addPiecePosition(x, y, piece);
        }
    }
    
    inline bool inBounds(int x, int y) const {
        return x >= 0 && x < cols && y >= 0 && y < rows;
    }
    
    inline bool isEmpty(int x, int y) const {
        return inBounds(x, y) && board[y][x] == EMPTY;
    }
    
    int getRows() const { return rows; }
    int getCols() const { return cols; }
    const std::vector<int>& getScoreCols() const { return score_cols; }
    // uint64_t getHash() const { return hash_value; }
    
    // Fast access to piece positions - O(1) access, O(log n) insert/delete
    const std::set<std::pair<int,int>>& getCirclePiecePositions() const { return circle_piece_positions; }
    const std::set<std::pair<int,int>>& getSquarePiecePositions() const { return square_piece_positions; }
    
    // Get all player pieces - convenience method
    const std::set<std::pair<int,int>>& getPlayerPiecePositions(bool isCircle) const {
        return isCircle ? circle_piece_positions : square_piece_positions;
    }
    
    // Check if position is opponent's scoring area
    bool isOpponentScoreCell(int x, int y, bool isCircle) const {
        auto it = std::find(score_cols.begin(), score_cols.end(), x);
        if (it == score_cols.end()) return false;
        
        if (isCircle) {
            return y == getBottomScoreRow(); // Circle scores at bottom
        } else {
            return y == TOP_SCORE_ROW; // Square scores at top  
        }
    }
    
    // Check if position is own scoring area
    bool isOwnScoreCell(int x, int y, bool isCircle) const {
        return isOpponentScoreCell(x, y, !isCircle);
    }
    
    // Win condition checking (optimized)
    std::string getWinner() const {
        int circle_count = 0, square_count = 0;
        
        // Check circle's scoring area (top row)
        for (int x : score_cols) {
            if (inBounds(x, TOP_SCORE_ROW)) {
                uint8_t piece = board[TOP_SCORE_ROW][x];
                if (piece == CIRCLE_STONE) {
                    circle_count++;
                }
            }
        }
        
        // Check square's scoring area (bottom row)  
        int bot_row = getBottomScoreRow();
        for (int x : score_cols) {
            if (inBounds(x, bot_row)) {
                uint8_t piece = board[bot_row][x];
                if (piece == SQUARE_STONE) {
                    square_count++;
                }
            }
        }
        
        if (circle_count >= WIN_COUNT) return "circle";
        if (square_count >= WIN_COUNT) return "square";
        return ""; // No winner
    }
    
    // Count pieces in scoring areas
    int countScoringPieces(bool isCircle) const {
        int count = 0;
        int target_row = isCircle ? TOP_SCORE_ROW : getBottomScoreRow();
        uint8_t target_piece = isCircle ? CIRCLE_STONE : SQUARE_STONE;
        
        for (int x : score_cols) {
            if (inBounds(x, target_row) && board[target_row][x] == target_piece) {
                count++;
            }
        }
        return count;
    }
    
    // Count all pieces of a player - O(1) using position sets
    int countPlayerPieces(bool isCircle) const {
        return isCircle ? circle_piece_positions.size() : square_piece_positions.size();
    }
    
    // Count stones specifically - O(n) where n = number of player pieces (max 12)
    int countPlayerStones(bool isCircle) const {
        int count = 0;
        const auto& positions = isCircle ? circle_piece_positions : square_piece_positions;
        
        for (const auto& pos : positions) {
            uint8_t piece = board[pos.second][pos.first];
            if (::isStone(piece)) {
                count++;
            }
        }
        return count;
    }
    
    bool isPlayerPiece(int x, int y, bool isCircle) const {
        if (!inBounds(x, y)) return false;
        uint8_t piece = board[y][x];
        return isCircle ? ::isCircle(piece) : ::isSquare(piece);
    }
    
    // Get piece owner
    std::string getPieceOwner(int x, int y) const {
        if (!inBounds(x, y) || board[y][x] == EMPTY) return "";
        return ::isCircle(board[y][x]) ? "circle" : "square";
    }
    
    // Get piece type
    std::string getPieceType(int x, int y) const {
        if (!inBounds(x, y) || board[y][x] == EMPTY) return "";
        return ::isStone(board[y][x]) ? "stone" : "river";
    }
    
    // Get river orientation
    std::string getRiverOrientation(int x, int y) const {
        if (!inBounds(x, y) || !::isRiver(board[y][x])) return "";
        return ::isHorizontal(board[y][x]) ? "horizontal" : "vertical";
    }
    
    // Apply basic move (without validation for speed)
    void applyBasicMove(int from_x, int from_y, int to_x, int to_y) {
        if (inBounds(from_x, from_y) && inBounds(to_x, to_y)) {
            uint8_t piece = board[from_y][from_x];
            uint8_t displaced_piece = board[to_y][to_x];
            
            // Update position tracking
            removePiecePosition(from_x, from_y, piece);
            removePiecePosition(to_x, to_y, displaced_piece);
            
            board[to_y][to_x] = piece;
            board[from_y][from_x] = EMPTY;
            
            addPiecePosition(to_x, to_y, piece);
            
            // computeHash();
        }
    }
    
    // Apply push move
    void applyPushMove(int from_x, int from_y, int to_x, int to_y, int push_x, int push_y) {
        if (inBounds(from_x, from_y) && inBounds(to_x, to_y) && inBounds(push_x, push_y)) {
            uint8_t our_piece = board[from_y][from_x];
            uint8_t pushed_piece = board[to_y][to_x];
            uint8_t displaced_piece = board[push_y][push_x];
            
            // Update position tracking
            removePiecePosition(from_x, from_y, our_piece);
            removePiecePosition(to_x, to_y, pushed_piece);
            removePiecePosition(push_x, push_y, displaced_piece);
            
            // Move pushed piece to push destination
            board[push_y][push_x] = pushed_piece;
            // Move our piece to the intermediate position
            board[to_y][to_x] = our_piece;
            // Clear original position
            board[from_y][from_x] = EMPTY;
            
            addPiecePosition(to_x, to_y, our_piece);
            addPiecePosition(push_x, push_y, pushed_piece);
            
            // computeHash();
        }
    }
    
    // Flip piece (stone <-> river)
    void applyFlip(int x, int y, const std::string& new_orientation = "horizontal") {
        if (!inBounds(x, y)) return;
        
        uint8_t old_piece = board[y][x];
        if (old_piece == EMPTY) return;
        
        bool isCircleOwner = ::isCircle(old_piece);
        uint8_t new_piece;
        
        if (::isStone(old_piece)) {
            // Stone -> River
            bool isHoriz = (new_orientation == "horizontal");
            new_piece = isCircleOwner ? 
                (isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V) : 
                (isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V);
        } else {
            // River -> Stone
            new_piece = isCircleOwner ? CIRCLE_STONE : SQUARE_STONE;
        }
        
        // Update position tracking (piece stays in same position but changes type)
        removePiecePosition(x, y, old_piece);
        board[y][x] = new_piece;
        addPiecePosition(x, y, new_piece);
        
        // computeHash();
    }
    
    // Rotate river
    void applyRotate(int x, int y) {
        if (!inBounds(x, y)) return;
        
        uint8_t old_piece = board[y][x];
        if (!::isRiver(old_piece)) return;
        
        uint8_t new_piece;
        // Toggle orientation
        if (old_piece == CIRCLE_RIVER_H) {
            new_piece = CIRCLE_RIVER_V;
        } else if (old_piece == CIRCLE_RIVER_V) {
            new_piece = CIRCLE_RIVER_H;
        } else if (old_piece == SQUARE_RIVER_H) {
            new_piece = SQUARE_RIVER_V;
        } else if (old_piece == SQUARE_RIVER_V) {
            new_piece = SQUARE_RIVER_H;
        } else {
            return; // Invalid piece for rotation
        }
        
        // Update position tracking (piece stays in same position but changes orientation)
        removePiecePosition(x, y, old_piece);
        board[y][x] = new_piece;
        addPiecePosition(x, y, new_piece);
        
        // computeHash();
    }
    
    // Debugging: print board state
    void printBoard() const {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                std::cout << (int)board[y][x] << " ";
            }
            std::cout << std::endl;
        }
    }
};



// ==================== MOVE REPRESENTATION ====================
struct Move {
    std::string action;
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> pushed_to;
    std::string orientation;
    
    // Helper constructors for convenience
    Move() = default;
    Move(std::string act, std::vector<int> f, std::vector<int> t) 
        : action(std::move(act)), from(std::move(f)), to(std::move(t)) {}
    Move(std::string act, std::vector<int> f, std::vector<int> t, std::vector<int> pt, std::string ori = "")
        : action(std::move(act)), from(std::move(f)), to(std::move(t)), pushed_to(std::move(pt)), orientation(std::move(ori)) {}
        
    // Equality operator for comparison
    bool operator==(const Move& other) const {
        return action == other.action && from == other.from && to == other.to && 
               pushed_to == other.pushed_to && orientation == other.orientation;
    }
 
    // Inequality operator for comparison
    bool operator!=(const Move& other) const {
        return !(*this == other);
    }
};

// ==================== LOGGER METHOD IMPLEMENTATIONS ====================

// Implementation of moveToString (after Move struct is defined)
std::string AILogger::moveToString(const Move& move) const {
    std::stringstream ss;
    ss << move.action;
    if (!move.from.empty()) {
        ss << " (" << move.from[0] << "," << move.from[1] << ")";
    }
    if (!move.to.empty()) {
        ss << " -> (" << move.to[0] << "," << move.to[1] << ")";
    }
    if (!move.orientation.empty()) {
        ss << " [" << move.orientation << "]";
    }
    return ss.str();
}

// ==================== MOVE GENERATION ENGINE ====================

class MoveGenerator {
private:
    // Pre-allocated containers for performance (Phase 1)
    mutable std::vector<Move> move_buffer;
    mutable std::vector<std::pair<int,int>> bfs_queue;
    mutable std::vector<std::vector<bool>> visited_grid;
    mutable std::set<std::pair<int,int>> visited_set;
    mutable std::vector<std::pair<int,int>> destinations_buffer;
    
    // Direction constants
    static const std::vector<std::pair<int,int>> DIRECTIONS;
    
    // ==================== PHASE 2: RIVER FLOW ENGINE ====================
public:
    // BFS-based river flow computation (mirrors sample.cpp logic exactly)
    std::vector<std::pair<int,int>> computeRiverFlow(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int rx, int ry, int sx, int sy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols, 
        bool river_push = false) const {
        
        // Clear and prepare containers
        destinations_buffer.clear();
        visited_set.clear();
        bfs_queue.clear();
        bfs_queue.emplace_back(rx, ry);
        
        // Lambda for safe board access
        auto getCell = [&](int x, int y) -> const std::map<std::string, std::string>& {
            return board[y][x];
        };
        
        while (!bfs_queue.empty()) {
            auto [x, y] = bfs_queue.front();
            bfs_queue.erase(bfs_queue.begin()); // pop_front equivalent
            
            if (!inBounds(x, y, rows, cols)) continue;
            if (visited_set.count({x, y})) continue;
            visited_set.insert({x, y});
            
            auto cell = getCell(x, y);
            
            // Special case for river push: treat entry cell as the mover
            if (river_push && x == rx && y == ry) {
                cell = getCell(sx, sy);
            }
            
            // Empty cell - potential destination
            if (cell.empty()) {
                if (!isOpponentScoreCell(x, y, player, rows, cols, score_cols)) {
                    destinations_buffer.emplace_back(x, y);
                }
                continue;
            }
            
            // Not a river - stop flow
            if (!isRiverPiece(cell)) continue;
            
            // Get flow directions based on river orientation
            std::vector<std::pair<int,int>> flow_dirs;
            if (getRiverOrientationFromCell(cell) == "horizontal") {
                flow_dirs = {{1, 0}, {-1, 0}};
            } else {
                flow_dirs = {{0, 1}, {0, -1}};
            }
            
            // Follow flow in each direction
            for (auto [dx, dy] : flow_dirs) {
                int nx = x + dx, ny = y + dy;
                
                while (inBounds(nx, ny, rows, cols)) {
                    // Block flow into opponent scoring area
                    if (isOpponentScoreCell(nx, ny, player, rows, cols, score_cols)) break;
                    
                    const auto& next_cell = getCell(nx, ny);
                    
                    if (next_cell.empty()) {
                        // Empty cell - add as destination and continue flow
                        destinations_buffer.emplace_back(nx, ny);
                        nx += dx; ny += dy;
                        continue;
                    }
                    
                    // Skip source position during flow
                    if (nx == sx && ny == sy) {
                        nx += dx; ny += dy;
                        continue;
                    }
                    
                    // Another river - add to BFS queue
                    if (isRiverPiece(next_cell)) {
                        bfs_queue.emplace_back(nx, ny);
                        break;
                    }
                    
                    // Solid piece - stop flow
                    break;
                }
            }
        }
        
        // Remove duplicates and return unique destinations
        std::vector<std::pair<int,int>> unique_destinations;
        std::set<std::pair<int,int>> seen;
        for (const auto& dest : destinations_buffer) {
            if (seen.insert(dest).second) {
                unique_destinations.push_back(dest);
            }
        }
        
        return unique_destinations;
    }

    // OPTIMIZED: Use 2D boolean grid for destinations
    std::vector<std::pair<int,int>> computeRiverFlowOptimized(
        const GameState& gameState, 
        int rx, int ry, int sx, int sy, 
        bool isCirclePlayer, bool river_push = false) const {
        
        int rows = gameState.getRows();
        int cols = gameState.getCols();
        
        // Clear and prepare containers
        bfs_queue.clear();
        bfs_queue.emplace_back(rx, ry);
        
        // Use 2D boolean array for visited tracking
        if (visited_grid.size() != rows) {
            visited_grid.resize(rows, std::vector<bool>(cols, false));
        } else {
            // Reset existing grid
            for (auto& row : visited_grid) {
                std::fill(row.begin(), row.end(), false);
            }
        }
        
        // NEW: Use separate 2D boolean grid for destinations
        static  std::vector<std::vector<bool>> destinations_grid;
        if (destinations_grid.size() != rows) {
            destinations_grid.resize(rows, std::vector<bool>(cols, false));
        } else {
            // Reset existing destinations grid
            for (auto& row : destinations_grid) {
                std::fill(row.begin(), row.end(), false);
            }
        }

        // FIXED: Preserve BFS discovery order instead of row-major order
        std::vector<std::pair<int,int>> unique_destinations;
        unique_destinations.reserve(64);
        
        while (!bfs_queue.empty()) {
            auto [x, y] = bfs_queue.front();
            bfs_queue.erase(bfs_queue.begin());
            
            if (!gameState.inBounds(x, y)) continue;
            if (visited_grid[y][x]) continue;
            visited_grid[y][x] = true;
            
            uint8_t current_piece = gameState.getPiece(x, y);
            
            // Special case for river push
            if (river_push && x == rx && y == ry) {
                current_piece = gameState.getPiece(sx, sy);
            }
            
            if (current_piece == EMPTY) {
                if (!gameState.isOpponentScoreCell(x, y, isCirclePlayer)) {
                    if (!destinations_grid[y][x]) {  // Only add if not already added
                        destinations_grid[y][x] = true;
                        destinations_buffer.emplace_back(x, y);  // Preserve BFS order
                        unique_destinations.push_back({x, y}); // For final return
                    }
                }
                continue;
            }
            
            // Not a river - stop flow
            if (!::isRiver(current_piece)) continue;
            
            // Get flow directions
            std::vector<std::pair<int,int>> flow_dirs;
            if (::isHorizontal(current_piece)) {
                flow_dirs = {{1, 0}, {-1, 0}};
            } else {
                flow_dirs = {{0, 1}, {0, -1}};
            }
            
            // Follow flow in each direction
            for (auto [dx, dy] : flow_dirs) {
                int nx = x + dx, ny = y + dy;
                
                while (gameState.inBounds(nx, ny)) {
                    if (gameState.isOpponentScoreCell(nx, ny, isCirclePlayer)) break;
                    
                    uint8_t next_piece = gameState.getPiece(nx, ny);
                    
                    if (next_piece == EMPTY) {
                        if (!destinations_grid[ny][nx]) {  // Only add if not already added
                            destinations_grid[ny][nx] = true;
                            destinations_buffer.emplace_back(nx, ny);  // Preserve BFS order
                            unique_destinations.push_back({nx, ny}); // For final return
                        }
                        nx += dx; ny += dy;
                        continue;
                    }
                    
                    // Skip source position during flow
                    if (nx == sx && ny == sy) {
                        nx += dx; ny += dy;
                        continue;
                    }
                    
                    // Another river - add to BFS queue
                    if (::isRiver(next_piece)) {
                        bfs_queue.emplace_back(nx, ny);
                        break;
                    }
                    
                    // Solid piece - stop flow
                    break;
                }
            }
        }
        
        return unique_destinations;
        
    }

private:
    
    // ==================== HELPER FUNCTIONS ====================
    
    inline bool inBounds(int x, int y, int rows, int cols) const {
        return x >= 0 && x < cols && y >= 0 && y < rows;
    }
    
    inline bool isOpponentScoreCell(int x, int y, const std::string& player, 
                                   int rows, int cols, const std::vector<int>& score_cols) const {
        auto it = std::find(score_cols.begin(), score_cols.end(), x);
        if (it == score_cols.end()) return false;
        
        if (player == "circle") {
            return y == (rows - 3); // bottom_score_row
        } else {
            return y == 2; // top_score_row
        }
    }
    
    inline std::string getOwnerFromCell(const std::map<std::string, std::string>& cell) const {
        auto it = cell.find("owner");
        return (it != cell.end()) ? it->second : "";
    }
    
    inline std::string getSideFromCell(const std::map<std::string, std::string>& cell) const {
        auto it = cell.find("side");
        return (it != cell.end()) ? it->second : "stone";
    }
    
    inline std::string getRiverOrientationFromCell(const std::map<std::string, std::string>& cell) const {
        auto it = cell.find("orientation");
        return (it != cell.end()) ? it->second : "horizontal";
    }
    
    inline bool isRiverPiece(const std::map<std::string, std::string>& cell) const {
        return !cell.empty() && getSideFromCell(cell) == "river";
    }
    
    inline bool isStonePiece(const std::map<std::string, std::string>& cell) const {
        return !cell.empty() && getSideFromCell(cell) == "stone";
    }
    
public:
    // Constructor - initialize pre-allocated containers
    MoveGenerator() {
        move_buffer.reserve(200);  // Typical game has 50-100 moves
        bfs_queue.reserve(150);
        destinations_buffer.reserve(100);
        // visited_grid will be resized as needed
    }
    
    // OPTIMIZED: Generate moves using position sets instead of iterating entire board
    std::vector<Move> generateAllMovesOptimized(const GameState& state, const std::string& player) {
        // Clear buffer and start fresh
        move_buffer.clear();
        
        bool isCircle = (player == "circle");
        const auto& player_positions = isCircle ? state.getCirclePiecePositions() : state.getSquarePiecePositions();
        
        // Convert GameState to board format for existing move generation logic
        int rows = state.getRows();
        int cols = state.getCols();
        std::vector<std::vector<std::map<std::string, std::string>>> board_map(rows,
            std::vector<std::map<std::string, std::string>>(cols));
        
        // Only populate cells that have pieces (much faster than full board scan)
        for (const auto& pos : state.getCirclePiecePositions()) {
            int x = pos.first, y = pos.second;
            uint8_t piece = state.getPiece(x, y);
            board_map[y][x]["owner"] = "circle";
            board_map[y][x]["side"] = isPieceStone(piece) ? "stone" : "river";
            if (!isPieceStone(piece)) {
                board_map[y][x]["orientation"] = isRiverHorizontal(piece) ? "horizontal" : "vertical";
            }
        }
        
        for (const auto& pos : state.getSquarePiecePositions()) {
            int x = pos.first, y = pos.second;
            uint8_t piece = state.getPiece(x, y);
            board_map[y][x]["owner"] = "square";
            board_map[y][x]["side"] = isPieceStone(piece) ? "stone" : "river";
            if (!isPieceStone(piece)) {
                board_map[y][x]["orientation"] = isRiverHorizontal(piece) ? "horizontal" : "vertical";
            }
        }
        
        // Generate moves only for player's pieces (O(n) where n = player piece count)
        for (const auto& pos : player_positions) {
            int x = pos.first, y = pos.second;
            generateMovesForPiece(board_map, x, y, player, rows, cols, state.getScoreCols());
        }
        
        return move_buffer;
    }
    
    
    // Final Optimised Version of generateAllMoves using only GameState
    std::vector<Move> generateAllMovesOptimizedV2(const GameState& state, const std::string& player) {
        // Clear buffer and start fresh
        move_buffer.clear();
        bool isCircle = (player == "circle");
        const auto& player_positions = state.getPlayerPiecePositions(isCircle);

        for (const auto& pos : player_positions) {
            int x = pos.first, y = pos.second;
            generateMovesForPieceOptimized(state, x, y, isCircle);
        }
        return move_buffer;
    }


public:

    // ==================== PHASE 3: MOVE ORDERING SYSTEM ====================
    
    // Generate all moves for a single piece (mirrors sample.cpp logic)
    void generateMovesForPiece(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int x, int y, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols) {
        
        const auto& cell = board[y][x];
        
        // Generate movement/push moves using valid targets
        auto targets = computeValidTargets(board, x, y, player, rows, cols, score_cols);
        
        // Add movement moves
        for (const auto& move_pos : targets.moves) {
            move_buffer.emplace_back("move", std::vector<int>{x, y}, 
                                   std::vector<int>{move_pos.first, move_pos.second});
        }
        
        // Add push moves
        for (const auto& push_pair : targets.pushes) {
            const auto& own_final = push_pair.first;
            const auto& pushed_to = push_pair.second;
            move_buffer.emplace_back("push", std::vector<int>{x, y},
                                   std::vector<int>{own_final.first, own_final.second},
                                   std::vector<int>{pushed_to.first, pushed_to.second});
        }
        
        // Generate flip moves
        if (isStonePiece(cell)) {
            // Stone -> River (try both orientations if safe)
            for (const std::string& orientation : {"horizontal", "vertical"}) {
                if (isFlipSafe(board, x, y, player, rows, cols, score_cols, orientation)) {
                    move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                           std::vector<int>{x, y}, std::vector<int>{}, orientation);
                }
            }
        } else if (isRiverPiece(cell)) {
            // River -> Stone (always safe)
            move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                   std::vector<int>{x, y}, std::vector<int>{}, "");
        }
        
        // Generate rotate moves (only for rivers)
        if (isRiverPiece(cell)) {
            std::string current_orientation = getRiverOrientationFromCell(cell);
            std::string new_orientation = (current_orientation == "horizontal") ? "vertical" : "horizontal";
            
            if (isRotateSafe(board, x, y, player, rows, cols, score_cols, new_orientation)) {
                move_buffer.emplace_back("rotate", std::vector<int>{x, y}, 
                                       std::vector<int>{x, y}, std::vector<int>{}, "");
            }
        }
    }
    

    // OPTIMIZED: GameState-only version of generateMovesForPiece
    void generateMovesForPieceOptimized(
        const GameState& gameState,
        int x, int y, bool isCirclePlayer) {
        
        uint8_t piece = gameState.getPiece(x, y);
        if (piece == EMPTY || !::isPieceOwnerFast(piece, isCirclePlayer)) return;
        
        // Generate movement/push moves using optimized valid targets
        auto targets = computeValidTargetsOptimized(gameState, x, y, isCirclePlayer);
        
        // Add movement moves
        for (const auto& move_pos : targets.moves) {
            move_buffer.emplace_back("move", std::vector<int>{x, y}, 
                                std::vector<int>{move_pos.first, move_pos.second});
        }
        
        // Add push moves
        for (const auto& push_pair : targets.pushes) {
            const auto& own_final = push_pair.first;
            const auto& pushed_to = push_pair.second;
            move_buffer.emplace_back("push", std::vector<int>{x, y},
                                std::vector<int>{own_final.first, own_final.second},
                                std::vector<int>{pushed_to.first, pushed_to.second});
        }
        
        // Generate flip moves - direct GameState operations
        if (::isStone(piece)) {
            // Stone -> River (try both orientations)
            for (const std::string& orientation : {"horizontal", "vertical"}) {
                if (isFlipSafeOptimized(gameState, x, y, isCirclePlayer, orientation)) {
                    move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                        std::vector<int>{x, y}, std::vector<int>{}, orientation);
                }
            }
        } else if (::isRiver(piece)) {
            // River -> Stone (always safe)
            move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                std::vector<int>{x, y}, std::vector<int>{}, "");
        }
        
        // Generate rotate moves (only for rivers) - direct GameState operations
        if (::isRiver(piece)) {
            if (isRotateSafeOptimized(gameState, x, y, isCirclePlayer)) {
                move_buffer.emplace_back("rotate", std::vector<int>{x, y}, 
                                    std::vector<int>{x, y}, std::vector<int>{}, "");
            }
        }
    }



    // Compute valid targets for a piece (mirrors sample.cpp compute_valid_targets)
    struct ValidTargets {
        std::set<std::pair<int,int>> moves;
        std::vector<std::pair<std::pair<int,int>, std::pair<int,int>>> pushes; // ((own_final), (pushed_to))
    };
    
    ValidTargets computeValidTargets(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int sx, int sy, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols) const {
        
        ValidTargets targets;
        if (!inBounds(sx, sy, rows, cols)) return targets;
        
        const auto& piece = board[sy][sx];
        if (piece.empty() || getOwnerFromCell(piece) != player) return targets;
        
        // Check all four directions
        for (auto [dx, dy] : DIRECTIONS) {
            int tx = sx + dx, ty = sy + dy;
            if (!inBounds(tx, ty, rows, cols)) continue;
            
            // Block movement into opponent score cells
            if (isOpponentScoreCell(tx, ty, player, rows, cols, score_cols)) continue;
            
            const auto& target = board[ty][tx];
            
            if (target.empty()) {
                // Simple move to empty cell
                targets.moves.insert({tx, ty});
            } else if (isRiverPiece(target)) {
                // Move via river flow
                auto flow_destinations = computeRiverFlow(board, tx, ty, sx, sy, player, rows, cols, score_cols, false);
                for (const auto& dest : flow_destinations) {
                    targets.moves.insert(dest);
                }
            } else {
                // Target is a stone - potential push
                if (isStonePiece(piece)) {
                    // Stone pushing stone
                    int px = tx + dx, py = ty + dy;
                    if (inBounds(px, py, rows, cols) && 
                        board[py][px].empty() && 
                        !isOpponentScoreCell(px, py, getOwnerFromCell(piece), rows, cols, score_cols)) {
                        targets.pushes.push_back({{tx, ty}, {px, py}});
                    }
                } else {
                    // River pushing stone (river-push logic)
                    std::string pushed_player = getOwnerFromCell(target);
                    auto flow_destinations = computeRiverFlow(board, tx, ty, sx, sy, pushed_player, rows, cols, score_cols, true);
                    for (const auto& dest : flow_destinations) {
                        if (!isOpponentScoreCell(dest.first, dest.second, pushed_player, rows, cols, score_cols)) {
                            targets.pushes.push_back({{tx, ty}, dest});
                        }
                    }
                }
            }
        }
        
        return targets;
    }
    
    // OPTIMIZED: GameState-only version of computeValidTargets
    ValidTargets computeValidTargetsOptimized(
        const GameState& gameState,
        int sx, int sy, bool isCirclePlayer) const {
        
        ValidTargets targets;
        
        if (!gameState.inBounds(sx, sy)) return targets;
        
        uint8_t source_piece = gameState.getPiece(sx, sy);
        if (source_piece == EMPTY || !::isPieceOwnerFast(source_piece, isCirclePlayer)) return targets;
        
        // Check all four directions using static array (faster than vector construction)
        static const int DIRECTIONS[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
        
        for (int dir = 0; dir < 4; ++dir) {
            int tx = sx + DIRECTIONS[dir][0];
            int ty = sy + DIRECTIONS[dir][1];
            
            if (!gameState.inBounds(tx, ty)) continue;
            
            // Block movement into opponent score cells - direct GameState check
            if (gameState.isOpponentScoreCell(tx, ty, isCirclePlayer)) continue;
            
            uint8_t target_piece = gameState.getPiece(tx, ty);
            
            if (target_piece == EMPTY) {
                // Simple move to empty cell
                targets.moves.insert({tx, ty});
            }
            else if (::isRiver(target_piece)) {
                // OPTIMIZED: Use direct GameState river flow (no board conversion!)
                auto flow_destinations = computeRiverFlowOptimized(
                    gameState, tx, ty, sx, sy, isCirclePlayer, false
                );
                
                // Add all flow destinations to moves set
                for (const auto& dest : flow_destinations) {
                    targets.moves.insert(dest);
                }
            }
            else {
                // Target is a stone - potential push
                if (::isStone(source_piece)) {
                    // Stone pushing stone
                    int px = tx + DIRECTIONS[dir][0];
                    int py = ty + DIRECTIONS[dir][1];
                    
                    if (gameState.inBounds(px, py) && 
                        gameState.getPiece(px, py) == EMPTY && 
                        !gameState.isOpponentScoreCell(px, py, isCirclePlayer)) {
                        targets.pushes.push_back({{tx, ty}, {px, py}});
                    }
                }
                else {
                    // River pushing stone (river-push logic)
                    bool pushed_piece_is_circle = ::isCircle(target_piece);
                    
                    // OPTIMIZED: Direct GameState river flow for pushed piece
                    auto flow_destinations = computeRiverFlowOptimized(
                        gameState, tx, ty, sx, sy, pushed_piece_is_circle, true
                    );
                    
                    for (const auto& dest : flow_destinations) {
                        if (!gameState.isOpponentScoreCell(dest.first, dest.second, pushed_piece_is_circle)) {
                            targets.pushes.push_back({{tx, ty}, dest});
                        }
                    }
                }
            }
        }
        
        return targets;
    }


    // Safety check for flip moves - prevents rivers that allow flow into opponent score    TODO - Check if this is needed 
    bool isFlipSafe(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                int fx, int fy, const std::string& player,
                int rows, int cols, const std::vector<int>& score_cols,
                const std::string& orientation) const {
        
        // Only basic validation - make sure it's a valid piece to flip
        const auto& piece = board[fy][fx];
        return !piece.empty(); // Can flip any piece (stone->river or river->stone)
    }
    
    // Safety check for rotate moves - prevents rotations that allow flow into opponent score
    bool isRotateSafe(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                  int rx, int ry, const std::string& player,
                  int rows, int cols, const std::vector<int>& score_cols,
                  const std::string& new_orientation) const {
    
        // Only basic validation - make sure it's a river
        const auto& piece = board[ry][rx];
        return isRiverPiece(piece); // Can rotate any river
    }
    
    // Optimized safety checks (much simpler without string operations)
    bool isFlipSafeOptimized(const GameState& gameState, int fx, int fy, 
                            bool isCirclePlayer, const std::string& orientation) const {
        uint8_t piece = gameState.getPiece(fx, fy);
        return piece != EMPTY;  // Can flip any non-empty piece
    }

    bool isRotateSafeOptimized(const GameState& gameState, int rx, int ry, bool isCirclePlayer) const {
        uint8_t piece = gameState.getPiece(rx, ry);
        return ::isRiver(piece);  // Can rotate any river
    }

};

// Static member definition
const std::vector<std::pair<int,int>> MoveGenerator::DIRECTIONS = {{1,0},{-1,0},{0,1},{0,-1}};

// ==================== BOARD EVALUATION SYSTEM ====================

class BoardEvaluator {
private:
    
    struct ScoringArea {
        int row = 0;  // Inclusive bounds for scoring rows
        std::vector<int> score_cols;  // Scoring columns
    };
    
    mutable ScoringArea circle_scoring;   
    mutable ScoringArea square_scoring;
    mutable bool scoring_areas_initialized = false;

    float weight1 = 1000.0f;
    float weight2 = -4.0f; //<180 onwards
    float weight4 = 0.0f;
    float weight_river_mobility = 0.10f;    // Weight for territorial reach
    float weight_river_combos = 0.08f;      // Weight for river combinations

    
    void initializeScoringAreas(const GameState& gameState) const {
        if (scoring_areas_initialized) return;
        
        int rows = gameState.getRows();
        const auto& score_cols = gameState.getScoreCols();
        
        // Circle scores in bottom rows (rows-3 to rows-1)
        circle_scoring.row = 2;
        circle_scoring.score_cols = score_cols;
        
        // Square scores in top rows (0 to 2)
        square_scoring.row = rows-3;
        square_scoring.score_cols = score_cols;
        
        scoring_areas_initialized = true;
    }
    
public:

    float EvaluateBoard(const GameState& gameState, bool isCirclePlayer) const {

        initializeScoringAreas(gameState);
        float score1 = computeBasicEvaluation(gameState, isCirclePlayer);
        float score2 = evaluatePosition(gameState, isCirclePlayer);
        float score4 = evaluateMobility(gameState, isCirclePlayer);
        
        float river_mobility = evaluateRiverMobility(gameState, isCirclePlayer);
        float river_combos = evaluateRiverCombos(gameState, isCirclePlayer);
        
        float base_score = score1 * weight1 + score2 * weight2;
                    
        float river_score_tot = river_mobility * weight_river_mobility + river_combos * weight_river_combos;

        return base_score + river_score_tot;
    }
    
    float computeBasicEvaluation(const GameState& gameState, bool isCirclePlayer) const {
        
        float score = 0.0f;
        int player_scoring_stones = countStonesInScoringArea(gameState, isCirclePlayer);
        int opponent_scoring_stones = countStonesInScoringArea(gameState, !isCirclePlayer);
        
        score += player_scoring_stones * 2.0f;   // +100 per scoring stone
        score -= opponent_scoring_stones * 2.0f; // -100 per opponent scoring stone

        int player_scoring_Rivers = countRiversInScoringArea(gameState, isCirclePlayer);
        int opponent_scoring_Rivers = countRiversInScoringArea(gameState, !isCirclePlayer);
        
        score += player_scoring_Rivers * 0.75f;   // +60 per scoring River
        score -= opponent_scoring_Rivers * 0.75f; // -60 per opponent scoring River
        
        return score;
    }
    
    float evaluatePosition(const GameState& gameState, bool isCirclePlayer) const {
        std::vector<int> player_distances = getmoveDistancesFromScoringArea(gameState, isCirclePlayer);
        std::vector<int> opponent_distances = getmoveDistancesFromScoringArea(gameState, !isCirclePlayer);

        float sum1=0,sum2=0;
        for(int i=0;i<4;i++){
            sum1+=player_distances[i];
            sum2+=opponent_distances[i];
        }
        
        return sum1-sum2;
    }
    
    float evaluateMobility(const GameState& gameState, bool isCirclePlayer) const {

        int my_mobile_stones = countStonesAdjacentToRivers(gameState, isCirclePlayer);
        
        int opponent_mobile_stones = countStonesAdjacentToRivers(gameState, !isCirclePlayer);
        
        return static_cast<float>(my_mobile_stones);
    }
    

private:

    // ================= RIVER-BUILDING EVALUATION ========================
        
        // Evaluate river combinations - rivers that connect to extend kreach
    float evaluateRiverCombos(const GameState& gameState, bool isCirclePlayer) const {
        float combo_score = 0.0f;
        const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
        
        // Find all our rivers
        std::vector<std::pair<int,int>> our_rivers;
        for (const auto& pos : player_positions) {
            uint8_t piece = gameState.getPiece(pos.first, pos.second);
            if (::isRiver(piece)) {
                our_rivers.push_back(pos);
            }
        }
        
        // Check each river pair for connectivity
        for (size_t i = 0; i < our_rivers.size(); ++i) {
            for (size_t j = i + 1; j < our_rivers.size(); ++j) {
                float connection_score = evaluateRiverConnection(gameState, our_rivers[i], our_rivers[j], isCirclePlayer);
                combo_score += connection_score;
            }
        }
        
        return combo_score;
    }

    // Evaluate how well two rivers connect/combo together
    float evaluateRiverConnection(const GameState& gameState, const std::pair<int,int>& river1, 
                                const std::pair<int,int>& river2, bool isCirclePlayer) const {
        float connection_score = 0.0f;
        
        // Get flow destinations from both rivers
        auto destinations1 = simulateRiverFlow(gameState, river1.first, river1.second, isCirclePlayer);
        auto destinations2 = simulateRiverFlow(gameState, river2.first, river2.second, isCirclePlayer);
        
        // Check if river1 can reach river2's position (or vice versa)
        for (const auto& dest : destinations1) {
            if (dest == river2) {
                connection_score += 1.0f; // Direct connection bonus
                break;
            }
        }
        
        // Check for overlapping reachable areas (rivers that reach the same strategic areas)
        int shared_territory = 0;
        for (const auto& dest1 : destinations1) {
            for (const auto& dest2 : destinations2) {
                // If both rivers can reach positions close to each other
                int distance = abs(dest1.first - dest2.first) + abs(dest1.second - dest2.second);
                if (distance <= 2) { // Within 2 Manhattan distance
                    shared_territory++;
                }
            }
        }
        
        connection_score += shared_territory * 0.2f; // Bonus for shared territorial control
        
        return connection_score;
    }

    // Simple mobility evaluation - count reachable positions in opponent area
    float evaluateRiverMobility(const GameState& gameState, bool isCirclePlayer) const {
        float mobility_score = 0.0f;
        const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);

        // Check each of our rivers
        for (const auto& pos : player_positions) {
            int x = pos.first, y = pos.second;
            uint8_t piece = gameState.getPiece(x, y);
            
            if (::isRiver(piece)) {
                // Count positions this river can reach in opponent territory
                float reachable_opponent_positions = countReachableOpponentPositions(gameState, x, y, isCirclePlayer);
                mobility_score += reachable_opponent_positions * 0.15f; // 3 points per reachable position
                
                // Bonus for reaching scoring area specifically
                float scoring_area_reach = countReachableScoringPositions(gameState, x, y, isCirclePlayer);
                mobility_score += scoring_area_reach * 0.85f; // 1 point per scoring position reachable
            }
        }
        
        return mobility_score;
    }

    // Change return type from int to float
    float countReachableOpponentPositions(const GameState& gameState, int river_x, int river_y, bool isCirclePlayer) const {
        float weighted_score = 0.0f;  // Changed from int reachable_count = 0
        int opponent_half_start = isCirclePlayer ? 0 : (gameState.getRows() / 2);
        int opponent_half_end = isCirclePlayer ? (gameState.getRows() / 2) : gameState.getRows();
        
        // Get our goal zone info for distance calculation
        const auto& score_cols = gameState.getScoreCols();
        int our_goal_row = isCirclePlayer ? 2 : (gameState.getRows() - 3);
        
        std::vector<std::pair<int,int>> destinations = simulateRiverFlow(gameState, river_x, river_y, isCirclePlayer);
        
        for (const auto& dest : destinations) {
            int dest_x = dest.first, dest_y = dest.second;
            // Check if destination is in opponent territory
            if (dest_y >= opponent_half_start && dest_y < opponent_half_end) {
                
                // Calculate distance to nearest goal zone position
                int min_distance = 100;
                for (int goal_col : score_cols) {
                    int distance = abs(dest_x - goal_col) + abs(dest_y - our_goal_row);
                    min_distance = std::min(min_distance, distance);
                }
                
                // Weight: closer to goal = higher score (3.0 for distance 0, decreasing to 1.0 for distance 2+)
                float weight = std::max(1.0f, 4.0f - min_distance);
                weighted_score += weight;
            }
        }
        
        return weighted_score;  // Changed from return reachable_count
    }
    
    // Change return type from int to float  
    float countReachableScoringPositions(const GameState& gameState, int river_x, int river_y, bool isCirclePlayer) const {
        float weighted_score = 0.0f;  // Changed from int scoring_reach = 0
        const auto& score_cols = gameState.getScoreCols();
        int target_scoring_row = isCirclePlayer ? 2 : (gameState.getRows() - 3);
        
        std::vector<std::pair<int,int>> destinations = simulateRiverFlow(gameState, river_x, river_y, isCirclePlayer);
        
        for (const auto& dest : destinations) {
            int dest_x = dest.first, dest_y = dest.second;
            
            // Calculate distance to nearest scoring column
            int min_distance_to_scoring = 100;
            for (int score_col : score_cols) {
                int distance = abs(dest_x - score_col) + abs(dest_y - target_scoring_row);
                min_distance_to_scoring = std::min(min_distance_to_scoring, distance);
            }
            
            // Weight positions closer to scoring area more heavily
            if (min_distance_to_scoring <= 3) {  // Only count positions reasonably close to scoring
                float weight = std::max(1.0f, 4.0f - min_distance_to_scoring);
                weighted_score += weight;
                
                // Extra bonus if this position is exactly in scoring area
                if (dest_y == target_scoring_row) {
                    for (int score_col : score_cols) {
                        if (dest_x == score_col) {
                            weighted_score += 2.0f;  // Big bonus for direct scoring positions
                            break;
                        }
                    }
                }
            }
        }
        
        return weighted_score;  // Changed from return scoring_reach
    }
    
    // Simple river flow simulation - just follow the river orientation
    std::vector<std::pair<int,int>> simulateRiverFlow(const GameState& gameState, int start_x, int start_y, bool isCirclePlayer) const {
        std::vector<std::pair<int,int>> reachable_positions;
        uint8_t river_piece = gameState.getPiece(start_x, start_y);
        
        if (!::isRiver(river_piece)) return reachable_positions;
        
        // Determine flow directions based on river orientation
        std::vector<std::pair<int,int>> directions;
        if (::isHorizontal(river_piece)) {
            directions = {{1, 0}, {-1, 0}}; // left and right
        } else {
            directions = {{0, 1}, {0, -1}}; // up and down
        }
        
        // Follow each direction until blocked
        for (const auto& dir : directions) {
            int current_x = start_x + dir.first;
            int current_y = start_y + dir.second;
            
            // Follow this direction while we can
            while (gameState.inBounds(current_x, current_y)) {
                uint8_t current_piece = gameState.getPiece(current_x, current_y);
                
                if (current_piece == EMPTY) {
                    // Empty position - can reach here
                    reachable_positions.push_back({current_x, current_y});
                    current_x += dir.first;
                    current_y += dir.second;
                } else if (::isRiver(current_piece)) {
                    // Another river - continue flow but don't count as reachable (already has piece)
                    current_x += dir.first;
                    current_y += dir.second;
                } else {
                    // Stone or opponent piece - flow stops
                    break;
                }
            }
        }
        
        return reachable_positions;
    }
    
    // Count stones in scoring area - helper function
    int countStonesInScoringArea(const GameState& gameState, bool isCirclePlayer) const {
        int count = 0;
        const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
        
        // Check all positions in the scoring area
        for (int col : area.score_cols) {
            if (gameState.inBounds(col,area.row) &&
                gameState.isPlayerPiece(col,area.row, isCirclePlayer) &&
                gameState.getPieceType(col,area.row) == "stone") {
                count++;
            }
        }
        
        return count;
    }
    // Count stones in scoring area - helper function
    int countRiversInScoringArea(const GameState& gameState, bool isCirclePlayer) const {
        int count = 0;
        const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
        
        // Check all positions in the scoring area
        for (int col : area.score_cols) {
            if (gameState.inBounds(col,area.row) &&
                gameState.isPlayerPiece(col,area.row, isCirclePlayer) &&
                gameState.getPieceType(col,area.row) == "river") {
                count++;
            }
        }
        
        return count;
    }
    
    // Calculate distances from scoring area for all player pieces
    // Returns list of minimum Manhattan distances to any of the 4 scoring cells
    std::vector<int> getmoveDistancesFromScoringArea(const GameState& gameState, bool isCirclePlayer) const {        
        
        std::vector<int> distances;
        const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
        const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
        
        for(auto &piece:player_positions){
            int d = distance_from_piece(gameState, isCirclePlayer, piece);
            if(d==0) continue; // Skip pieces already in scoring area
            distances.push_back(d);
        }

        sort(distances.begin(),distances.end());

        return distances;
    }

    int distance_from_piece(const GameState& gameState, bool isCirclePlayer, const std::pair<int,int>& piece) const {
    // Find minimum number of moves for a piece to reach any cell of scoring area using BFS
    int piece_x = piece.first;
    int piece_y = piece.second;
    
    // Check if piece is already in scoring area
    if (gameState.isOwnScoreCell(piece_x, piece_y, isCirclePlayer)) {
        return 0;
    }
    
    int rows = gameState.getRows();
    int cols = gameState.getCols();
    
    // BFS setup - replace std::set with 2D boolean array
    std::queue<std::pair<std::pair<int,int>, int>> bfs_queue; // ((x,y), distance)
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    
    // Start BFS from piece position
    bfs_queue.push({{piece_x, piece_y}, 0});
    visited[piece_y][piece_x] = true;  // Note: visited[y][x] format
    
    // Direction vectors for adjacent cells
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    
    while (!bfs_queue.empty()) {
        auto current = bfs_queue.front();
        bfs_queue.pop();
        
        int curr_x = current.first.first;
        int curr_y = current.first.second;
        int curr_dist = current.second;
        
        // Check all 4 adjacent positions
        for (int dir = 0; dir < 4; dir++) {
            int next_x = curr_x + dx[dir];
            int next_y = curr_y + dy[dir];
            
            // Skip if out of bounds
            if (!gameState.inBounds(next_x, next_y)) continue;
            
            // Skip if already visited - use 2D array instead of set
            if (visited[next_y][next_x]) continue;
            
            uint8_t next_piece = gameState.getPiece(next_x, next_y);
            
            // If it's an empty cell
            if (next_piece == EMPTY) {
                // Check if this is a scoring cell
                if (gameState.isOwnScoreCell(next_x, next_y, isCirclePlayer)) {
                    return curr_dist + 1;
                }
                
                // Add to BFS queue for further exploration
                visited[next_y][next_x] = true;  // Mark as visited
                bfs_queue.push({{next_x, next_y}, curr_dist + 1});
            }
            // If it's a river piece, we can potentially flow through it
            else if (::isRiver(next_piece)) {
                visited[next_y][next_x] = true;  // Mark as visited
                
                // // Get all positions reachable through this river using river flow
                // // Convert current GameState to board format for river flow computation
                // std::vector<std::vector<std::map<std::string, std::string>>> board_map(rows,
                //     std::vector<std::map<std::string, std::string>>(cols));
                
                // // Populate board map (only need to populate pieces for river flow)
                // for (const auto& pos : gameState.getCirclePiecePositions()) {
                //     int x = pos.first, y = pos.second;
                //     uint8_t piece = gameState.getPiece(x, y);
                //     board_map[y][x]["owner"] = "circle";
                //     board_map[y][x]["side"] = ::isStone(piece) ? "stone" : "river";
                //     if (!::isStone(piece)) {
                //         board_map[y][x]["orientation"] = ::isHorizontal(piece) ? "horizontal" : "vertical";
                //     }
                // }
                
                // for (const auto& pos : gameState.getSquarePiecePositions()) {
                //     int x = pos.first, y = pos.second;
                //     uint8_t piece = gameState.getPiece(x, y);
                //     board_map[y][x]["owner"] = "square";
                //     board_map[y][x]["side"] = ::isStone(piece) ? "stone" : "river";
                //     if (!::isStone(piece)) {
                //         board_map[y][x]["orientation"] = ::isHorizontal(piece) ? "horizontal" : "vertical";
                //     }
                // }
                
                // Use move generator's river flow to find all reachable positions
                MoveGenerator temp_gen;
                std::string player_str = isCirclePlayer ? "circle" : "square";
                // auto flow_destinations = temp_gen.computeRiverFlow(
                //     board_map, next_x, next_y, curr_x, curr_y, player_str,
                //     rows, cols, gameState.getScoreCols(), false
                // );
                
                auto flow_destinations = temp_gen.computeRiverFlowOptimized(
                    gameState, next_x, next_y, curr_x, curr_y, isCirclePlayer, false
                );

                // Check all flow destinations
                for (const auto& dest : flow_destinations) {
                    int dest_x = dest.first;
                    int dest_y = dest.second;
                    
                    // Skip if already visited - use 2D array instead of set
                    if (visited[dest_y][dest_x]) continue;
                    
                    // Check if this destination is a scoring cell
                    if (gameState.isOwnScoreCell(dest_x, dest_y, isCirclePlayer)) {
                        return curr_dist + 1;
                    }
                    
                    // Add to BFS queue for further exploration
                    visited[dest_y][dest_x] = true;  // Mark as visited
                    bfs_queue.push({{dest_x, dest_y}, curr_dist + 1});
                }
            }
            // If it's a stone piece, we can potentially push it (but that's complex, skip for now)
            // For simplicity, we'll only consider empty cells and river flow
        }
    }
    
    // If no path found to scoring area, return a large number
    return 100;
}
    
    // Count stones adjacent to any river (for mobility evaluation)
    int countStonesAdjacentToRivers(const GameState& gameState, bool isCirclePlayer) const {
        int mobile_stone_count = 0;
        const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
        
        // Check each player piece
        for (const auto& pos : player_positions) {
            int x = pos.first;
            int y = pos.second;
            uint8_t piece = gameState.getPiece(x, y);
            
            // Only count stones (not rivers)
            if (!::isStone(piece)) continue;
            
            // Check if this stone is adjacent to any river (in 4 directions)
            bool is_adjacent_to_river = false;
            
            // Check all 4 adjacent directions: up, down, left, right
            static const int dx[] = {0, 0, -1, 1};
            static const int dy[] = {-1, 1, 0, 0};
            
            for (int dir = 0; dir < 4; ++dir) {
                int adj_x = x + dx[dir];
                int adj_y = y + dy[dir];
                
                if (gameState.inBounds(adj_x, adj_y)) {
                    uint8_t adj_piece = gameState.getPiece(adj_x, adj_y);
                    
                    // If adjacent cell contains any river (regardless of owner), this stone is mobile
                    if (::isRiver(adj_piece)) {
                        is_adjacent_to_river = true;
                        break;  // Found at least one adjacent river, no need to check other directions
                    }
                }
            }
            
            if (is_adjacent_to_river) {
                mobile_stone_count++;
            }
        }
        
        return mobile_stone_count;
    }
    
// public:
//     // Clear evaluation cache (useful for testing)
//     void clearCache() const {
//         evaluation_cache.clear();
//     }
    
//     // Get cache statistics (for performance monitoring)
//     size_t getCacheSize() const {
//         return evaluation_cache.size();
//     }
};

// ==================== MINIMAX SEARCH ENGINE ====================

// ==================== PHASE 5A: TIME MANAGER ====================
class TimeManager {
private:
    std::chrono::high_resolution_clock::time_point search_start;
    float allocated_time;           // Time allocated for this move (seconds)
    float emergency_buffer;         // Reserve time for critical positions
    bool time_up;                   // Emergency stop flag
    float total_remaining_time;     // Total time left in game
    
public:
    TimeManager() : allocated_time(0.0f), emergency_buffer(2.0f), time_up(false), total_remaining_time(60.0f) {}
    
    // Start search timer and allocate time for this move
    void startSearch(float remaining_time, float opponent_time) {
        search_start = std::chrono::high_resolution_clock::now();
        total_remaining_time = remaining_time;
        time_up = false;
        
        // Dynamic time allocation based on game situation
        allocated_time = allocateTimeForMove(remaining_time, opponent_time);
    }
    
    // Check if we should stop search due to time constraints
    bool shouldStop() const {
        if (time_up) return true;
        return getElapsedTime() >= allocated_time;
    }
    
    // Force stop (for emergency situations)
    void forceStop() {
        time_up = true;
    }
    
    // Get elapsed time since search started
    float getElapsedTime() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start);
        return duration.count() / 1000.0f;  // Convert to seconds
    }
    
    // Get remaining time for this search
    float getRemainingTime() const {
        return std::max(0.0f, allocated_time - getElapsedTime());
    }
    
private:
    // Allocate time for current move based on game state
    float allocateTimeForMove(float remaining_time, float opponent_time) const {
        // Safety check: always reserve emergency buffer
        float usable_time = std::max(0.1f, remaining_time - emergency_buffer);
        
        // // Basic time allocation strategy
        // if (remaining_time > 30.0f) {
        //     // Early/mid game: use moderate time
        //     return std::min(5.0f, usable_time * 0.15f);
        // } else if (remaining_time > 10.0f) {
        //     // Late game: be more aggressive with time usage
        //     return std::min(8.0f, usable_time * 0.25f);
        // } else {
        //     // Endgame: use most remaining time but keep emergency reserve
        //     return std::min(remaining_time * 0.7f, usable_time);
        // }
        return 60.0f;
    }
};

// ==================== PHASE 5A: BASIC MINIMAX ENGINE ====================

// Structure to hold search result with evaluation and best move
struct SearchResult {
    float evaluation;
    Move bestMove;
    int depth_reached;
    bool timeout_occurred;
    
    SearchResult() : evaluation(0.0f), depth_reached(0), timeout_occurred(false) {}
    SearchResult(float eval, const Move& move, int depth = 0) 
        : evaluation(eval), bestMove(move), depth_reached(depth), timeout_occurred(false) {}
};

// ==================== TRANSPOSITION TABLE ====================

struct TTEntry {
    float evaluation;
    Move best_move;
    int depth;
    enum NodeType {
        EXACT = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2
    } node_type;
    
    TTEntry() : evaluation(0.0f), depth(-1), node_type(EXACT) {}
    TTEntry(float eval, const Move& move, int d, NodeType type) 
        : evaluation(eval), best_move(move), depth(d), node_type(type) {}
};

class TranspositionTable {
private:
    static constexpr size_t MAX_TABLE_SIZE = 500000;  // 500K entries max
    static constexpr int MAX_BOARD_SIZE = 20;
    
    std::unordered_map<uint64_t, TTEntry> table;
    
    // Zobrist hash tables (minimal implementation)
    static uint64_t piece_square_table[7][MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    static uint64_t player_to_move_key;
    static bool zobrist_initialized;
    
    static void initializeZobrist() {
        if (zobrist_initialized) return;
        
        std::mt19937_64 rng(42);  // Fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> dist;
        
        // Initialize piece-square table
        for (int piece = 0; piece < 7; ++piece) {
            for (int y = 0; y < MAX_BOARD_SIZE; ++y) {
                for (int x = 0; x < MAX_BOARD_SIZE; ++x) {
                    piece_square_table[piece][y][x] = dist(rng);
                }
            }
        }
        
        player_to_move_key = dist(rng);
        zobrist_initialized = true;
    }
    
public:
    TranspositionTable() {
        initializeZobrist();
        table.reserve(MAX_TABLE_SIZE);
    }
    
    void clear() {
        table.clear();
    }
    
    void newSearch() {
        // Optionally clear old entries if table gets too large
        if (table.size() > MAX_TABLE_SIZE) {
            table.clear();
        }
    }
    
    // Compute Zobrist hash for a game state
    uint64_t computeHash(const GameState& state, bool isCircleToMove) const {
        uint64_t hash = 0;
        
        // Hash piece positions
        for (int y = 0; y < state.getRows(); ++y) {
            for (int x = 0; x < state.getCols(); ++x) {
                uint8_t piece = state.getPiece(x, y);
                if (piece != EMPTY && y < MAX_BOARD_SIZE && x < MAX_BOARD_SIZE) {
                    hash ^= piece_square_table[piece][y][x];
                }
            }
        }
        
        // Hash player to move
        if (isCircleToMove) {
            hash ^= player_to_move_key;
        }
        
        return hash;
    }
    
    // Probe the transposition table
    bool probe(uint64_t hash_key, int depth, float alpha, float beta, 
               float& evaluation, Move& best_move) const {
        
        auto it = table.find(hash_key);
        if (it == table.end() || it->second.depth < depth) {
            return false;
        }
        
        const TTEntry& entry = it->second;
        best_move = entry.best_move;
        
        // Check if we can use this evaluation based on node type
        switch (entry.node_type) {
            case TTEntry::EXACT:
                evaluation = entry.evaluation;
                return true;
                
            case TTEntry::LOWER_BOUND:
                if (entry.evaluation >= beta) {
                    evaluation = entry.evaluation;
                    return true;
                }
                break;
                
            case TTEntry::UPPER_BOUND:
                if (entry.evaluation <= alpha) {
                    evaluation = entry.evaluation;
                    return true;
                }
                break;
        }
        
        return false;  // Can't use this entry's evaluation
    }
    
    // Store position in transposition table
    void store(uint64_t hash_key, float evaluation, const Move& best_move, 
               int depth, float original_alpha, float beta) {
        
        // Determine node type
        TTEntry::NodeType node_type;
        if (evaluation <= original_alpha) {
            node_type = TTEntry::UPPER_BOUND;
        } else if (evaluation >= beta) {
            node_type = TTEntry::LOWER_BOUND;
        } else {
            node_type = TTEntry::EXACT;
        }
        
        // Check if we should replace existing entry
        auto it = table.find(hash_key);
        if (it == table.end() || it->second.depth <= depth) {
            // Insert or replace with deeper search
            table[hash_key] = TTEntry(evaluation, best_move, depth, node_type);
        }
    }
    
    // Get table statistics
    size_t size() const { return table.size(); }
};

// Static member definitions
uint64_t TranspositionTable::piece_square_table[7][MAX_BOARD_SIZE][MAX_BOARD_SIZE];
uint64_t TranspositionTable::player_to_move_key;
bool TranspositionTable::zobrist_initialized = false;

class MinimaxEngine {
private:
    BoardEvaluator* evaluator;
    MoveGenerator* moveGenerator;
    TimeManager timeManager;
    TranspositionTable tt;
    
    // Search statistics
    int nodes_searched;
    int max_depth_reached;
    
    // Principal Variation storage - stores best moves from previous iteration
    // pv_moves[depth][position_hash] = best_move
    std::vector<std::unordered_map<uint64_t, Move>> pv_moves;
    
public:
    MinimaxEngine(BoardEvaluator* eval, MoveGenerator* moveGen) 
        : evaluator(eval), moveGenerator(moveGen), nodes_searched(0), max_depth_reached(0) {
        pv_moves.resize(10);  // Support up to depth 10
    }
    
    // Main entry point: find best move using iterative deepening
    Move getBestMove(const GameState& position, const std::string& player, 
                     float remaining_time, float opponent_time) {
        
        // Start timing
        timeManager.startSearch(remaining_time, opponent_time);
        
        // Initialize TT for new search
        tt.newSearch();
        
        // Clear PV moves from previous search
        clearPVMoves();
        
        // Reset search statistics
        nodes_searched = 0;
        max_depth_reached = 0;

        g_logger.resetSearchStats();
        // Convert player string to boolean for consistency
        bool isCirclePlayer = (player == "circle");
        
        g_logger.log(LogLevel::DEBUG, "MINIMAX: Starting search for " + player + 
                    ", time=" + std::to_string(remaining_time) + "s");
        
        // Get all legal moves for the root position
        std::vector<Move> allRootMoves = generateMovesForPosition(position, player);
        
        std::vector<Move> rootMoves = selectTopRootMoves(position, allRootMoves, isCirclePlayer, 32);

        g_logger.log(LogLevel::DEBUG, "MINIMAX: Found " + std::to_string(rootMoves.size()) + 
                    " legal moves at root");
        
        if (rootMoves.empty()) {
            g_logger.log(LogLevel::ERROR, "MINIMAX: No legal moves available!");
            // No legal moves - return a default move
            return {"move", {0,0}, {0,0}, {}, ""};
        }
        
        if (rootMoves.size() == 1) {
            g_logger.log(LogLevel::INFO, "MINIMAX: Only one legal move, skipping search");
            // Only one legal move - no need to search
            return rootMoves[0];
        }
        
        // Iterative deepening search
        SearchResult bestResult;
        bestResult.bestMove = rootMoves[0];  // Default to first legal move
        
        g_logger.log(LogLevel::DEBUG, "MINIMAX: Starting iterative deepening");
        
        for (int depth = 1; depth <= MAX_DEPTH; ++depth) { 
            if (timeManager.shouldStop()) {
                g_logger.log(LogLevel::DEBUG, "MINIMAX: Time limit reached at depth " + std::to_string(depth));
                break;
            }
            
            g_logger.log(LogLevel::DEBUG, "MINIMAX: Searching at depth " + std::to_string(depth));
            int nodes_before = nodes_searched;
            
            SearchResult currentResult = searchAtDepth(position, depth, isCirclePlayer, rootMoves);
            
            int nodes_at_depth = nodes_searched - nodes_before;
            g_logger.log(LogLevel::DEBUG, "MINIMAX: Depth " + std::to_string(depth) + 
                        " completed, nodes=" + std::to_string(nodes_at_depth) + 
                        ", eval=" + std::to_string(currentResult.evaluation) +
                        ", move=" + currentResult.bestMove.action);
            
            if (!currentResult.timeout_occurred) {
                if (bestResult.bestMove != currentResult.bestMove) {
                    g_logger.log(LogLevel::INFO, "MINIMAX: Best move changed from " + 
                                bestResult.bestMove.action + " to " + currentResult.bestMove.action +
                                " (eval: " + std::to_string(bestResult.evaluation) + " -> " + 
                                std::to_string(currentResult.evaluation) + ")");
                }
                bestResult = currentResult;
                max_depth_reached = depth;
            }
            
            // If we found a very good position, don't waste more time
            // if (bestResult.evaluation > 1000000.0f) {
            //     g_logger.log(LogLevel::DEBUG, "MINIMAX: Found winning position, stopping early");
            //     break;
            // }
        }
        
        g_logger.log(LogLevel::INFO, "MINIMAX: Search complete - depth=" + std::to_string(max_depth_reached) +
                    ", nodes=" + std::to_string(nodes_searched) + 
                    ", TT_size=" + std::to_string(tt.size()) +
                    ", time=" + std::to_string(timeManager.getElapsedTime()) + "s");

        
        g_logger.logDepthStatistics();

        return bestResult.bestMove;
    }
    
private:
    
    // ========== MOVE GENERATION HELPER METHODS ============

    struct MoveEvaluation {
        Move move;
        float quick_eval;
        
        MoveEvaluation(const Move& m, float eval) : move(m), quick_eval(eval) {}
        
        // Sort in descending order (best moves first)
        bool operator<(const MoveEvaluation& other) const {
            return quick_eval > other.quick_eval;
        }
    };
    
    // Quick evaluation of moves at root level
    std::vector<Move> selectTopRootMoves(const GameState& position, 
                                       const std::vector<Move>& allMoves,
                                       bool isCirclePlayer,
                                       int maxMoves = 32) { // Limit to top 16 moves
        
        if (allMoves.size() <= maxMoves) {
            return allMoves; // No need to prune if we have few moves
        }
        
        std::vector<MoveEvaluation> evaluatedMoves;
        evaluatedMoves.reserve(allMoves.size());
        
        g_logger.log(LogLevel::DEBUG, "ROOT PRUNING: Evaluating " + std::to_string(allMoves.size()) + 
                    " moves to select top " + std::to_string(maxMoves));
        
        // Quick evaluate each move
        for (const auto& move : allMoves) {
            GameState testPosition = position.clone();
            
            if (applyMoveToPosition(testPosition, move)) {
                // Quick evaluation after applying move
                float eval = evaluator->EvaluateBoard(testPosition, isCirclePlayer);
                evaluatedMoves.emplace_back(move, eval);
            }
        }
        
        // Sort by evaluation (best first)
        std::sort(evaluatedMoves.begin(), evaluatedMoves.end());
        
        // Extract top moves
        std::vector<Move> topMoves;
        topMoves.reserve(maxMoves);
        
        for (int i = 0; i < std::min(maxMoves, (int)evaluatedMoves.size()); ++i) {
            topMoves.push_back(evaluatedMoves[i].move);
            
            if (i < 5) { // Log top 5 moves
                g_logger.log(LogLevel::DEBUG, "ROOT PRUNING: Top move " + std::to_string(i+1) + 
                            ": " + evaluatedMoves[i].move.action + " (eval: " + 
                            std::to_string(evaluatedMoves[i].quick_eval) + ")");
            }
        }
        
        g_logger.log(LogLevel::DEBUG, "ROOT PRUNING: Selected " + std::to_string(topMoves.size()) + 
                    "/" + std::to_string(allMoves.size()) + " moves for deep search");
        
        return topMoves;
    }

    // ========== PRINCIPAL VARIATION HELPER METHODS ==========
    
    // Get PV move for a position at a specific depth from previous iteration
    Move getPVMove(uint64_t position_hash, int depth) const {
        if (depth >= 0 && depth < pv_moves.size()) {
            auto it = pv_moves[depth].find(position_hash);
            if (it != pv_moves[depth].end()) {
                return it->second;
            }
        }
        return Move();  // Return empty move if not found
    }
    
    // Store PV move for a position at a specific depth
    void storePVMove(uint64_t position_hash, int depth, const Move& move) {
        if (depth >= 0 && depth < pv_moves.size()) {
            pv_moves[depth][position_hash] = move;
        }
    }
    
    // Clear PV moves for new search
    void clearPVMoves() {
        for (auto& pv_map : pv_moves) {
            pv_map.clear();
        }
    }
    
    // Order moves using PV from previous iteration
    std::vector<Move> orderMovesWithPV(const std::vector<Move>& moves, uint64_t position_hash, 
                                      int current_depth) const {
        if (moves.empty()) return moves;
        
        std::vector<Move> ordered_moves = moves;
        
        // Get PV move from previous iteration at this depth
        Move pv_move = getPVMove(position_hash, current_depth);
        
        if (!pv_move.action.empty()) {
            // Find PV move in current moves and move to front
            auto it = std::find(ordered_moves.begin(), ordered_moves.end(), pv_move);
            if (it != ordered_moves.end()) {
                std::swap(*it, ordered_moves[0]);
                g_logger.log(LogLevel::DEBUG, "MINIMAX: Using PV move at depth " + 
                            std::to_string(current_depth) + ": " + pv_move.action);
                return ordered_moves;
            }
        }
        
        // If no PV move found, try TT move ordering as fallback
        Move tt_move;
        float dummy_eval;
        if (tt.probe(position_hash, current_depth, -10000000.0f, 10000000.0f, dummy_eval, tt_move)) {
            if (!tt_move.action.empty()) {
                auto it = std::find(ordered_moves.begin(), ordered_moves.end(), tt_move);
                if (it != ordered_moves.end()) {
                    std::swap(*it, ordered_moves[0]);
                    g_logger.log(LogLevel::DEBUG, "MINIMAX: Using TT move at depth " + 
                                std::to_string(current_depth) + ": " + tt_move.action);
                }
            }
        }
        
        return ordered_moves;
    }
    
    // CORRECTED searchAtDepth method
    SearchResult searchAtDepth(const GameState& position, int depth, bool isCirclePlayer, 
                            const std::vector<Move>& rootMoves) {
        
        SearchResult bestResult;
        bestResult.evaluation = -10000000.0f;  // Negative infinity
        bestResult.bestMove = rootMoves[0];   // Default move
        bestResult.depth_reached = depth;
        
        float alpha = -10000000.0f;  // Initialize alpha at root
        float beta = 10000000.0f;    // Initialize beta at root
        int moves_evaluated = 0;
        int cutoffs = 0;
        
        // Order root moves using PV from previous iteration
        uint64_t root_hash = tt.computeHash(position, isCirclePlayer);
        std::vector<Move> orderedMoves = orderMovesWithPV(rootMoves, root_hash, 0);
        
        g_logger.log(LogLevel::DEBUG, "MINIMAX: Ordered " + std::to_string(orderedMoves.size()) + 
                    " root moves for depth " + std::to_string(depth));
        
        for (size_t i = 0; i < orderedMoves.size(); ++i) {  // Change int to size_t and add explicit loop variable
            const Move& move = orderedMoves[i];
            if (timeManager.shouldStop()) {
                bestResult.timeout_occurred = true;
                g_logger.logHierarchical(0, "TIMEOUT after " + std::to_string(moves_evaluated) + 
                                        "/" + std::to_string(orderedMoves.size()) + " moves");
                break;
            }

            std::string move_desc = move.action;
            if (!move.from.empty() && move.from.size() >= 2) {
                move_desc += " (" + std::to_string(move.from[0]) + "," + std::to_string(move.from[1]) + ")";
            }
            if (!move.to.empty() && move.to.size() >= 2) {
                move_desc += "->(" + std::to_string(move.to[0]) + "," + std::to_string(move.to[1]) + ")";
            }
            
            g_logger.enterDepth(0, i, orderedMoves.size(), move_desc);
            g_logger.logAlphaBeta(0, alpha, beta, "[ROOT]");
            
            // Apply move to get new position
            GameState newPosition = position.clone();
            if (!applyMoveToPosition(newPosition, move)) {
                g_logger.logHierarchical(0, "INVALID MOVE - skipped");
                continue;  // Invalid move, skip
            }
            
            moves_evaluated++;
            

            // NEW: Check for immediate win at root level
            std::string winner_after_move = newPosition.getWinner();
            if (!winner_after_move.empty()) {
                bool we_won = (winner_after_move == "circle" && isCirclePlayer) || 
                            (winner_after_move == "square" && !isCirclePlayer);
                
                if (we_won) {
                    // IMMEDIATE WIN AT ROOT! Return this move immediately
                    float immediate_win_score = 1000000.0f + depth;
                    
                    g_logger.logHierarchical(0, "IMMEDIATE WIN AT ROOT! " + winner_after_move + 
                                        " wins with move: " + move_desc);
                    g_logger.log(LogLevel::INFO, "SEARCH TERMINATED: Found immediate winning move!");
                    
                    bestResult.evaluation = immediate_win_score;
                    bestResult.bestMove = move;
                    return bestResult; // Return immediately, no need to search further!
                } else {
                    // This root move loses immediately - skip it
                    g_logger.logHierarchical(0, "ROOT MOVE LOSES IMMEDIATELY - skipped: " + move_desc);
                    continue;
                }
            }
            
            

            // Search this branch with current alpha-beta window
            float evaluation = -negamax(newPosition, depth - 1, -beta, -alpha, !isCirclePlayer, 1);
            
            g_logger.logHierarchical(0, "eval=" + std::to_string(evaluation));
            
            // Update best move if this is better
            if (evaluation > bestResult.evaluation) {
                g_logger.logBestMoveChange(0, move_desc, bestResult.evaluation, evaluation);
                bestResult.evaluation = evaluation;
                bestResult.bestMove = move;
            }
            
            // Update alpha for next root moves (critical for pruning!)
            float old_alpha = alpha;
            alpha = std::max(alpha, evaluation);
            
            if (alpha > old_alpha) {
                g_logger.logHierarchical(0, "α improved: " + std::to_string(old_alpha) + 
                                        " → " + std::to_string(alpha));
            }
            
            // Beta cutoff at root level (though rare with initial beta=+infinity)
            if (beta <= alpha) {
                cutoffs++;
                g_logger.log(LogLevel::DEBUG, "MINIMAX: Root beta cutoff! Remaining " + 
                            std::to_string(rootMoves.size() - moves_evaluated) + " moves pruned");
                break;  // Remaining root moves can be pruned
            }
        }
        
        g_logger.log(LogLevel::DEBUG, "MINIMAX: Root search complete - evaluated=" + 
                    std::to_string(moves_evaluated) + "/" + std::to_string(rootMoves.size()) + 
                    ", cutoffs=" + std::to_string(cutoffs));
        
        return bestResult;
    }
    
    // Negamax algorithm with alpha-beta pruning, TT, and PV storage
    float negamax(const GameState& position, int depth, float alpha, float beta, bool isCirclePlayer, 
                  int current_depth = 0) {
        nodes_searched++;
        
        // Check for timeout
        if (timeManager.shouldStop()) {
            return 0.0f;
        }
        
        // Compute position hash
        uint64_t position_hash = tt.computeHash(position, isCirclePlayer);
        
        // Probe transposition table
        float tt_evaluation;
        Move tt_best_move;
        if (tt.probe(position_hash, depth, alpha, beta, tt_evaluation, tt_best_move)) {
            if (current_depth <= 4) {
                g_logger.logHierarchical(current_depth, "TT HIT: eval=" + std::to_string(tt_evaluation) + 
                                        ", move=" + tt_best_move.action);
            }
            return tt_evaluation;
        }
        
        // Terminal conditions
        if (depth == 0) {
            float evaluation = evaluator->EvaluateBoard(position, isCirclePlayer);

            // Log leaf evaluation
            if (current_depth <= 6) {
                std::string player_info = isCirclePlayer ? "[circle]" : "[square]";
                g_logger.logEvaluation(current_depth, evaluation, "LEAF " + player_info);
            }
            // Store leaf evaluation in TT
            Move dummy_move;
            tt.store(position_hash, evaluation, dummy_move, depth, alpha, beta);
            return evaluation;
        }
        
        // Check for game ending (win condition)
        std::string winner = position.getWinner();
        if (!winner.empty()) {
            float terminal_eval;
            if ((winner == "circle" && isCirclePlayer) || (winner == "square" && !isCirclePlayer)) {
                terminal_eval = 1000000.0f + depth;  // Win bonus for shorter paths
            } else {
                terminal_eval = -1000000.0f - depth;  // Loss penalty
            }
            
            if (current_depth <= 6) {
                g_logger.logEvaluation(current_depth, terminal_eval, "TERMINAL: " + winner + " wins");
            }
            
            return terminal_eval;
        }
        
        // Generate moves for current position
        std::string currentPlayer = isCirclePlayer ? "circle" : "square";
        std::vector<Move> moves = generateMovesForPosition(position, currentPlayer);

        if (moves.empty()) {
            // No moves available - likely a loss
            float evaluation = -50.0f;
            Move dummy_move;
            tt.store(position_hash, evaluation, dummy_move, depth, alpha, beta);
            return evaluation;
        }
        
        // Order moves using PV from previous iteration + TT move
        std::vector<Move> orderedMoves = orderMovesWithPV(moves, position_hash, current_depth);
        
        float maxEval = -10000000.0f;
        Move best_move = orderedMoves[0];  // Default best move
        float original_alpha = alpha;
        
        int moves_tried = 0;
        for (int i = 0; i < orderedMoves.size(); ++i) {
            const Move& move = orderedMoves[i];
            
            if (timeManager.shouldStop()) break;
            
            // Create move description for logging
            std::string move_desc = move.action;
            if (!move.from.empty() && move.from.size() >= 2) {
                move_desc += " (" + std::to_string(move.from[0]) + "," + std::to_string(move.from[1]) + ")";
            }
            if (!move.to.empty() && move.to.size() >= 2) {
                move_desc += "->(" + std::to_string(move.to[0]) + "," + std::to_string(move.to[1]) + ")";
            }
            
            // Log entry into this child node (only for depths we care about)
            if (current_depth <= 4) {  // Avoid excessive logging at deep levels
                g_logger.enterDepth(current_depth, i, orderedMoves.size(), move_desc);
                g_logger.logAlphaBeta(current_depth, alpha, beta);
            }
            
            // Apply move
            GameState newPosition = position.clone();
            if (!applyMoveToPosition(newPosition, move)) {
                if (current_depth <= 4) {
                    g_logger.logHierarchical(current_depth, "INVALID MOVE - skipped");
                }
                continue;  // Invalid move
            }
            
            moves_tried++;
            


            // Check for immediate win after applying this move
            std::string winner_after_move = newPosition.getWinner();
            if (!winner_after_move.empty()) {
                bool we_won = (winner_after_move == "circle" && isCirclePlayer) || 
                            (winner_after_move == "square" && !isCirclePlayer);
                
                if (we_won) {
                    // IMMEDIATE WIN! Return huge positive score and propagate this move up
                    float immediate_win_score = 1000000.0f + depth; // Higher score for quicker wins
                    
                    if (current_depth <= 4) {
                        g_logger.logHierarchical(current_depth, "IMMEDIATE WIN DETECTED! " + 
                                            winner_after_move + " wins with move: " + move_desc);
                    }
                    
                    // Store this winning move in TT and PV
                    tt.store(position_hash, immediate_win_score, move, depth, original_alpha, beta);
                    storePVMove(position_hash, current_depth, move);
                    
                    return immediate_win_score;
                } else {
                    // Opponent wins - this move loses immediately, skip it
                    if (current_depth <= 4) {
                        g_logger.logHierarchical(current_depth, "IMMEDIATE LOSS - move skipped: " + move_desc);
                    }
                    continue;
                }
            }

            // Check for immediate goal zone scoring moves to not miss obvious best moves

            bool creates_goal_score = checkForGoalZoneScoring(position, newPosition, move, isCirclePlayer);
            if (creates_goal_score) {
                // This move puts a stone in our goal zone - very high priority!
                float goal_zone_bonus = 750000.0f + depth; // Very high score but less than immediate win
                
                if (current_depth <= 4) {
                    g_logger.logHierarchical(current_depth, "GOAL ZONE SCORING MOVE DETECTED: " + move_desc);
                }
                
                // Store this high-value move and strongly consider it
                tt.store(position_hash, goal_zone_bonus, move, depth, original_alpha, beta);
                storePVMove(position_hash, current_depth, move);
                
                // Still continue search to see if there are even better moves, but this is very promising
                maxEval = goal_zone_bonus;
                best_move = move;
                alpha = std::max(alpha, goal_zone_bonus);
                
                // If this move is so good it causes a cutoff, take it immediately
                if (beta <= alpha) {
                    if (current_depth <= 4) {
                        g_logger.logCutoff(current_depth, moves_tried, orderedMoves.size(), alpha, beta);
                        g_logger.logHierarchical(current_depth, "GOAL ZONE MOVE CAUSES CUTOFF!");
                    }
                    return goal_zone_bonus;
                }
                
                continue; // Don't do recursive search for this move, we already know it's excellent
            }

            // Recursive search
            float evaluation = -negamax(newPosition, depth - 1, -beta, -alpha, !isCirclePlayer, current_depth + 1);
            
            // Log the evaluation result
            if (current_depth <= 4) {
                g_logger.logHierarchical(current_depth, "child returned " + std::to_string(evaluation));
            }
            
            if (evaluation > maxEval) {
                if (current_depth <= 4) {
                    g_logger.logBestMoveChange(current_depth, move_desc, maxEval, evaluation);
                }
                maxEval = evaluation;
                best_move = move;
            }
            
            alpha = std::max(alpha, evaluation);
            
            if (beta <= alpha) {
                if (current_depth <= 4) {
                    g_logger.logCutoff(current_depth, moves_tried, orderedMoves.size(), alpha, beta);
                }
                break;  // Alpha-beta cutoff
            }
        }
        
        // Store result in transposition table
        tt.store(position_hash, maxEval, best_move, depth, original_alpha, beta);
        
        // Store PV move if we have a best move
        if (!best_move.action.empty()) {
            storePVMove(position_hash, current_depth, best_move);
        }
        
        return maxEval;
    }
    
    // Generate moves for a given position and player
    std::vector<Move> generateMovesForPosition(const GameState& position, const std::string& player) {
        // Use optimized move generation that leverages position sets
        // return moveGenerator->generateAllMovesOptimized(position, player);
        return moveGenerator->generateAllMovesOptimizedV2(position, player);
    }
    

    // Check if a move results in scoring a stone in our goal zone
    bool checkForGoalZoneScoring(const GameState& beforeState, const GameState& afterState, 
                                const Move& move, bool isCirclePlayer) const {
        
        // Get our goal zone row
        int our_goal_row = isCirclePlayer ? 2 : (beforeState.getRows() - 3);
        const auto& score_cols = beforeState.getScoreCols();
        
        // Check if we gained any stones in our scoring area
        int stones_before = 0, stones_after = 0;
        
        for (int col : score_cols) {
            // Count stones in goal zone before move
            if (beforeState.inBounds(col, our_goal_row)) {
                uint8_t piece_before = beforeState.getPiece(col, our_goal_row);
                if (::isStone(piece_before) && 
                    ((isCirclePlayer && ::isCircle(piece_before)) || (!isCirclePlayer && ::isSquare(piece_before)))) {
                    stones_before++;
                }
            }
            
            // Count stones in goal zone after move
            if (afterState.inBounds(col, our_goal_row)) {
                uint8_t piece_after = afterState.getPiece(col, our_goal_row);
                if (::isStone(piece_after) && 
                    ((isCirclePlayer && ::isCircle(piece_after)) || (!isCirclePlayer && ::isSquare(piece_after)))) {
                    stones_after++;
                }
            }
        }
        
        // Return true if we gained stones in our goal zone
        bool gained_goal_stones = stones_after > stones_before;
        
        if (gained_goal_stones) {
            g_logger.log(LogLevel::DEBUG, "GOAL ZONE DETECTION: Move " + move.action + 
                        " increases goal stones from " + std::to_string(stones_before) + 
                        " to " + std::to_string(stones_after));
        }
        
        return gained_goal_stones;
    }

    // Apply a move to a GameState position
    bool applyMoveToPosition(GameState& position, const Move& move) {
        try {
            if (move.action == "move") {
                position.applyBasicMove(move.from[0], move.from[1], move.to[0], move.to[1]);
                return true;
            }
            else if (move.action == "flip") {
                if (!move.orientation.empty()) {
                    position.applyFlip(move.from[0], move.from[1], move.orientation);
                } else {
                    position.applyFlip(move.from[0], move.from[1]);
                }
                return true;
            }
            else if (move.action == "rotate") {
                position.applyRotate(move.from[0], move.from[1]);
                return true;
            }
            else if (move.action == "push") {
                // For push moves, we need to handle the push logic
                // This is more complex and depends on the specific push implementation
                // For now, we'll treat it as a basic move
                position.applyBasicMove(move.from[0], move.from[1], move.to[0], move.to[1]);
                return true;
            }
            
            return false;  // Unknown action
        }
        catch (...) {
            return false;  // Move application failed
        }
    }
    
public:
    // Get search statistics
    int getNodesSearched() const { return nodes_searched; }
    int getMaxDepthReached() const { return max_depth_reached; }
    
    // Clear transposition table (for testing or new games)
    void clearTT() { tt.clear(); }
};


// ==================== STUDENT AGENT ENGINE ====================
class StudentAgent {
private:
    std::string side;                    // Player side ("circle" or "square")
    std::random_device rd;              // Random device
    std::mt19937 gen;                   // Random number generator
    MoveGenerator moveGen;               // High-performance move generator
    BoardEvaluator evaluator;            // Board evaluation system
    MinimaxEngine* searchEngine;         // Basic minimax search engine
    GameState gameState;                 // Current game state representation

public:
    explicit StudentAgent(std::string side) : side(std::move(side)), gen(rd()), gameState(5, 5) {
        // Initialize basic search engine
        g_logger.enableLogging(false);
        searchEngine = new MinimaxEngine(&evaluator, &moveGen);
    }
    
    ~StudentAgent() {
        delete searchEngine;
    }

    Move choose(const std::vector<std::vector<std::map<std::string, std::string>>>& board, int row, int col, 
        const std::vector<int>& score_cols, float current_player_time, float opponent_time) {
        
        // Start new move logging
        g_logger.nextMove();
        
        int rows = board.size();
        int cols = board[0].size();
        
        // Log move start
        std::stringstream move_info;
        move_info << "=== MOVE DECISION START === "
                 << "Player: " << side << ", "
                 << "Board: " << rows << "x" << cols << ", "
                 << "Time: " << current_player_time << "s (vs " << opponent_time << "s)";
        g_logger.log(LogLevel::DECISION, move_info.str());

        // Convert board to GameState for minimax search
        gameState = GameState(rows, cols);
        gameState.loadFromPython(board);
        
        std::string chosen_reasoning = "Minimax search";
        
        // Use basic minimax search to find best move
        try {
            Move bestMove = searchEngine->getBestMove(gameState, side, 
                                                     current_player_time, opponent_time);
            
            const auto& moves = moveGen.generateAllMovesOptimizedV2(gameState,side);

            g_logger.log(LogLevel::DEBUG, "VALIDATION DEBUG: Generated " + std::to_string(moves.size()) + 
                        " legal moves for validation");
            g_logger.log(LogLevel::DEBUG, "VALIDATION DEBUG: Minimax returned move: " + 
                        bestMove.action + " (" + std::to_string(bestMove.from[0]) + "," + 
                        std::to_string(bestMove.from[1]) + ") -> (" + 
                        std::to_string(bestMove.to[0]) + "," + std::to_string(bestMove.to[1]) + ")");
            
            // Log first few legal moves for comparison
            for (size_t i = 0; i < std::min((size_t)5, moves.size()); ++i) {
                const auto& move = moves[i];
                g_logger.log(LogLevel::DEBUG, "VALIDATION DEBUG: Legal move " + std::to_string(i+1) + ": " + 
                            move.action + " (" + std::to_string(move.from[0]) + "," + 
                            std::to_string(move.from[1]) + ") -> (" + 
                            std::to_string(move.to[0]) + "," + std::to_string(move.to[1]) + ")");
            }
            
            for (const auto& move : moves) {
                if (move == bestMove) {
                    g_logger.log(LogLevel::DEBUG, "VALIDATION DEBUG: Move found in legal moves - SUCCESS!");
                    g_logger.logDecision(g_logger.moveToString(bestMove), chosen_reasoning);
                    g_logger.log(LogLevel::DECISION, "=== MOVE DECISION END ===");
                    return bestMove;
                }
            }
            
            g_logger.log(LogLevel::DEBUG, "VALIDATION DEBUG: Move NOT found in legal moves - FAILED!");
            
            // Fallback: if minimax move is invalid, use first legal move
            if (!moves.empty()) {
                // chosen_reasoning = "Minimax move invalid, using first legal move";
                g_logger.log(LogLevel::ERROR, "Minimax returned invalid move, using fallback");
                g_logger.logDecision(g_logger.moveToString(moves[0]), chosen_reasoning);
                g_logger.log(LogLevel::DECISION, "=== MOVE DECISION END ===");
                return moves[0];
            }
        } catch (const std::exception& e) {
            // Fallback to evaluation-based selection on any search error
            chosen_reasoning = "Minimax failed with exception: " + std::string(e.what());
            g_logger.log(LogLevel::ERROR, "Minimax search failed: " + std::string(e.what()));
        } catch (...) {
            chosen_reasoning = "Minimax failed with unknown exception";
            g_logger.log(LogLevel::ERROR, "Minimax search failed with unknown exception");
        }
        
        // FALLBACK: Use evaluation-based move selection  
        g_logger.log(LogLevel::INFO, "Using evaluation-based fallback selection");
        
        
        const auto& moves = moveGen.generateAllMovesOptimizedV2(gameState,side);
        
        if (moves.empty()) {
            chosen_reasoning = "No legal moves available - emergency fallback";
            g_logger.log(LogLevel::ERROR, "No legal moves found!");
            g_logger.logDecision("emergency fallback", chosen_reasoning);
            g_logger.log(LogLevel::DECISION, "=== MOVE DECISION END ===");
            return {"move", {0,0}, {0,0}, {}, ""}; // fallback
        }
        
        bool isCirclePlayer = (side == "circle");
        Move bestMove = moves[0];
        float bestScore = -1000000.0f;
        
        // Evaluate each move and pick the best one
        for (const auto& move : moves) {
            GameState tempState = gameState;
            float score = evaluator.EvaluateBoard(tempState, isCirclePlayer);
            
            // Add small random factor for variety when moves are equally good
            std::uniform_real_distribution<float> random_factor(-0.01f, 0.01f);
            score += random_factor(gen);
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        
        // Log final decision
        chosen_reasoning = "Evaluation-based selection (score: " + std::to_string(bestScore) + ")";
        g_logger.logDecision(g_logger.moveToString(bestMove), chosen_reasoning);
        g_logger.log(LogLevel::DECISION, "=== MOVE DECISION END ===");
        
        return bestMove;
    }
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
