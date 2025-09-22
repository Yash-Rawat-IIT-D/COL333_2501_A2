"""
Student Agent Implementation for River and Stones Game

This file contains the essential utilities and template for implementing your AI agent.
Your task is to complete the StudentAgent class with intelligent move selection.

Game Rules:
- Goal: Get 4 of your stones into the opponent's scoring area
- Pieces can be stones or rivers (horizontal/vertical orientation)  
- Actions: move, push, flip (stone↔river), rotate (river orientation)
- Rivers enable flow-based movement across the board

Your Task:
Implement the choose() method in the StudentAgent class to select optimal moves.
You may add any helper methods and modify the evaluation function as needed.
"""

import random
import copy
from typing import List, Dict, Any, Optional, Tuple
from time import perf_counter_ns
from abc import ABC, abstractmethod
from gameEngine import compute_valid_targets, validate_and_apply_move

# ==================== CONSTANTS FOR STUDENT_AGENT ====================




# Time management thresholds (in seconds)

LOW_TIME_THRESHOLD = 5.0 # Seconds threshold to switch to faster strategies
HIGH_TIME_THRESHOLD = 15.0 # Seconds threshold to allow deeper strategies
CRITICAL_TIME_THRESHOLD = 2.0 # Seconds threshold to avoid timeouts

DEADLINE_BUFFER = 0.05 # Seconds to leave as buffer before actual deadline

LOW_MOVE_TIME = 1.0 # Seconds for low time moves
HIGH_MOVE_TIME = 3.0 # Seconds for high time moves
CRITICAL_MOVE_TIME = 0.01 # Seconds for critical time moves

# Alpha-Beta Pruning and Other Search Parameters

POS_INF = 1e10
NEG_INF = -1e10 
MAX_DEPTH = 3  # Maximum search depth for minimax or other algorithms

# Weights for evaluation of Move Ordering  (to be tuned)

SCORE_IMM = 1_00_000
BLOCK_IMM = 90_000
TT_BEST = 20_000
LANE_IMPACT = 100
M_DELTA_WT = 1

KILLER_BONUS = 500  

# ==================== GAME UTILITIES ====================
# Essential utility functions for game state analysis

def in_bounds(x: int, y: int, rows: int, cols: int) -> bool:
    """Check if coordinates are within board boundaries."""
    return 0 <= x < cols and 0 <= y < rows

