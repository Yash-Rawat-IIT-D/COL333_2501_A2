import build.student_agent_module as student_agent
"""
C++ Student Agent Wrapper for River and Stones Game

This module provides a Python wrapper for the C++ implementation,
maintaining compatibility with the instructor's testing framework.
"""

import sys
import os
from typing import List, Dict, Any, Optional

# Add the client_server directory to path to import BaseAgent
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'client_server'))

try:
    from agent import BaseAgent
except ImportError:
    # Fallback BaseAgent definition if agent.py is not available
    from abc import ABC, abstractmethod
    
    class BaseAgent(ABC):
        def __init__(self, player: str):
            self.player = player
            self.opponent = "square" if player == "circle" else "circle"
        
        @abstractmethod
        def choose(self, board: List[List[Any]], rows: int, cols: int, 
                  score_cols: List[int], current_player_time: float, 
                  opponent_time: float) -> Optional[Dict[str, Any]]:
            pass

# Import the compiled C++ module
try:
    import build.student_agent_module as student_agent_cpp
except ImportError:
    try:
        # Try importing from current directory
        import student_agent_module as student_agent_cpp
    except ImportError as e:
        print(f"Error: Could not import C++ module: {e}")
        print("Make sure the C++ module is compiled and accessible.")
        sys.exit(1)
    def choose(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], current_player_time: float, opponent_time: float) -> Optional[Dict[str, Any]]:
        pass

class StudentAgent(BaseAgent):
    """
    Python wrapper for C++ Student Agent implementation.
    
    This class maintains the BaseAgent interface expected by the instructor's
    testing framework while delegating the actual AI logic to C++.
    """
    
    def __init__(self, player: str):
        """Initialize the C++ agent wrapper."""
        super().__init__(player)
        # Create the C++ agent instance
        self.cpp_agent = student_agent_cpp.StudentAgent(player)
    
    def choose(self, board: List[List[Any]], rows: int, cols: int, 
              score_cols: List[int], current_player_time: float, 
              opponent_time: float) -> Optional[Dict[str, Any]]:
        """
        Choose the best move using C++ implementation.
        
        This method maintains the exact interface expected by the game engine
        while translating data between Python and C++ formats.
        
        Args:
            board: 2D list representing the game board
            rows, cols: Board dimensions  
            score_cols: List of column indices for scoring areas
            current_player_time: Remaining time for this player (in seconds)
            opponent_time: Remaining time for the opponent (in seconds)
            
        Returns:
            Dictionary representing the chosen move, or None if no valid moves
        """
        try:
            # Call C++ agent with the same interface
            cpp_move = self.cpp_agent.choose(board, rows, cols, score_cols, 
                                           current_player_time, opponent_time)
            
            if cpp_move is None:
                return None
                
            # Convert C++ Move object to Python dictionary format expected by game engine
            move_dict = {
                "action": cpp_move.action,
                "from": cpp_move.from_pos,  # C++ binding exposes as from_pos
                "to": cpp_move.to_pos,      # C++ binding exposes as to_pos
            }
            
            # Add optional fields based on move type
            if cpp_move.action == "push":
                move_dict["pushed_to"] = cpp_move.pushed_to
            elif cpp_move.action in ["flip", "rotate"]:
                if cpp_move.orientation:  # Only add if not empty
                    move_dict["orientation"] = cpp_move.orientation
                    
            return move_dict
            
        except Exception as e:
            print(f"Error in C++ agent: {e}")
            return None
        # move_dict = {
        #     "action": cpp_move.action,
        #     "from": cpp_move.from_pos,
        #     "to": cpp_move.to_pos,
        # }
        # if cpp_move.action == "push":
        #     move_dict["pushed_to"] = cpp_move.pushed_to
        # if cpp_move.action == "flip":
        #     move_dict["orientation"] = cpp_move.orientation

        # return move_dict
    

def test_student_agent():
    """
    Basic test to verify the student agent can be created and make moves.
    """
    print("Testing C++ StudentAgent...")
    
    try:
        # Create a simple test board
        rows, cols = 10, 8
        score_cols = [2, 3, 4, 5]  # Default scoring columns
        
        # Create empty board
        board = []
        for r in range(rows):
            row = []
            for c in range(cols):
                row.append({})  # Empty cell
            board.append(row)
        
        # Add a few test pieces
        board[1][1] = {"owner": "circle", "side": "stone", "orientation": "horizontal"}
        board[8][6] = {"owner": "square", "side": "river", "orientation": "vertical"}
        
        # Test agent
        agent = StudentAgent("circle")
        move = agent.choose(board, rows, cols, score_cols, 1.0, 1.0)
        
        if move:
            print(f"✓ C++ Agent successfully returned move: {move}")
            return True
        else:
            print("✗ C++ Agent returned None")
            return False
    
    except Exception as e:
        print(f"✗ Error testing C++ agent: {e}")
        return False

if __name__ == "__main__":
    # Run basic test when file is executed directly
    success = test_student_agent()
    if success:
        print("\n🎉 C++ wrapper is working correctly!")
        print("You can now use this agent with the game engine.")
    else:
        print("\n❌ C++ wrapper test failed. Check the error messages above.")
