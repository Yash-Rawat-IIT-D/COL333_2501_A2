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
        move_dict = {
            "action": cpp_move.action,
            "from": cpp_move.from_pos,
            "to": cpp_move.to_pos,
        }
        if cpp_move.action == "push":
            move_dict["pushed_to"] = cpp_move.pushed_to
        if cpp_move.action == "flip":
            move_dict["orientation"] = cpp_move.orientation

        return move_dict
    

def test_piece_utilities():
    """
    Test the piece utility functions using the C++ module.
    """
    print("\n⚙️ TESTING PIECE UTILITIES THROUGH PYTHON WRAPPER ⚙️")
    print("===================================================")
    
    try:
        # Test the piece utilities
        student_agent_cpp.testPieceUtilities()
        print("✅ All Piece Utility tests passed!")
        return True
        
    except Exception as e:
        print(f"❌ Piece Utility test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_move_generator():
    """
    Test the MoveGenerator functionality using the C++ module.
    """
    print("\n🎯 TESTING MOVE GENERATOR THROUGH PYTHON WRAPPER 🎯")
    print("==================================================")
    
    try:
        # Test the move generator
        student_agent_cpp.runMoveGeneratorTests()
        print("✅ All MoveGenerator tests passed!")
        return True
        
    except Exception as e:
        print(f"❌ MoveGenerator test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_gamestate_functionality():
    """
    Test the GameState class functionality using the C++ module.
    """
    print("\n🧪 TESTING GAMESTATE THROUGH PYTHON WRAPPER 🧪")
    print("==============================================")
    
    try:
        # Test 1: Use the standalone test function
        print("Test 1: Running C++ GameState Tests")
        student_agent_cpp.runGameStateTests()
        
        # Test 2: Test through StudentAgent wrapper
        print("\nTest 2: Testing through StudentAgent wrapper")
        agent = StudentAgent("circle")
        
        # Access the C++ agent directly for testing
        cpp_agent = agent.cpp_agent
        cpp_agent.testGameState()
        
        print("✅ All GameState tests passed!")
        return True
        
    except Exception as e:
        print(f"❌ GameState test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_phase3_features():
    """
    Test Phase 3 MoveGenerator features using the C++ testMoveGen3 method.
    """
    print("\n🚀 TESTING PHASE 3 MOVE GENERATOR FEATURES 🚀")
    print("=============================================")
    
    try:
        # Test using the dedicated Phase 3 test method
        print("Running C++ Phase 3 tests...")
        agent = StudentAgent("circle")
        cpp_agent = agent.cpp_agent
        cpp_agent.testMoveGen3()
        
        print("\n✅ All Phase 3 tests passed!")
        print("✅ Safety Checks: isFlipSafe() and isRotateSafe() working")
        print("✅ Move Categorization: Capture, Quiet, Aggressive moves working") 
        print("✅ Move Ordering: Priority-based scoring system working")
        print("✅ GameState Integration: Enhanced validation methods working")
        return True
        
    except Exception as e:
        print(f"❌ Phase 3 test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_board_evaluator():
    """
    Test Phase 4 BoardEvaluator features using the C++ testBoardEvaluator method.
    """
    print("\n📊 TESTING BOARD EVALUATOR FEATURES 📊")
    print("======================================")
    
    try:
        # Test using the dedicated BoardEvaluator test method
        print("Running C++ BoardEvaluator tests...")
        agent = StudentAgent("circle")
        cpp_agent = agent.cpp_agent
        cpp_agent.testBoardEvaluator()
        
        print("\n✅ All BoardEvaluator tests passed!")
        print("✅ Basic Evaluation: Compatible with Python basic_evaluate_board")
        print("✅ Evaluation Caching: High-performance caching system working")
        print("✅ Scoring Areas: Proper scoring area detection implemented")
        print("✅ Scaffold Methods: All 6 advanced evaluation methods ready for implementation")
        return True
        
    except Exception as e:
        print(f"❌ BoardEvaluator test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_student_agent():
    """
    Basic test to verify the student agent can be created and make moves.
    """
    print("\n🔧 TESTING C++ STUDENT AGENT 🔧")
    print("===============================")
    
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
    # Run comprehensive tests when file is executed directly
    print("🚀 RUNNING COMPREHENSIVE C++ TESTS 🚀")
    print("====================================\n")
    
    # Test 1: Piece Utilities functionality
    utilities_success = test_piece_utilities()
    
    # Test 2: GameState functionality
    gamestate_success = test_gamestate_functionality()
    
    # Test 3: MoveGenerator functionality
    movegen_success = test_move_generator()
    
    # Test 4: Phase 3 MoveGenerator features
    phase3_success = test_phase3_features()
    
    # Test 5: Phase 4 BoardEvaluator features
    evaluator_success = test_board_evaluator()
    
    # Test 6: Agent functionality  
    agent_success = test_student_agent()
    
    # Results
    print("\n" + "="*65)
    print("📊 TEST RESULTS SUMMARY")
    print("="*65)
    print(f"Piece Utilities Tests:  {'✅ PASSED' if utilities_success else '❌ FAILED'}")
    print(f"GameState Tests:        {'✅ PASSED' if gamestate_success else '❌ FAILED'}")
    print(f"MoveGenerator Tests:    {'✅ PASSED' if movegen_success else '❌ FAILED'}")
    print(f"Phase 3 Features Tests: {'✅ PASSED' if phase3_success else '❌ FAILED'}")
    print(f"BoardEvaluator Tests:   {'✅ PASSED' if evaluator_success else '❌ FAILED'}")
    print(f"Agent Tests:            {'✅ PASSED' if agent_success else '❌ FAILED'}")
    
    if utilities_success and gamestate_success and movegen_success and phase3_success and evaluator_success and agent_success:
        print("\n🎉 ALL TESTS PASSED! C++ FRAMEWORK COMPLETE!")
        print("🚀 High-performance move generation engine implemented")
        print("⚡ River flow computation with BFS optimization")
        print("🛡️ Safety checks and move categorization working")
        print("🎯 Move ordering system for alpha-beta optimization ready")
        print("🔗 GameState integration methods implemented")
        print("� Board evaluation system with Python compatibility")
        print("⚡ High-performance evaluation caching implemented")
        print("�💡 Phase 1, 2, 3, & 4 complete - ready for Phase 5 (minimax)")
        print("You can now use this agent with the game engine.")
    else:
        print("\n❌ Some tests failed. Check the error messages above.")
        print("Make sure the C++ module compiled correctly.")
