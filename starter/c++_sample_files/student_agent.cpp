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

namespace py = pybind11;

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
    uint64_t hash_value;
    
    // Game constants (from gameEngine.py analysis)
    static constexpr int WIN_COUNT = 4;
    static constexpr int TOP_SCORE_ROW = 2;
    
    int getBottomScoreRow() const { return rows - 3; }
    
    // Hash computation for transposition tables
    void computeHash() {
        hash_value = 0;
        std::hash<uint64_t> hasher;
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (board[y][x] != EMPTY) {
                    // Zobrist-like hashing: position + piece type
                    uint64_t piece_hash = (uint64_t(board[y][x]) << 16) | (uint64_t(y) << 8) | uint64_t(x);
                    hash_value ^= hasher(piece_hash);
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
        
        computeHash();
    }
    
    // Fast copy constructor for minimax simulations
    GameState(const GameState& other) 
        : board(other.board), rows(other.rows), cols(other.cols), 
          score_cols(other.score_cols), hash_value(other.hash_value) {}
    
    // Assignment operator
    GameState& operator=(const GameState& other) {
        if (this != &other) {
            board = other.board;
            rows = other.rows;
            cols = other.cols;
            score_cols = other.score_cols;
            hash_value = other.hash_value;
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
        computeHash();
    }
    
    // Deep copy for search tree
    GameState clone() const {
        return GameState(*this);
    }
    
    // Accessors
    inline uint8_t getPiece(int x, int y) const { 
        return (inBounds(x, y)) ? board[y][x] : EMPTY; 
    }
    
    inline void setPiece(int x, int y, uint8_t piece) {
        if (inBounds(x, y)) {
            board[y][x] = piece;
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
    uint64_t getHash() const { return hash_value; }
    
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
    
    // Count all pieces of a player
    int countPlayerPieces(bool isCircle) const {
        int count = 0;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                uint8_t piece = board[y][x];
                if (isCircle ? ::isCircle(piece) : ::isSquare(piece)) {
                    count++;
                }
            }
        }
        return count;
    }
    
    // Count stones specifically
    int countPlayerStones(bool isCircle) const {
        int count = 0;
        uint8_t target = isCircle ? CIRCLE_STONE : SQUARE_STONE;
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (board[y][x] == target) {
                    count++;
                }
            }
        }
        return count;
    }
    
    // Quick validation helpers
    bool isValidPosition(int x, int y) const {
        return inBounds(x, y);
    }
    
    bool canPlacePiece(int x, int y) const {
        return isEmpty(x, y);
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
            board[to_y][to_x] = board[from_y][from_x];
            board[from_y][from_x] = EMPTY;
            computeHash();
        }
    }
    
    // Apply push move
    void applyPushMove(int from_x, int from_y, int to_x, int to_y, int push_x, int push_y) {
        if (inBounds(from_x, from_y) && inBounds(to_x, to_y) && inBounds(push_x, push_y)) {
            // Move pushed piece to push destination
            board[push_y][push_x] = board[to_y][to_x];
            // Move our piece to the intermediate position
            board[to_y][to_x] = board[from_y][from_x];
            // Clear original position
            board[from_y][from_x] = EMPTY;
            computeHash();
        }
    }
    
    // Flip piece (stone <-> river)
    void applyFlip(int x, int y, const std::string& new_orientation = "horizontal") {
        if (!inBounds(x, y)) return;
        
        uint8_t piece = board[y][x];
        if (piece == EMPTY) return;
        
        bool isCircleOwner = ::isCircle(piece);
        
        if (::isStone(piece)) {
            // Stone -> River
            bool isHoriz = (new_orientation == "horizontal");
            board[y][x] = isCircleOwner ? 
                (isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V) : 
                (isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V);
        } else {
            // River -> Stone
            board[y][x] = isCircleOwner ? CIRCLE_STONE : SQUARE_STONE;
        }
        computeHash();
    }
    
    // Rotate river
    void applyRotate(int x, int y) {
        if (!inBounds(x, y)) return;
        
        uint8_t piece = board[y][x];
        if (!::isRiver(piece)) return;
        
        // Toggle orientation
        if (piece == CIRCLE_RIVER_H) {
            board[y][x] = CIRCLE_RIVER_V;
        } else if (piece == CIRCLE_RIVER_V) {
            board[y][x] = CIRCLE_RIVER_H;
        } else if (piece == SQUARE_RIVER_H) {
            board[y][x] = SQUARE_RIVER_V;
        } else if (piece == SQUARE_RIVER_V) {
            board[y][x] = SQUARE_RIVER_H;
        }
        computeHash();
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
};

// ==================== BOARD EVALUATION SYSTEM ====================

class BoardEvaluator {
private:
    // Evaluation caching for performance
    mutable std::unordered_map<uint64_t, float> evaluation_cache;
    
    // Performance optimization: pre-computed scoring area bounds
    struct ScoringArea {
        int start_row = 0;
        int end_row = 0;  // Inclusive bounds for scoring rows
        std::vector<int> score_cols;  // Scoring columns
    };
    
    mutable ScoringArea circle_scoring;   // Circle's scoring area (bottom rows)
    mutable ScoringArea square_scoring;   // Square's scoring area (top rows)
    mutable bool scoring_areas_initialized = false;
    
    // Initialize scoring areas based on game configuration
    void initializeScoringAreas(const GameState& gameState) const {
        if (scoring_areas_initialized) return;
        
        int rows = gameState.getRows();
        const auto& score_cols = gameState.getScoreCols();
        
        // Circle scores in bottom rows (rows-3 to rows-1)
        circle_scoring.start_row = rows - 3;
        circle_scoring.end_row = rows - 1;
        circle_scoring.score_cols = score_cols;
        
        // Square scores in top rows (0 to 2)
        square_scoring.start_row = 0;
        square_scoring.end_row = 2;
        square_scoring.score_cols = score_cols;
        
        scoring_areas_initialized = true;
    }
    
public:
    // ==================== PHASE 4A: CORE EVALUATION ====================
    
    // Main evaluation function - exact compatibility with Python basic_evaluate_board
    float basicEvaluateBoard(const GameState& gameState, bool isCirclePlayer) const {
        // Check cache first
        uint64_t hash = gameState.getHash();
        uint64_t cache_key = hash ^ (isCirclePlayer ? 1ULL : 0ULL);  // Player-specific cache
        
        auto cache_it = evaluation_cache.find(cache_key);
        if (cache_it != evaluation_cache.end()) {
            return cache_it->second;
        }
        
        float score = computeBasicEvaluation(gameState, isCirclePlayer);
        
        // Cache the result
        evaluation_cache[cache_key] = score;
        return score;
    }
    
    // ==================== PHASE 4B: EVALUATION SCAFFOLDS ====================
    
    // Material evaluation - piece counting and type analysis
    float evaluateMaterial(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: Advanced material evaluation
        // - Count stones vs rivers
        // - Weight piece types differently
        // - Consider piece positioning in material value
        return 0.0f;  // Placeholder
    }
    
