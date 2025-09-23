Perfect! Let's map out the detailed deliverables for a high-performance C++ framework without generating code yet. Here's the strategic breakdown for the next 4 hours:

## **Core Framework Deliverables (4-Hour Strategy)**

### **1. GameState Class (30-45 minutes)**
**Purpose**: Efficient board representation and game state management

**Key Components**:
- `std::vector<std::vector<int>>` board representation (using integers for piece encoding)
- Piece encoding: `empty=0`, `circle_stone=1`, `square_stone=2`, `circle_river_h=3`, `circle_river_v=4`, `square_river_h=5`, `square_river_v=6`
- Fast copy constructor for minimax simulations
- Hash function for transposition table support
- Quick win condition checking
- Efficient piece counting methods

**Methods**:
- `applyMove()` - Fast move application with rollback capability
- `isValidMove()` - Quick move validation
- `getWinner()` - Win condition check
- `getPieceCount()` - Count pieces in scoring areas
- `clone()` - Deep copy for search tree
- `hash()` - Board position hashing

### **2. Piece Utilities (15-20 minutes)**
**Purpose**: Lightweight piece manipulation without objects

**Key Functions**:
- `isPieceOwner(int piece, std::string player)` - Fast ownership check
- `isPieceStone(int piece)` / `isPieceRiver(int piece)` - Type checking
- `getRiverOrientation(int piece)` - Orientation extraction
- `flipPiece(int piece)` - Stone ↔ River conversion
- `rotatePiece(int piece)` - River rotation
- Bitwise operations for maximum speed

### **3. Move Generation Engine (45-60 minutes)**
**Purpose**: High-performance legal move generation

**Key Components**:
- `std::vector<Move>` pre-allocated move lists
- Direction arrays: `std::vector<std::pair<int,int>> DIRECTIONS = {{0,1},{0,-1},{1,0},{-1,0}}`
- River flow computation using BFS with visited arrays
- Move categorization: normal moves, pushes, flips, rotates
- Move ordering for alpha-beta optimization

**Methods**:
- `generateAllMoves(GameState&, std::string player)` - Complete move generation
- `generateCaptureMoves()` - Priority moves toward scoring
- `isMoveLegal()` - Fast legality checking
- `computeRiverFlow()` - Efficient flow destination calculation

### **4. Board Evaluation System (45-60 minutes)**
**Purpose**: Strategic position assessment

**Key Metrics**:
- **Material Count**: Stones in play vs opponent
- **Position Value**: Distance to opponent's scoring area
- **Scoring Threat**: Pieces that can reach scoring area in 1-2 moves  
- **River Control**: Strategic river placements
- **Mobility**: Available legal moves
- **Safety**: Pieces under attack/defendable

**Evaluation Categories**:
- `evaluateMaterial()` - Basic piece counting
- `evaluatePosition()` - Positional bonuses/penalties
- `evaluateThreats()` - Immediate scoring opportunities
- `evaluateMobility()` - Move flexibility assessment

### **5. Minimax/Negamax Engine (90-120 minutes)**
**Purpose**: Core AI decision-making algorithm

**Features**:
- **Alpha-Beta Pruning**: Standard optimization
- **Iterative Deepening**: Time management + better move ordering
- **Transposition Tables**: Position caching with `std::unordered_map<uint64_t, TTEntry>`
- **Move Ordering**: Score promising moves first
- **Time Management**: Respect time constraints with periodic checking

**Key Components**:
- `negamax(GameState&, int depth, int alpha, int beta, bool isMaxPlayer)`
- `TranspositionTable` with zobrist hashing
- `MoveOrderer` for pruning efficiency
- `TimeManager` for clock management

### **6. Utility Integration Layer (30-45 minutes)**
**Purpose**: Bridge C++ implementation with Python game engine

**Key Functions**:
- **Data Translation**: Python board ↔ C++ GameState
- **Move Format Conversion**: C++ Move ↔ Python dict
- **Error Handling**: Graceful failure modes
- **Performance Monitoring**: Track search statistics

## **Priority Order for Implementation:**

### **Hour 1**: Foundation
1. GameState class with integer encoding
2. Basic piece utilities  
3. Simple move validation

### **Hour 2**: Move Generation
1. Complete legal move generation
2. River flow computation
3. Move ordering basics

### **Hour 3**: Search Algorithm
1. Basic minimax with alpha-beta
2. Simple evaluation function
3. Integration with move generation

### **Hour 4**: Optimization & Polish
1. Transposition tables
2. Iterative deepening
3. Advanced evaluation features
4. Time management

## **Performance Targets:**
- **Move Generation**: <1ms for typical position
- **Position Evaluation**: <0.1ms
- **Search Depth**: 6-8 plies in time limit
- **Memory Usage**: Minimal allocations during search

This framework prioritizes **speed through vectors**, **minimal object creation**, and **cache-friendly data structures** while maintaining the abstractions needed for the Python testing interface.

Ready to start implementing when you give the go-ahead! 🚀