def score_cols_for(cols: int) -> List[int]:
    """Get the column indices for scoring areas."""
    w = 4
    start = max(0, (cols - w) // 2)
    return list(range(start, start + w))

def top_score_row() -> int:
    """Get the row index for Circle's scoring area."""
    return 2

def bottom_score_row(rows: int) -> int:
    """Get the row index for Square's scoring area."""
    return rows - 3

def is_opponent_score_cell(x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> bool:
    """Check if a cell is in the opponent's scoring area."""
    if player == "circle":
        return (y == bottom_score_row(rows)) and (x in score_cols)
    else:
        return (y == top_score_row()) and (x in score_cols)

def is_own_score_cell(x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> bool:
    """Check if a cell is in the player's own scoring area."""
    if player == "circle":
        return (y == top_score_row()) and (x in score_cols)
    else:
        return (y == bottom_score_row(rows)) and (x in score_cols)

def get_opponent(player: str) -> str:
    """Get the opponent player identifier."""
    return "square" if player == "circle" else "circle"

# ==================== MOVE GENERATION HELPERS ====================

def get_valid_moves_for_piece_v0(board, x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> List[Dict[str, Any]]:
    """
    Generate all valid moves for a specific piece.
    
    Args:
        board: Current board state
        x, y: Piece position
        player: Current player
        rows, cols: Board dimensions
        score_cols: Scoring column indices
    
    Returns:
        List of valid move dictionaries
    """
    moves = []
    piece = board[y][x]
    
    if piece is None or piece.owner != player:
        return moves
    
    directions = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    
    if piece.side == "stone":
        # Stone movement
        for dx, dy in directions:
            nx, ny = x + dx, y + dy
            if not in_bounds(nx, ny, rows, cols):
                continue
            
            if is_opponent_score_cell(nx, ny, player, rows, cols, score_cols):
                continue
            
            if board[ny][nx] is None:
                # Simple move
                moves.append({"action": "move", "from": [x, y], "to": [nx, ny]})
            elif board[ny][nx].owner != player:
                # Push move
                px, py = nx + dx, ny + dy
                if (in_bounds(px, py, rows, cols) and 
                    board[py][px] is None and 
                    not is_opponent_score_cell(px, py, player, rows, cols, score_cols)):
                    moves.append({"action": "push", "from": [x, y], "to": [nx, ny], "pushed_to": [px, py]})
        
        # Stone to river flips
        for orientation in ["horizontal", "vertical"]:
            moves.append({"action": "flip", "from": [x, y], "orientation": orientation})
    
    else:  # River piece
        # River to stone flip
        moves.append({"action": "flip", "from": [x, y]})
        
        # River rotation
        moves.append({"action": "rotate", "from": [x, y]})
    
    return moves

# TODO - Check if v1 is generating all possible moves including river flows and pushes
def get_valid_moves_for_piece(board, x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> List[Dict[str, Any]]:
    moves = []
    piece = board[y][x]
    if piece is None or piece.owner != player:
        return moves

    # 1) Moves and pushes from authoritative targets (includes river flows and river-push logic)
    info = compute_valid_targets(board, x, y, player, rows, cols, score_cols)
    for (tx, ty) in info.get('moves', set()):
        moves.append({"action": "move", "from": [x, y], "to": [tx, ty]})
    for ((ofx, ofy), (ptx, pty)) in info.get('pushes', []):
        moves.append({"action": "push", "from": [x, y], "to": [ofx, ofy], "pushed_to": [ptx, pty]})

    # 2) Flips and rotates (validation will enforce flow-safety)
    if piece.side == "stone":
        for ori in ("horizontal", "vertical"):
            moves.append({"action": "flip", "from": [x, y], "orientation": ori})
    else:
        moves.append({"action": "flip", "from": [x, y]})           # river -> stone
        moves.append({"action": "rotate", "from": [x, y]})         # toggle orientation

    return moves

def generate_all_moves(board: List[List[Any]], player: str, rows: int, cols: int, score_cols: List[int]) -> List[Dict[str, Any]]:
    """
    Generate all legal moves for the current player.
    
    Args:
        board: Current board state
        player: Current player ("circle" or "square")
        rows, cols: Board dimensions
        score_cols: Scoring column indices
    
    Returns:
        List of all valid move dictionaries
    """
    all_moves = []
    
    for y in range(rows):
        for x in range(cols):
            piece = board[y][x]
            if piece and piece.owner == player:
                piece_moves = get_valid_moves_for_piece_v0(board, x, y, player, rows, cols, score_cols)
                all_moves.extend(piece_moves)
    
    return all_moves

# ==================== BOARD EVALUATION ====================

def count_stones_in_scoring_area(board: List[List[Any]], player: str, rows: int, cols: int, score_cols: List[int]) -> int:
    """Count how many stones a player has in their scoring area."""
    count = 0
    
    if player == "circle":
        score_row = top_score_row()
    else:
        score_row = bottom_score_row(rows)
    
    for x in score_cols:
        if in_bounds(x, score_row, rows, cols):
            piece = board[score_row][x]
            if piece and piece.owner == player and piece.side == "stone":
                count += 1
    
    return count

def basic_evaluate_board(board: List[List[Any]], player: str, rows: int, cols: int, score_cols: List[int]) -> float:
    """
    Basic board evaluation function.
    
    Returns a score where higher values are better for the given player.
    Students can use this as a starting point and improve it.
    """
    score = 0.0
    opponent = get_opponent(player)
    
    # Count stones in scoring areas
    player_scoring_stones = count_stones_in_scoring_area(board, player, rows, cols, score_cols)
    opponent_scoring_stones = count_stones_in_scoring_area(board, opponent, rows, cols, score_cols)
    
    score += player_scoring_stones * 100  
    score -= opponent_scoring_stones * 100  
    
    # Count total pieces and positional factors
    for y in range(rows):
        for x in range(cols):
            piece = board[y][x]
            if piece and piece.owner == player and piece.side == "stone":
                # Basic positional scoring
                if player == "circle":
                    score += (rows - y) * 0.1
                else:
                    score += y * 0.1
    
    return score

def simulate_move(board: List[List[Any]], move: Dict[str, Any], player: str, rows: int, cols: int, score_cols: List[int]) -> Tuple[bool, Any]:
    """
    Simulate a move on a copy of the board.
    
    Args:
        board: Current board state
        move: Move to simulate
        player: Player making the move
        rows, cols: Board dimensions
        score_cols: Scoring column indices
    
    Returns:
        (success: bool, new_board_state or error_message)
    """
    # Import the game engine's move validation function
    try:
        from gameEngine import validate_and_apply_move
        board_copy = copy.deepcopy(board)
        success, message = validate_and_apply_move(board_copy, move, player, rows, cols, score_cols)
        return success, board_copy if success else message
    except ImportError:
        # Fallback to basic simulation if game engine not available
        return True, copy.deepcopy(board)
    
    
# TODO : Add a non-deepcopy, inplace one step simulation and reverse functionality for efficiency

# ==================== BASE AGENT CLASS ====================

class BaseAgent(ABC):
    """
    Abstract base class for all agents.
    """
    
    def __init__(self, player: str):
        """Initialize agent with player identifier."""
        self.player = player
        self.opponent = get_opponent(player)
    
    @abstractmethod
    def choose(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], current_player_time: float, opponent_time: float) -> Optional[Dict[str, Any]]:
        """
        Choose the best move for the current board state.
        
        Args:
            board: 2D list representing the game board
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
        
        Returns:
            Dictionary representing the chosen move, or None if no moves available
        """
        pass

# ==================== STUDENT AGENT HELPERS ====================

def safe_move_timing(current_player_time: float, opponent_time: float) -> Tuple[str, float]:
    """
    Calculate a safe time limit for the current move.
    
    Args:
        current_player_time: Remaining time for the current player
        opponent_time: Remaining time for the opponent
    Returns:
        Safe time limit for the current move
    """
    
    if current_player_time < CRITICAL_TIME_THRESHOLD:
        return "critical", CRITICAL_MOVE_TIME
    elif current_player_time < LOW_TIME_THRESHOLD:
        return "low", LOW_MOVE_TIME
    elif current_player_time < HIGH_TIME_THRESHOLD:
        return "high", HIGH_MOVE_TIME
    else:
        return "normal", HIGH_MOVE_TIME
    
# ==================== STUDENT AGENT IMPLEMENTATION ====================

class StudentAgent(BaseAgent):
    """
    Student Agent Implementation
    
    TODO: Implement your AI agent for the River and Stones game.
    The goal is to get 4 of your stones into the opponent's scoring area.
    
    You have access to these utility functions:
    - generate_all_moves(): Get all legal moves for current player
    - basic_evaluate_board(): Basic position evaluation 
    - simulate_move(): Test moves on board copy
    - count_stones_in_scoring_area(): Count stones in scoring positions
    """
    
    def __init__(self, player: str):
        super().__init__(player)
        self.max_depth_cap = MAX_DEPTH
        
        # Min-Max Parameters
        self.tt = { }
        self.killers = { }
        self.history = { }
        
        # Evaluation Weights (To be tuned)
        self.wt = {
            # WN: weight for n-difference = (own stones already in own scoring area) - (opponent’s stones already in their scoring area).
            # This directly reflects progress toward the 4-stone win and affects both victory penalty and draw margin in the spec.
            "WN": 20.0,

            # WM: weight for m-difference = (own stones that can reach own scoring area in one legal move) - (opponent’s one-move reachables).
            # Compute with legal targets including river flows and pushes; this term shapes imminent threats and is used in the scoring formulas.
            "WM": 10.0,

            # W_LANE: weight for river-lane/control advantage: how many safe flow-based targets or lanes move pieces toward scoring rows for us vs them.
            # Approximate with cheap counts of reachable flow destinations or moves that reduce distance to scoring cells without enabling illegal flows.
            "W_LANE": 2.0,

            # W_TACT: weight for immediate tactical swing: bonuses for “score now”, “must-block now”, and creating new one-move scores this ply.
            # Use engine-valid simulations or target enumeration to detect these volatile, near-term scoring/denial actions.
            "W_TACT": 50.0,

            # W_RISK: weight (penalty) for enabling opponent threats: moves that increase opponent one-move reachability or open unsafe lanes after flips/rotations/pushes.
            # Estimate by checking opponent immediate-score count or lane openness after simulating our move; higher means riskier and should be penalized.
            "W_RISK": 50.0,
        }
             
    def choose(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], current_player_time: float, opponent_time: float) -> Optional[Dict[str, Any]]:
        """
        Choose the best move for the current board state.
        
        Args:
            board: 2D list representing the game board
            rows, cols: Board dimensions  
            score_cols: Column indices for scoring areas
            
        Returns:
            Dictionary representing your chosen move
        """
        
        # 1) Time Budgeting - Establish safe time limits for move
        mode, time_sl = safe_move_timing(current_player_time, opponent_time)
        deadline_ns = perf_counter_ns() + int(max(DEADLINE_BUFFER, time_sl) * 1e9)
                
        # 2) It_Deepening - Iterative deepening to find best move within time
        best_move, best_val = None, NEG_INF
        depth = 1
        
        while depth <= self.max_depth_cap:
            try:
                val, move = self._root_search(board, rows, cols, score_cols, depth)
                if move is not None:
                    best_move, best_val = move, val                   
                depth += 1
            except TimeoutError:
                break
 
        
        # 3) Fallback - If no move found
        if best_move is None:
            moves = self._gen_moves_and_order(board, self.player, rows, cols)
            return moves[0] if moves else None
        
        return best_move

    def _root_search(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], depth: int) -> Tuple[float, Optional[Dict[str, Any]]]:
        
        alpha, beta = NEG_INF, POS_INF
        best_val, best_move = NEG_INF, None
             
        return NEG_INF, None  # Placeholder for actual search implementation

    def _negamax(self, board: List[List[Any]], depth: int, alpha: float, beta: float, player: str, rows: int, cols: int, score_cols: List[int]) -> float:
        return 0.0  # Placeholder for actual negamax implementation

    def _generate_and_order_moves(self, board, side, rows, cols, score_cols, tt_best) -> List[Dict[str, Any]]:
        
        moves = generate_all_moves(board, side, rows, cols, score_cols)
        # Ordered Moves :
        ordered_moves = []
        
        for mv in moves:
            score_move = 0
            if self._is_immediate_score_move(board, mv, side, rows, cols, score_cols):
                score_move += SCORE_IMM
            if self._is_must_block_move(board, mv, side, rows, cols, score_cols):
                score_move += BLOCK_IMM
            
            score_move += self._gen_m_delta(board, side, mv, rows, cols, score_cols)          
            
            if tt_best is not None and self._same_move(mv, tt_best):
                score_move += TT_BEST
            
            score_move += self._killer_heuristic_score(mv)
            score_move += self._history_heuristic_score(mv)
            
            ordered_moves.append((score_move, mv))
            
        ordered_moves.sort(key=lambda x: x[0], reverse=True)
        
        return [mv for _, mv in ordered_moves]
    
    
    # Heuristic Helper and Domain Specific Knowledge Methods
    
    def _is_immediate_score_move(self, board, mv, side, rows, cols, score_cols) -> bool:
        
        action_type = mv["action"]
        
        if action_type not in ("move", "push"):
            return False
        
        pos_x, pos_y = POS_INF, POS_INF
        
        if action_type == "move":
            pos_x, pos_y = mv["to"]
        elif action_type == "push":
            pos_x, pos_y = mv["pushed_to"]
        
        assert type(pos_x) is int and type(pos_y) is int # Get rid of this later -TODO Fix type hint warnings
         
        return is_own_score_cell(pos_x, pos_y, side, rows, cols, score_cols) 
    
    def _is_must_block_move(self, board, mv, side, rows, cols, score_cols) -> bool:
        
    
    # --- Ordering heuristics storage ---
    
    def _same_move(self, a, b) -> bool:
        if b is None: return False
        return self._move_key(a) == self._move_key(b)
    
    def _store_killer(self, depth, mv):
        lst = self.killers.get(depth, [])
        if not any(self._same_move(mv, k) for k in lst):
            lst = [mv] + lst
            self.killers[depth] = lst[:2]

    def _killer_heuristic_score(self, mv) -> int:
        for _, lst in self.killers.items():
            if any(self._same_move(mv, k) for k in lst):
                return KILLER_BONUS
        return 0

    def _history_heuristic_score(self, mv) -> int:
        key = self._move_key(mv)
        return self.history.get(key, 0)

    def _move_key(self, mv) -> tuple:
        a = mv.get("action")
        fr = tuple(mv.get("from", ()))
        if a == "move":
            # Simple move; may include pushed_to in this engine when entering occupied target
            to = tuple(mv.get("to", ()))
            pushed = tuple(mv.get("pushed_to", ()))  # optional
            return ("move", fr, to, pushed)
        if a == "push":
            # Explicit push always has pushed_to
            to = tuple(mv.get("to", ()))
            pushed = tuple(mv.get("pushed_to", ()))
            return ("push", fr, to, pushed)
        if a == "flip":
            # Stone->river requires orientation; river->stone has no orientation
            ori = mv.get("orientation")
            flip_kind = "stone2river" if ori in ("horizontal", "vertical") else "river2stone"
            return ("flip", fr, flip_kind, ori)
        if a == "rotate":
            # Rotate toggles orientation at a single square
            return ("rotate", fr)
        # Fallback keeps compatibility if new action types appear
        return (a, fr, tuple(mv.get("to", ())), tuple(mv.get("pushed_to", ())), mv.get("orientation"))

# ==================== TESTING HELPERS ====================

def test_student_agent():
    """
    Basic test to verify the student agent can be created and make moves.
    """
    print("Testing StudentAgent...")
    
    try:
        from gameEngine import default_start_board, DEFAULT_ROWS, DEFAULT_COLS
        
        rows, cols = DEFAULT_ROWS, DEFAULT_COLS
        score_cols = score_cols_for(cols)
        board = default_start_board(rows, cols)
        
        agent = StudentAgent("circle")
        move = agent.choose(board, rows, cols, score_cols,1.0,1.0)
        
        if move:
            print("✓ Agent successfully generated a move")
        else:
            print("✗ Agent returned no move")
    
    except ImportError:
        agent = StudentAgent("circle")
        print("✓ StudentAgent created successfully")

if __name__ == "__main__":
    # Run basic test when file is executed directly
    test_student_agent()