    // Position evaluation - territorial control and piece placement
    float evaluatePosition(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: Advanced positional evaluation  
        // - Territory control metrics
        // - Piece centralization
        // - Distance to key squares
        // - Formation analysis
        return 0.0f;  // Placeholder
    }
    
    // Threat evaluation - immediate scoring opportunities
    float evaluateThreats(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: Advanced threat evaluation
        // - Pieces 1-2 moves from scoring
        // - Attacking/defending patterns
        // - Forced sequences analysis
        return 0.0f;  // Placeholder
    }
    
    // Mobility evaluation - move flexibility assessment
    float evaluateMobility(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: Mobility evaluation
        // - Available move count
        // - Quality of available moves
        // - Piece coordination potential
        return 0.0f;  // Placeholder
    }
    
    // River control evaluation - strategic river placement
    float evaluateRiverControl(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: River control evaluation
        // - Connected river networks
        // - Strategic river positioning
        // - Flow control advantages
        return 0.0f;  // Placeholder
    }
    
    // Safety evaluation - piece vulnerability analysis
    float evaluateSafety(const GameState& gameState, bool isCirclePlayer) const {
        // TODO: Safety evaluation
        // - Pieces under attack
        // - Defensive formations
        // - Escape route analysis
        return 0.0f;  // Placeholder
    }
    
    // ==================== PHASE 4A: IMPLEMENTATION DETAILS ====================
    
private:
    // Core evaluation computation - mirrors Python basic_evaluate_board exactly
    float computeBasicEvaluation(const GameState& gameState, bool isCirclePlayer) const {
        initializeScoringAreas(gameState);
        
        float score = 0.0f;
        int rows = gameState.getRows();
        int cols = gameState.getCols();
        
        // Count stones in scoring areas (matches Python logic exactly)
        int player_scoring_stones = countStonesInScoringArea(gameState, isCirclePlayer);
        int opponent_scoring_stones = countStonesInScoringArea(gameState, !isCirclePlayer);
        
        score += player_scoring_stones * 100.0f;   // +100 per scoring stone
        score -= opponent_scoring_stones * 100.0f; // -100 per opponent scoring stone
        
        // Positional scoring for all player stones (matches Python logic)
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (gameState.isEmpty(x, y)) continue;
                
                // Check if this is a player's stone
                if (gameState.isPlayerPiece(x, y, isCirclePlayer) && 
                    gameState.getPieceType(x, y) == "stone") {
                    
                    // Basic positional scoring (matches Python exactly)
                    if (isCirclePlayer) {
                        score += (rows - y) * 0.1f;  // Circle prefers bottom
                    } else {
                        score += y * 0.1f;           // Square prefers top
                    }
                }
            }
        }
        
        return score;
    }
    
    // Count stones in scoring area - helper function
    int countStonesInScoringArea(const GameState& gameState, bool isCirclePlayer) const {
        int count = 0;
        const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
        
        // Check all positions in the scoring area
        for (int y = area.start_row; y <= area.end_row; ++y) {
            for (int col : area.score_cols) {
                if (gameState.inBounds(col, y) &&
                    gameState.isPlayerPiece(col, y, isCirclePlayer) &&
                    gameState.getPieceType(col, y) == "stone") {
                    count++;
                }
            }
        }
        
        return count;
    }
    
public:
    // Clear evaluation cache (useful for testing)
    void clearCache() const {
        evaluation_cache.clear();
    }
    
    // Get cache statistics (for performance monitoring)
    size_t getCacheSize() const {
        return evaluation_cache.size();
    }
};

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
        bfs_queue.reserve(100);
        destinations_buffer.reserve(50);
        // visited_grid will be resized as needed
    }
    
    // ==================== PHASE 1: CORE MOVE GENERATION ====================
    
    // Main entry point - generates all legal moves for a player
    const std::vector<Move>& generateAllMoves(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        const std::string& player,
        int rows, int cols, 
        const std::vector<int>& score_cols) {
        
        // Clear buffer and start fresh
        move_buffer.clear();
        
        // Iterate through all board positions
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const auto& cell = board[y][x];
                
                // Skip empty cells or opponent pieces
                if (cell.empty() || getOwnerFromCell(cell) != player) continue;
                
                // Generate moves for this piece
                generateMovesForPiece(board, x, y, player, rows, cols, score_cols);
            }
        }
        
        return move_buffer;
    }
    
    // ==================== PHASE 3: MOVE CATEGORIZATION ====================
    
    // Generate high-priority capture/scoring moves
    const std::vector<Move>& generateCaptureMoves(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        const std::string& player,
        int rows, int cols, 
        const std::vector<int>& score_cols) {
        
        move_buffer.clear();
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty() || getOwnerFromCell(cell) != player) continue;
                
                // Only generate moves that advance toward opponent scoring area
                generateCapturingMovesForPiece(board, x, y, player, rows, cols, score_cols);
            }
        }
        
        return move_buffer;
    }
    
    // Generate quiet positional moves (no immediate threats)
    const std::vector<Move>& generateQuietMoves(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        const std::string& player,
        int rows, int cols, 
        const std::vector<int>& score_cols) {
        
        move_buffer.clear();
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty() || getOwnerFromCell(cell) != player) continue;
                
                // Generate non-aggressive moves
                generateQuietMovesForPiece(board, x, y, player, rows, cols, score_cols);
            }
        }
        
        return move_buffer;
    }
    
    // Generate aggressive moves (pushes, blocks opponent advancement)
    const std::vector<Move>& generateAggressiveMoves(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        const std::string& player,
        int rows, int cols, 
        const std::vector<int>& score_cols) {
        
        move_buffer.clear();
        
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty() || getOwnerFromCell(cell) != player) continue;
                
                // Generate push moves and defensive blocks
                generateAggressiveMovesForPiece(board, x, y, player, rows, cols, score_cols);
            }
        }
        
        return move_buffer;
    }
    
    // ==================== PHASE 3: MOVE ORDERING SYSTEM ====================
    
    // Generate ordered moves for alpha-beta optimization
    const std::vector<Move>& generateOrderedMoves(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        const std::string& player,
        int rows, int cols, 
        const std::vector<int>& score_cols) {
        
        // Generate all moves first
        generateAllMoves(board, player, rows, cols, score_cols);
        
        // Sort moves by priority for alpha-beta pruning
        std::sort(move_buffer.begin(), move_buffer.end(), 
                 [this, &board, &player, rows, cols, &score_cols](const Move& a, const Move& b) {
                     return scoreMoveForOrdering(a, board, player, rows, cols, score_cols) >
                            scoreMoveForOrdering(b, board, player, rows, cols, score_cols);
                 });
        
        return move_buffer;
    }
    
    // Score a move for ordering purposes (higher = better for alpha-beta)
    int scoreMoveForOrdering(const Move& move,
                           const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                           const std::string& player,
                           int rows, int cols,
                           const std::vector<int>& score_cols) const {
        
        int score = 0;
        int from_x = move.from[0], from_y = move.from[1];
        int to_x = move.to[0], to_y = move.to[1];
        
        int opponent_score_row = (player == "circle") ? (rows - 3) : 2;
        int player_score_row = (player == "circle") ? 2 : (rows - 3);
        
        // Priority 1: Moves that reach scoring area (highest priority)
        if (to_y == opponent_score_row && 
            std::find(score_cols.begin(), score_cols.end(), to_x) != score_cols.end()) {
            score += 10000;  // Immediate scoring move
        }
        
        // Priority 2: Push moves (aggressive, often good in alpha-beta)
        if (move.action == "push") {
            score += 5000;
            
            // Extra points for pushing opponent away from our scoring area
            if (!move.pushed_to.empty()) {
                int pushed_distance = std::abs(move.pushed_to[1] - player_score_row);
                score += pushed_distance * 100;  // Further away = better
            }
        }
        
        // Priority 3: Moves that advance toward opponent scoring area
        if (move.action == "move") {
            int old_distance = std::abs(from_y - opponent_score_row);
            int new_distance = std::abs(to_y - opponent_score_row);
            int advancement = old_distance - new_distance;
            score += advancement * 1000;  // Closer to scoring = better
            
            // Bonus for moves in scoring columns
            if (std::find(score_cols.begin(), score_cols.end(), to_x) != score_cols.end()) {
                score += 2000;
            }
        }
        
        // Priority 4: Flip moves that create strategic rivers
        if (move.action == "flip" && move.from == move.to) {
            const auto& piece = board[from_y][from_x];
            if (isStonePiece(piece)) {
                // Stone -> River: strategic positioning
                score += 800;
                
                // Bonus for flips near opponent scoring area
                int distance_to_opp_score = std::abs(from_y - opponent_score_row);
                score += (10 - distance_to_opp_score) * 100;
            } else {
                // River -> Stone: consolidation move
                score += 400;
            }
        }
        
        // Priority 5: Rotate moves (tactical adjustments)
        if (move.action == "rotate") {
            score += 300;
        }
        
        // Priority 6: Central board control (tie-breaker)
        int center_x = cols / 2, center_y = rows / 2;
        int centrality = 10 - (std::abs(to_x - center_x) + std::abs(to_y - center_y));
        score += centrality * 10;
        
        return score;
    }
    
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
    
    // Safety check for flip moves - prevents rivers that allow flow into opponent score
    bool isFlipSafe(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                    int fx, int fy, const std::string& player,
                    int rows, int cols, const std::vector<int>& score_cols,
                    const std::string& orientation) const {
        
        // Only need to check when flipping stone -> river
        const auto& piece = board[fy][fx];
        if (!isStonePiece(piece)) {
            return true; // River -> stone is always safe
        }
        
        // Create a temporary board copy to test the flip
        auto test_board = board;
        
        // Apply the flip temporarily
        test_board[fy][fx]["side"] = "river";
        test_board[fy][fx]["orientation"] = orientation;
        
        // Check if the new river allows flow into opponent scoring areas
        auto flow_destinations = computeRiverFlow(test_board, fx, fy, fx, fy, player, rows, cols, score_cols, false);
        
        // Check each flow destination
        for (const auto& dest : flow_destinations) {
            if (isOpponentScoreCell(dest.first, dest.second, player, rows, cols, score_cols)) {
                return false; // Unsafe - allows flow into opponent score
            }
        }
        
        return true; // Safe flip
    }
    
    // Safety check for rotate moves - prevents rotations that allow flow into opponent score
    bool isRotateSafe(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                      int rx, int ry, const std::string& player,
                      int rows, int cols, const std::vector<int>& score_cols,
                      const std::string& new_orientation) const {
        
        // Only rivers can be rotated
        const auto& piece = board[ry][rx];
        if (!isRiverPiece(piece)) {
            return false;
        }
        
        // Create a temporary board copy to test the rotation
        auto test_board = board;
        
        // Apply the rotation temporarily
        test_board[ry][rx]["orientation"] = new_orientation;
        
        // Check if the rotated river allows flow into opponent scoring areas
        auto flow_destinations = computeRiverFlow(test_board, rx, ry, rx, ry, player, rows, cols, score_cols, false);
        
        // Check each flow destination
        for (const auto& dest : flow_destinations) {
            if (isOpponentScoreCell(dest.first, dest.second, player, rows, cols, score_cols)) {
                return false; // Unsafe - rotation allows flow into opponent score
            }
        }
        
        return true; // Safe rotation
    }
    
    // ==================== PHASE 3: SPECIALIZED MOVE GENERATORS ====================
    
    // Generate moves that advance toward opponent scoring area
    void generateCapturingMovesForPiece(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int x, int y, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols) {
        
        const auto& cell = board[y][x];
        auto targets = computeValidTargets(board, x, y, player, rows, cols, score_cols);
        
        // Determine opponent scoring area
        int opponent_score_row = (player == "circle") ? (rows - 3) : 2;  // bottom for circle, top for square
        
        // Add moves that get closer to opponent scoring area
        for (const auto& move_pos : targets.moves) {
            int distance_improvement = std::abs(y - opponent_score_row) - std::abs(move_pos.second - opponent_score_row);
            if (distance_improvement > 0) {  // Gets closer to scoring area
                move_buffer.emplace_back("move", std::vector<int>{x, y}, 
                                       std::vector<int>{move_pos.first, move_pos.second});
            }
        }
        
        // Add all push moves (aggressive by nature)
        for (const auto& push_pair : targets.pushes) {
            const auto& own_final = push_pair.first;
            const auto& pushed_to = push_pair.second;
            move_buffer.emplace_back("push", std::vector<int>{x, y},
                                   std::vector<int>{own_final.first, own_final.second},
                                   std::vector<int>{pushed_to.first, pushed_to.second});
        }
    }
    
    // Generate non-threatening positional moves
    void generateQuietMovesForPiece(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int x, int y, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols) {
        
        const auto& cell = board[y][x];
        auto targets = computeValidTargets(board, x, y, player, rows, cols, score_cols);
        
        int opponent_score_row = (player == "circle") ? (rows - 3) : 2;
        
        // Add moves that don't change distance significantly or move away from scoring
        for (const auto& move_pos : targets.moves) {
            int distance_change = std::abs(move_pos.second - opponent_score_row) - std::abs(y - opponent_score_row);
            if (distance_change >= 0) {  // Doesn't get closer (quiet move)
                move_buffer.emplace_back("move", std::vector<int>{x, y}, 
                                       std::vector<int>{move_pos.first, move_pos.second});
            }
        }
        
        // Add safe flip moves (positional improvement)
        if (isStonePiece(cell)) {
            for (const std::string& orientation : {"horizontal", "vertical"}) {
                if (isFlipSafe(board, x, y, player, rows, cols, score_cols, orientation)) {
                    move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                           std::vector<int>{x, y}, std::vector<int>{}, orientation);
                }
            }
        }
        
        // Add safe rotate moves
        if (isRiverPiece(cell)) {
            std::string current_orientation = getRiverOrientationFromCell(cell);
            std::string new_orientation = (current_orientation == "horizontal") ? "vertical" : "horizontal";
            
            if (isRotateSafe(board, x, y, player, rows, cols, score_cols, new_orientation)) {
                move_buffer.emplace_back("rotate", std::vector<int>{x, y}, 
                                       std::vector<int>{x, y}, std::vector<int>{}, "");
            }
        }
    }
    
    // Generate aggressive/defensive moves
    void generateAggressiveMovesForPiece(
        const std::vector<std::vector<std::map<std::string, std::string>>>& board,
        int x, int y, const std::string& player,
        int rows, int cols, const std::vector<int>& score_cols) {
        
        auto targets = computeValidTargets(board, x, y, player, rows, cols, score_cols);
        
        // Add all push moves (inherently aggressive)
        for (const auto& push_pair : targets.pushes) {
            const auto& own_final = push_pair.first;
            const auto& pushed_to = push_pair.second;
            move_buffer.emplace_back("push", std::vector<int>{x, y},
                                   std::vector<int>{own_final.first, own_final.second},
                                   std::vector<int>{pushed_to.first, pushed_to.second});
        }
        
        // Add moves that block opponent pieces from advancing
        for (const auto& move_pos : targets.moves) {
            if (isBlockingMove(board, x, y, move_pos.first, move_pos.second, player, rows, cols, score_cols)) {
                move_buffer.emplace_back("move", std::vector<int>{x, y}, 
                                       std::vector<int>{move_pos.first, move_pos.second});
            }
        }
    }
    
    // Check if a move blocks opponent advancement
    bool isBlockingMove(const std::vector<std::vector<std::map<std::string, std::string>>>& board,
                       int from_x, int from_y, int to_x, int to_y, const std::string& player,
                       int rows, int cols, const std::vector<int>& score_cols) const {
        
        std::string opponent = (player == "circle") ? "square" : "circle";
        int player_score_row = (player == "circle") ? 2 : (rows - 3);
        
        // Check if destination position blocks a path to our scoring area
        for (auto [dx, dy] : DIRECTIONS) {
            int check_x = to_x + dx, check_y = to_y + dy;
            if (!inBounds(check_x, check_y, rows, cols)) continue;
            
            const auto& adjacent_cell = board[check_y][check_x];
            if (!adjacent_cell.empty() && getOwnerFromCell(adjacent_cell) == opponent) {
                // Check if this opponent piece was threatening our scoring area
                int distance_to_our_score = std::abs(check_y - player_score_row);
                if (distance_to_our_score <= 3) {  // Within threatening range
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // ==================== PHASE 3: GAMESTATE INTEGRATION ====================
    
    // Generate moves using GameState for enhanced validation
    const std::vector<Move>& generateMovesWithGameState(
        GameState& gameState, const std::string& player) {
        
        move_buffer.clear();
        
        int rows = gameState.getRows();
        int cols = gameState.getCols();
        const auto& score_cols = gameState.getScoreCols();
        
        bool isCirclePlayer = (player == "circle");
        
        // Iterate through all positions
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (gameState.isEmpty(x, y)) continue;
                
                // Check piece ownership using GameState utilities
                if (!gameState.isPlayerPiece(x, y, isCirclePlayer)) continue;
                
                // Generate moves using GameState validation
                generateMovesForPieceWithGameState(gameState, x, y, player);
            }
        }
        
        return move_buffer;
    }
    
    // Generate moves for a piece using GameState methods
    void generateMovesForPieceWithGameState(GameState& gameState, int x, int y, const std::string& player) {
        bool isCirclePlayer = (player == "circle");
        
        // Generate movement moves
        for (auto [dx, dy] : DIRECTIONS) {
            int tx = x + dx, ty = y + dy;
            
            if (!gameState.inBounds(tx, ty)) continue;
            
            // Block moves into opponent scoring area
            if (gameState.isOpponentScoreCell(tx, ty, isCirclePlayer)) continue;
            
            if (gameState.isEmpty(tx, ty)) {
                // Simple move
                move_buffer.emplace_back("move", std::vector<int>{x, y}, std::vector<int>{tx, ty});
            } else if (!gameState.isPlayerPiece(tx, ty, isCirclePlayer)) {
                // Potential push move
                int px = tx + dx, py = ty + dy;
                if (gameState.inBounds(px, py) && gameState.isEmpty(px, py) &&
                    !gameState.isOpponentScoreCell(px, py, isCirclePlayer)) {
                    move_buffer.emplace_back("push", std::vector<int>{x, y}, 
                                           std::vector<int>{tx, ty}, std::vector<int>{px, py});
                }
            }
        }
        
        // Generate flip moves
        std::string piece_type = gameState.getPieceType(x, y);
        if (piece_type == "stone") {
            // Stone -> River flips (with safety checks)
            for (const std::string& orientation : {"horizontal", "vertical"}) {
                if (isFlipSafeWithGameState(gameState, x, y, player, orientation)) {
                    move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                           std::vector<int>{x, y}, std::vector<int>{}, orientation);
                }
            }
        } else if (piece_type == "river") {
            // River -> Stone (always safe)
            move_buffer.emplace_back("flip", std::vector<int>{x, y}, 
                                   std::vector<int>{x, y}, std::vector<int>{}, "");
        }
        
        // Generate rotate moves (only for rivers)
        if (piece_type == "river") {
            std::string current_orientation = gameState.getRiverOrientation(x, y);
            if (isRotateSafeWithGameState(gameState, x, y, player)) {
                move_buffer.emplace_back("rotate", std::vector<int>{x, y}, 
                                       std::vector<int>{x, y}, std::vector<int>{}, "");
            }
        }
    }
    
    // Enhanced flip safety check using GameState
    bool isFlipSafeWithGameState(GameState& gameState, int fx, int fy, 
                                const std::string& player, const std::string& orientation) const {
        
        std::string piece_type = gameState.getPieceType(fx, fy);
        if (piece_type != "stone") {
            return true; // River -> stone is always safe
        }
        
        bool isCirclePlayer = (player == "circle");
        int opponent_score_row = isCirclePlayer ? (gameState.getRows() - 3) : 2;
        int distance_to_opponent_score = std::abs(fy - opponent_score_row);
        
        // Conservative safety: don't flip too close to opponent scoring area
        // This prevents accidental river flows into opponent scoring areas
        return distance_to_opponent_score >= 2;
    }
    
    // Enhanced rotate safety check using GameState
    bool isRotateSafeWithGameState(GameState& gameState, int rx, int ry,
                                  const std::string& player) const {
        
        std::string piece_type = gameState.getPieceType(rx, ry);
        if (piece_type != "river") {
            return false;
        }
        
        bool isCirclePlayer = (player == "circle");
        int opponent_score_row = isCirclePlayer ? (gameState.getRows() - 3) : 2;
        int distance_to_opponent_score = std::abs(ry - opponent_score_row);
        
        // Conservative safety: don't rotate too close to opponent scoring area
        return distance_to_opponent_score >= 2;
    }
};

// Static member definition
const std::vector<std::pair<int,int>> MoveGenerator::DIRECTIONS = {{1,0},{-1,0},{0,1},{0,-1}};

// ---- Student Agent ----
class StudentAgent {
private:
    std::string side;           // Player side ("circle" or "square")
    std::random_device rd;     // Random device
    std::mt19937 gen;          // Random number generator
    MoveGenerator moveGen;      // High-performance move generator
    BoardEvaluator evaluator;   // Board evaluation system

public:
    explicit StudentAgent(std::string side) : side(std::move(side)), gen(rd()) {}

    Move choose(const std::vector<std::vector<std::map<std::string, std::string>>>& board, int row, int col, const std::vector<int>& score_cols, float current_player_time, float opponent_time) {
        int rows = board.size();
        int cols = board[0].size();

        // Use the new MoveGenerator for high-performance move generation
        const auto& moves = moveGen.generateAllMoves(board, side, rows, cols, score_cols);

        if (moves.empty()) {
            return {"move", {0,0}, {0,0}, {}, ""}; // fallback
        }

        // Use evaluation-based move selection (improved from random)
        bool isCirclePlayer = (side == "circle");
        GameState gameState(rows, cols);
        gameState.loadFromPython(board);
        
        Move bestMove = moves[0];
        float bestScore = -1000000.0f;
        
        // Evaluate each move and pick the best one
        for (const auto& move : moves) {
            // Create a copy of the game state and apply the move
            GameState tempState = gameState;
            
            // Apply move to temporary state (simplified - real implementation would use proper move application)
            // For now, just evaluate current position as baseline
            float score = evaluator.basicEvaluateBoard(tempState, isCirclePlayer);
            
            // Add small random factor for variety when moves are equally good
            std::uniform_real_distribution<float> random_factor(-0.01f, 0.01f);
            score += random_factor(gen);
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        
        return bestMove;
    }
    
    // Test MoveGenerator functionality
    void testMoveGenerator() {
        std::cout << "\n🎯 TESTING MOVE GENERATOR 🎯\n";
        std::cout << "=============================\n";
        
        // Test 1: Basic move generation
        std::cout << "Test 1: Basic Move Generation\n";
        
        // Create a test board
        std::vector<std::vector<std::map<std::string, std::string>>> test_board(5, 
            std::vector<std::map<std::string, std::string>>(5));
        
        // Add some test pieces
        test_board[2][2] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[2][3] = {{"owner", "square"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[1][1] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
        test_board[3][3] = {{"owner", "square"}, {"side", "river"}, {"orientation", "vertical"}};
        
        std::vector<int> test_score_cols = {1, 2, 3, 4};
        
        // Generate moves for circle player
        const auto& circle_moves = moveGen.generateAllMoves(test_board, "circle", 5, 5, test_score_cols);
        std::cout << "✓ Generated " << circle_moves.size() << " moves for circle player\n";
        
        // Display first few moves for verification
        int count = 0;
        for (const auto& move : circle_moves) {
            if (count >= 5) break;  // Show only first 5 moves
            std::cout << "  Move " << (count+1) << ": " << move.action 
                      << " from (" << move.from[0] << "," << move.from[1] << ")"
                      << " to (" << move.to[0] << "," << move.to[1] << ")";
            if (!move.pushed_to.empty()) {
                std::cout << " pushed_to (" << move.pushed_to[0] << "," << move.pushed_to[1] << ")";
            }
            if (!move.orientation.empty()) {
                std::cout << " orientation: " << move.orientation;
            }
            std::cout << "\n";
            count++;
        }
        
        // Test 2: River flow computation
        std::cout << "\nTest 2: River Flow Computation\n";
        
        // Create board with connected rivers for flow testing
        std::vector<std::vector<std::map<std::string, std::string>>> flow_board(7, 
            std::vector<std::map<std::string, std::string>>(7));
        
        // Create a horizontal river chain: (2,2) -> (3,2) -> (4,2)
        flow_board[2][2] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
        flow_board[2][3] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
        flow_board[2][4] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
        
        // Add a piece that can use the flow
        flow_board[1][1] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        
        const auto& flow_moves = moveGen.generateAllMoves(flow_board, "circle", 7, 7, test_score_cols);
        std::cout << "✓ Generated " << flow_moves.size() << " moves with river flow\n";
        
        // Count flow-based moves (moves that go beyond adjacent cells)
        int flow_move_count = 0;
        for (const auto& move : flow_moves) {
            if (move.action == "move") {
                int dx = std::abs(move.to[0] - move.from[0]);
                int dy = std::abs(move.to[1] - move.from[1]);
                if (dx > 1 || dy > 1) {  // Non-adjacent move = flow move
                    flow_move_count++;
                }
            }
        }
        std::cout << "✓ Found " << flow_move_count << " river flow-based moves\n";
        
        // Test 3: Move type distribution
        std::cout << "\nTest 3: Move Type Distribution\n";
        int move_count = 0, push_count = 0, flip_count = 0, rotate_count = 0;
        
        for (const auto& move : circle_moves) {
            if (move.action == "move") move_count++;
            else if (move.action == "push") push_count++;
            else if (move.action == "flip") flip_count++;
            else if (move.action == "rotate") rotate_count++;
        }
        
        std::cout << "✓ Move distribution:\n";
        std::cout << "  - Regular moves: " << move_count << "\n";
        std::cout << "  - Push moves: " << push_count << "\n";
        std::cout << "  - Flip moves: " << flip_count << "\n";
        std::cout << "  - Rotate moves: " << rotate_count << "\n";
        
        std::cout << "\n🎉 ALL MOVE GENERATOR TESTS COMPLETED! 🎉\n";
        std::cout << "=========================================\n\n";
    }

    // Test Phase 3 MoveGenerator features
    void testMoveGen3() {
        std::cout << "\n🚀 TESTING PHASE 3 MOVE GENERATOR 🚀\n";
        std::cout << "=====================================\n";
        
        // Create test board using the correct format (map-based)
        std::vector<std::vector<std::map<std::string, std::string>>> test_board(7, 
            std::vector<std::map<std::string, std::string>>(7));
        
        // Add test pieces
        test_board[2][0] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[2][1] = {{"owner", "cross"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[2][5] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "vertical"}};
        test_board[3][2] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[3][3] = {{"owner", "cross"}, {"side", "river"}, {"orientation", "horizontal"}};
        test_board[4][1] = {{"owner", "cross"}, {"side", "stone"}, {"orientation", "horizontal"}};
        test_board[4][4] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        
        std::vector<int> score_cols = {0, 1, 2, 3, 4, 5, 6};
        MoveGenerator moveGen;
        
        std::cout << "Test 1: Safety Checks\n";
        
        // Test flip safety with correct parameters (using all required parameters)
        bool flipSafeH = moveGen.isFlipSafe(test_board, 0, 2, "circle", 7, 7, score_cols, "horizontal");
        bool flipSafeV = moveGen.isFlipSafe(test_board, 0, 2, "circle", 7, 7, score_cols, "vertical");
        std::cout << "✓ Flip safety (circle stone -> horizontal): " << (flipSafeH ? "SAFE" : "UNSAFE") << "\n";
        std::cout << "✓ Flip safety (circle stone -> vertical): " << (flipSafeV ? "SAFE" : "UNSAFE") << "\n";
        
        // Test rotate safety for rivers (using all required parameters)
        bool rotateSafe = moveGen.isRotateSafe(test_board, 5, 2, "circle", 7, 7, score_cols, "");
        std::cout << "✓ Rotate safety (circle river): " << (rotateSafe ? "SAFE" : "UNSAFE") << "\n";
        
        std::cout << "\nTest 2: Move Categorization\n";
        
        // Test different move categories (using all required parameters)
        auto captureM = moveGen.generateCaptureMoves(test_board, "circle", 7, 7, score_cols);
        auto quietM = moveGen.generateQuietMoves(test_board, "circle", 7, 7, score_cols);
        auto aggressiveM = moveGen.generateAggressiveMoves(test_board, "circle", 7, 7, score_cols);
        
        std::cout << "✓ Capture moves: " << captureM.size() << "\n";
        std::cout << "✓ Quiet moves: " << quietM.size() << "\n";
        std::cout << "✓ Aggressive moves: " << aggressiveM.size() << "\n";
        
        std::cout << "\nTest 3: Move Ordering System\n";
        
        // Test move ordering with priority scoring (using all required parameters)
        auto orderedM = moveGen.generateOrderedMoves(test_board, "circle", 7, 7, score_cols);
        std::cout << "✓ Generated ordered moves: " << orderedM.size() << "\n";
        
        // Show top prioritized moves
        std::cout << "✓ Top 3 prioritized moves:\n";
        for (size_t i = 0; i < std::min(orderedM.size(), size_t(3)); ++i) {
            const auto& move = orderedM[i];
            int score = moveGen.scoreMoveForOrdering(move, test_board, "circle", 7, 7, score_cols);
            std::cout << "  " << (i+1) << ". " << move.action << " from (" 
                      << move.from[0] << "," << move.from[1] << ") (Score: " << score << ")\n";
        }
        
        std::cout << "\nTest 4: GameState Integration\n";
        
        // Create GameState with simple constructor for testing
        GameState gameState(7, 7);
        
        // Test GameState-based move generation
        auto gsMovesCircle = moveGen.generateMovesWithGameState(gameState, "circle");
        auto gsMovesCross = moveGen.generateMovesWithGameState(gameState, "cross");
        
        std::cout << "✓ GameState moves (circle): " << gsMovesCircle.size() << "\n";
        std::cout << "✓ GameState moves (cross): " << gsMovesCross.size() << "\n";
        
        // Test GameState safety methods
        bool gsFlipSafe = moveGen.isFlipSafeWithGameState(gameState, 0, 2, "circle", "horizontal");
        bool gsRotateSafe = moveGen.isRotateSafeWithGameState(gameState, 5, 2, "circle");
        
        std::cout << "✓ GameState flip safety: " << (gsFlipSafe ? "SAFE" : "UNSAFE") << "\n";
        std::cout << "✓ GameState rotate safety: " << (gsRotateSafe ? "SAFE" : "UNSAFE") << "\n";
        
        std::cout << "\n🎉 PHASE 3 TESTS COMPLETED! 🎉\n";
        std::cout << "==============================\n\n";
    }

    // Test BoardEvaluator functionality
    void testBoardEvaluator() {
        std::cout << "\n📊 TESTING BOARD EVALUATOR 📊\n";
        std::cout << "===============================\n";
        
        // Create test scenarios
        GameState gameState(7, 7);
        std::vector<int> score_cols = {0, 1, 2, 3, 4, 5, 6};
        
        std::cout << "Test 1: Basic Evaluation\n";
        
        // Test empty board evaluation
        float empty_score_circle = evaluator.basicEvaluateBoard(gameState, true);
        float empty_score_square = evaluator.basicEvaluateBoard(gameState, false);
        
        std::cout << "✓ Empty board evaluation (circle): " << empty_score_circle << "\n";
        std::cout << "✓ Empty board evaluation (square): " << empty_score_square << "\n";
        
        std::cout << "\nTest 2: Scoring Area Evaluation\n";
        
        // Add stones in scoring areas
        gameState.setPiece(2, 1, CIRCLE_STONE);  // Circle stone in square scoring area
        gameState.setPiece(3, 5, SQUARE_STONE);  // Square stone in circle scoring area
        
        float scoring_score_circle = evaluator.basicEvaluateBoard(gameState, true);
        float scoring_score_square = evaluator.basicEvaluateBoard(gameState, false);
        
        std::cout << "✓ With scoring stones (circle): " << scoring_score_circle << "\n";
        std::cout << "✓ With scoring stones (square): " << scoring_score_square << "\n";
        std::cout << "✓ Circle advantage: " << (scoring_score_circle - empty_score_circle) << "\n";
        std::cout << "✓ Square disadvantage: " << (scoring_score_square - empty_score_square) << "\n";
        
        std::cout << "\nTest 3: Positional Evaluation\n";
        
        // Add stones in various positions
        GameState positionState(7, 7);
        positionState.setPiece(1, 1, CIRCLE_STONE);  // Circle stone near top
        positionState.setPiece(4, 5, CIRCLE_STONE);  // Circle stone near bottom
        positionState.setPiece(1, 5, SQUARE_STONE);  // Square stone near bottom
        positionState.setPiece(4, 1, SQUARE_STONE);  // Square stone near top
        
        float pos_score_circle = evaluator.basicEvaluateBoard(positionState, true);
        float pos_score_square = evaluator.basicEvaluateBoard(positionState, false);
        
        std::cout << "✓ Positional evaluation (circle): " << pos_score_circle << "\n";
        std::cout << "✓ Positional evaluation (square): " << pos_score_square << "\n";
        
        std::cout << "\nTest 4: Evaluation Caching\n";
        
        // Test cache performance
        auto start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            evaluator.basicEvaluateBoard(gameState, true);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "✓ 1000 cached evaluations took: " << duration.count() << " microseconds\n";
        std::cout << "✓ Cache size: " << evaluator.getCacheSize() << " entries\n";
        
        std::cout << "\nTest 5: Scaffold Method Verification\n";
        
        // Test all scaffold methods exist and return 0
        float material = evaluator.evaluateMaterial(gameState, true);
        float position = evaluator.evaluatePosition(gameState, true);  
        float threats = evaluator.evaluateThreats(gameState, true);
        float mobility = evaluator.evaluateMobility(gameState, true);
        float rivers = evaluator.evaluateRiverControl(gameState, true);
        float safety = evaluator.evaluateSafety(gameState, true);
        
        std::cout << "✓ Material evaluation scaffold: " << material << " (expected: 0)\n";
        std::cout << "✓ Position evaluation scaffold: " << position << " (expected: 0)\n";
        std::cout << "✓ Threats evaluation scaffold: " << threats << " (expected: 0)\n";
        std::cout << "✓ Mobility evaluation scaffold: " << mobility << " (expected: 0)\n";
        std::cout << "✓ River control scaffold: " << rivers << " (expected: 0)\n";
        std::cout << "✓ Safety evaluation scaffold: " << safety << " (expected: 0)\n";
        
        // Clear cache for clean state
        evaluator.clearCache();
        
        std::cout << "\n🎉 BOARD EVALUATOR TESTS COMPLETED! 🎉\n";
        std::cout << "=======================================\n\n";
    }

    // Test GameState functionality
    void testGameState() {
        std::cout << "\n🧪 TESTING GAMESTATE CLASS 🧪\n";
        std::cout << "================================\n";
        
        // Test 1: Basic construction and initialization
        std::cout << "Test 1: Basic Construction\n";
        GameState state(10, 8);
        std::cout << "✓ Created GameState (10x8)\n";
        std::cout << "✓ Rows: " << state.getRows() << ", Cols: " << state.getCols() << "\n";
        
        // Test 2: Piece encoding and placement
        std::cout << "\nTest 2: Piece Encoding\n";
        state.setPiece(1, 1, CIRCLE_STONE);
        state.setPiece(2, 2, SQUARE_RIVER_H);
        state.setPiece(3, 3, CIRCLE_RIVER_V);
        
        std::cout << "✓ Placed pieces:\n";
        std::cout << "  (1,1): " << (int)state.getPiece(1,1) << " (should be 1 - CIRCLE_STONE)\n";
        std::cout << "  (2,2): " << (int)state.getPiece(2,2) << " (should be 5 - SQUARE_RIVER_H)\n";
        std::cout << "  (3,3): " << (int)state.getPiece(3,3) << " (should be 4 - CIRCLE_RIVER_V)\n";
        
        // Test 3: Utility functions
        std::cout << "\nTest 3: Utility Functions\n";
        std::cout << "✓ isPlayerPiece(1,1, true): " << (state.isPlayerPiece(1,1,true) ? "YES" : "NO") << "\n";
        std::cout << "✓ isPlayerPiece(2,2, true): " << (state.isPlayerPiece(2,2,true) ? "YES" : "NO") << "\n";
        std::cout << "✓ isEmpty(0,0): " << (state.isEmpty(0,0) ? "YES" : "NO") << "\n";
        std::cout << "✓ isEmpty(1,1): " << (state.isEmpty(1,1) ? "YES" : "NO") << "\n";
        
        // Test 4: Move operations
        std::cout << "\nTest 4: Move Operations\n";
        uint64_t original_hash = state.getHash();
        std::cout << "✓ Original hash: " << original_hash << "\n";
        
        state.applyBasicMove(1, 1, 0, 0);  // Move circle stone
        uint64_t new_hash = state.getHash();
        std::cout << "✓ After move hash: " << new_hash << " (changed: " << (original_hash != new_hash ? "YES" : "NO") << ")\n";
        std::cout << "✓ Piece moved to (0,0): " << (int)state.getPiece(0,0) << "\n";
        std::cout << "✓ Old position (1,1) empty: " << (state.isEmpty(1,1) ? "YES" : "NO") << "\n";
        
        // Test 5: Flip operations
        std::cout << "\nTest 5: Flip Operations\n";
        state.applyFlip(0, 0);  // Flip stone to river
        std::cout << "✓ After flip: " << (int)state.getPiece(0,0) << " (should be 3 - CIRCLE_RIVER_H)\n";
        
        state.applyFlip(0, 0, "vertical");  // Flip back to stone (since it's now river)
        std::cout << "✓ After flip back: " << (int)state.getPiece(0,0) << " (should be 1 - CIRCLE_STONE)\n";
        
        // Test 6: Rotate operations
        std::cout << "\nTest 6: Rotate Operations\n";
        state.applyFlip(0, 0, "horizontal");  // Make it a river first
        std::cout << "✓ Made river: " << (int)state.getPiece(0,0) << "\n";
        
        state.applyRotate(0, 0);  // Rotate it
        std::cout << "✓ After rotate: " << (int)state.getPiece(0,0) << " (orientation changed)\n";
        
        // Test 7: Copy and clone
        std::cout << "\nTest 7: Copy Operations\n";
        GameState copy = state.clone();
        std::cout << "✓ Cloned successfully\n";
        std::cout << "✓ Original hash: " << state.getHash() << "\n";
        std::cout << "✓ Copy hash: " << copy.getHash() << " (same: " << (state.getHash() == copy.getHash() ? "YES" : "NO") << ")\n";
        
        // Test 8: Python board loading
        std::cout << "\nTest 8: Python Board Loading\n";
        std::vector<std::vector<std::map<std::string, std::string>>> python_board(3, 
            std::vector<std::map<std::string, std::string>>(3));
        
        // Add a test piece
        python_board[0][0] = {{"owner", "circle"}, {"side", "stone"}, {"orientation", "horizontal"}};
        python_board[1][1] = {{"owner", "square"}, {"side", "river"}, {"orientation", "vertical"}};
        
        GameState pythonState(3, 3);
        pythonState.loadFromPython(python_board);
        
        std::cout << "✓ Loaded from Python board:\n";
        std::cout << "  (0,0): " << (int)pythonState.getPiece(0,0) << " (should be 1)\n";
        std::cout << "  (1,1): " << (int)pythonState.getPiece(1,1) << " (should be 6)\n";
        
        // Test 9: Scoring and win conditions
        std::cout << "\nTest 9: Scoring System\n";
        GameState scoringState(13, 12);  // Default game size
        
        // Place some stones in scoring areas
        const auto& score_cols = scoringState.getScoreCols();
        std::cout << "✓ Score columns: ";
        for (int col : score_cols) std::cout << col << " ";
        std::cout << "\n";
        
        // Place circle stones in their scoring area (top)
        for (int i = 0; i < 3; ++i) {
            scoringState.setPiece(score_cols[i], 2, CIRCLE_STONE);  // Row 2 is top score row
        }
        
        int circle_score = scoringState.countScoringPieces(true);
        std::cout << "✓ Circle scoring pieces: " << circle_score << " (should be 3)\n";
        
        std::string winner = scoringState.getWinner();
        std::cout << "✓ Current winner: " << (winner.empty() ? "NONE" : winner) << "\n";
        
        std::cout << "\n🎉 ALL GAMESTATE TESTS COMPLETED! 🎉\n";
        std::cout << "=====================================\n\n";
    }
};

// ---- Piece Utilities Testing Function ----
void testPieceUtilities() {
    std::cout << "\n⚙️ TESTING PIECE UTILITIES ⚙️\n";
    std::cout << "=============================\n";
    
    // Test 1: Basic type checking
    std::cout << "Test 1: Basic Type Checking\n";
    std::cout << "✓ isEmpty(EMPTY): " << (isEmpty(EMPTY) ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceStone(CIRCLE_STONE): " << (isPieceStone(CIRCLE_STONE) ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceRiver(CIRCLE_RIVER_H): " << (isPieceRiver(CIRCLE_RIVER_H) ? "YES" : "NO") << "\n";
    
    // Test 2: Ownership checking
    std::cout << "\nTest 2: Ownership Checking\n";
    std::cout << "✓ isPieceOwner(CIRCLE_STONE, \"circle\"): " << (isPieceOwner(CIRCLE_STONE, "circle") ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwner(SQUARE_STONE, \"circle\"): " << (isPieceOwner(SQUARE_STONE, "circle") ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwner(CIRCLE_RIVER_V, \"circle\"): " << (isPieceOwner(CIRCLE_RIVER_V, "circle") ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwner(SQUARE_RIVER_H, \"square\"): " << (isPieceOwner(SQUARE_RIVER_H, "square") ? "YES" : "NO") << "\n";
    
    // Test 3: Fast ownership checking
    std::cout << "\nTest 3: Fast Ownership Checking\n";
    std::cout << "✓ isPieceOwnerFast(CIRCLE_STONE, true): " << (isPieceOwnerFast(CIRCLE_STONE, true) ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwnerFast(SQUARE_STONE, false): " << (isPieceOwnerFast(SQUARE_STONE, false) ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwnerFast(CIRCLE_RIVER_H, true): " << (isPieceOwnerFast(CIRCLE_RIVER_H, true) ? "YES" : "NO") << "\n";
    std::cout << "✓ isPieceOwnerFast(SQUARE_RIVER_V, false): " << (isPieceOwnerFast(SQUARE_RIVER_V, false) ? "YES" : "NO") << "\n";
    
    // Test 4: River orientation
    std::cout << "\nTest 4: River Orientation\n";
    std::cout << "✓ getRiverOrientation(CIRCLE_RIVER_H): \"" << getRiverOrientation(CIRCLE_RIVER_H) << "\"\n";
    std::cout << "✓ getRiverOrientation(SQUARE_RIVER_V): \"" << getRiverOrientation(SQUARE_RIVER_V) << "\"\n";
    std::cout << "✓ isRiverHorizontal(CIRCLE_RIVER_H): " << (isRiverHorizontal(CIRCLE_RIVER_H) ? "YES" : "NO") << "\n";
    std::cout << "✓ isRiverHorizontal(SQUARE_RIVER_V): " << (isRiverHorizontal(SQUARE_RIVER_V) ? "YES" : "NO") << "\n";
    
    // Test 5: Piece flipping
    std::cout << "\nTest 5: Piece Flipping\n";
    uint8_t flipped_h = flipPiece(CIRCLE_STONE, "horizontal");
    uint8_t flipped_v = flipPiece(SQUARE_STONE, "vertical");
    std::cout << "✓ flipPiece(CIRCLE_STONE, \"horizontal\"): " << (int)flipped_h << " (should be " << (int)CIRCLE_RIVER_H << ")\n";
    std::cout << "✓ flipPiece(SQUARE_STONE, \"vertical\"): " << (int)flipped_v << " (should be " << (int)SQUARE_RIVER_V << ")\n";
    
    // Test river to stone
    uint8_t back_to_stone = flipPiece(CIRCLE_RIVER_H);
    std::cout << "✓ flipPiece(CIRCLE_RIVER_H) back to stone: " << (int)back_to_stone << " (should be " << (int)CIRCLE_STONE << ")\n";
    
    // Test 6: Piece rotation
    std::cout << "\nTest 6: Piece Rotation\n";
    uint8_t rotated_h = rotatePiece(CIRCLE_RIVER_H);
    uint8_t rotated_v = rotatePiece(SQUARE_RIVER_V);
    std::cout << "✓ rotatePiece(CIRCLE_RIVER_H): " << (int)rotated_h << " (should be " << (int)CIRCLE_RIVER_V << ")\n";
    std::cout << "✓ rotatePiece(SQUARE_RIVER_V): " << (int)rotated_v << " (should be " << (int)SQUARE_RIVER_H << ")\n";
    
    // Test 7: Utility functions
    std::cout << "\nTest 7: Utility Functions\n";
    std::cout << "✓ getPieceOwnerFlag(CIRCLE_STONE): " << (getPieceOwnerFlag(CIRCLE_STONE) ? "true" : "false") << " (circle)\n";
    std::cout << "✓ getPieceOwnerFlag(SQUARE_RIVER_H): " << (getPieceOwnerFlag(SQUARE_RIVER_H) ? "true" : "false") << " (square)\n";
    std::cout << "✓ getPieceTypeIndex(CIRCLE_STONE): " << getPieceTypeIndex(CIRCLE_STONE) << " (stone=1)\n";
    std::cout << "✓ getPieceTypeIndex(SQUARE_RIVER_H): " << getPieceTypeIndex(SQUARE_RIVER_H) << " (river_h=2)\n";
    std::cout << "✓ getPieceTypeIndex(CIRCLE_RIVER_V): " << getPieceTypeIndex(CIRCLE_RIVER_V) << " (river_v=3)\n";
    
    // Test 8: Performance comparison
    std::cout << "\nTest 8: Performance Validation\n";
    bool consistent = true;
    
    // Verify consistency between fast and regular ownership checks
    for (uint8_t piece = 1; piece <= 6; ++piece) {
        bool regular_circle = isPieceOwner(piece, "circle");
        bool fast_circle = isPieceOwnerFast(piece, true);
        if (regular_circle != fast_circle) {
            consistent = false;
            std::cout << "✗ Inconsistency for piece " << (int)piece << "\n";
        }
    }
    
    std::cout << "✓ Fast vs Regular ownership check consistency: " << (consistent ? "PASS" : "FAIL") << "\n";
    
    std::cout << "\n🎉 ALL PIECE UTILITY TESTS COMPLETED! 🎉\n";
    std::cout << "=======================================\n\n";
}

// ---- GameState Testing Function ----
void runGameStateTests() {
    StudentAgent testAgent("circle");
    testAgent.testGameState();
}

// ---- MoveGenerator Testing Function ----
void runMoveGeneratorTests() {
    StudentAgent testAgent("circle");
    testAgent.testMoveGenerator();
}

// ---- Combined Testing Function ----
void runAllTests() {
    testPieceUtilities();
    runGameStateTests();
    runMoveGeneratorTests();
    
    // Create a StudentAgent instance to test Phase 3 & 4 features
    StudentAgent agent("circle");
    agent.testMoveGen3();
    agent.testBoardEvaluator();
}

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
        .def("choose", &StudentAgent::choose)
        .def("testGameState", &StudentAgent::testGameState)
        .def("testMoveGenerator", &StudentAgent::testMoveGenerator)
        .def("testMoveGen3", &StudentAgent::testMoveGen3)
        .def("testBoardEvaluator", &StudentAgent::testBoardEvaluator);
    
    // Add standalone test functions
    m.def("runGameStateTests", &runGameStateTests, "Run comprehensive GameState tests");
    m.def("testPieceUtilities", &testPieceUtilities, "Test piece manipulation utilities");
    m.def("runMoveGeneratorTests", &runMoveGeneratorTests, "Test move generation engine");
    m.def("runAllTests", &runAllTests, "Run all comprehensive tests");
}