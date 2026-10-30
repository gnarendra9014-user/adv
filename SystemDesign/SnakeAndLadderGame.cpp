/*
 * Snake and Ladder Game - interview-style low-level design
 *
 * Question:
 * Design a Snake and Ladder game for N players on a 100-cell board.
 * The board has snakes (move the player down) and ladders (move the
 * player up). Players take turns rolling a six-sided die and advance
 * accordingly. The first player to reach or exceed cell 100 wins.
 *
 * Key design points:
 *   - Board is configured with snakes and ladders (start -> end mapping).
 *   - Die is abstracted so it can be swapped (fair, loaded, etc.).
 *   - Game engine drives the turn loop; Board and Player are pure data.
 *   - A player landing on a snake's head slides to its tail;
 *     landing on a ladder's bottom climbs to its top.
 *   - If a roll would take the player past cell 100 the turn is skipped.
 */

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// ─────────────────────────────────────────────────────────────
// Die
// ─────────────────────────────────────────────────────────────
class Die {
public:
    explicit Die(int faces = 6) : faces_(faces) {
        if (faces < 1) {
            throw invalid_argument("A die must have at least one face.");
        }
    }

    int roll() const {
        return rand() % faces_ + 1;
    }

private:
    int faces_;
};

// ─────────────────────────────────────────────────────────────
// Player
// ─────────────────────────────────────────────────────────────
class Player {
public:
    explicit Player(string name) : name_(move(name)) {}

    const string& name() const { return name_; }
    int position() const { return position_; }
    void setPosition(int pos) { position_ = pos; }

private:
    string name_;
    int position_ = 0;   // 0 means not yet on the board
};

// ─────────────────────────────────────────────────────────────
// Board  –  holds snakes and ladders as start->end mappings
// ─────────────────────────────────────────────────────────────
class Board {
public:
    static constexpr int kBoardSize = 100;

    void addSnake(int head, int tail) {
        validate(head, tail);
        if (tail >= head) {
            throw invalid_argument("Snake tail must be below its head.");
        }
        jumps_[head] = tail;
    }

    void addLadder(int bottom, int top) {
        validate(bottom, top);
        if (top <= bottom) {
            throw invalid_argument("Ladder top must be above its bottom.");
        }
        jumps_[bottom] = top;
    }

    // Prints all snakes and ladders on the board.
    void printBoard() const {
        vector<pair<int, int>> snakes, ladders;
        for (const auto& [start, end] : jumps_) {
            if (end < start) {
                snakes.push_back({start, end});
            } else {
                ladders.push_back({start, end});
            }
        }

        sort(snakes.begin(), snakes.end());
        sort(ladders.begin(), ladders.end());

        cout << "--- Board Layout ---\n";
        cout << "Snakes (" << snakes.size() << "):\n";
        for (const auto& [head, tail] : snakes) {
            cout << "  " << head << " ↓ " << tail << "\n";
        }
        cout << "Ladders (" << ladders.size() << "):\n";
        for (const auto& [bottom, top] : ladders) {
            cout << "  " << bottom << " ↑ " << top << "\n";
        }
        cout << "--------------------\n";
    }

    // Returns the final position after resolving any snake/ladder at `cell`.
    int resolve(int cell) const {
        const auto it = jumps_.find(cell);
        if (it != jumps_.end()) {
            const int dest = it->second;
            cout << (dest < cell ? "  ↓ Snake" : "  ↑ Ladder")
                 << " at " << cell << " → " << dest << "\n";
            return dest;
        }
        return cell;
    }

private:
    void validate(int a, int b) const {
        if (a < 1 || a > kBoardSize || b < 1 || b > kBoardSize) {
            throw out_of_range("Cell positions must be within 1-" +
                               to_string(kBoardSize) + ".");
        }
        if (jumps_.count(a)) {
            throw invalid_argument("Cell " + to_string(a) +
                                   " already has a snake or ladder.");
        }
    }

    unordered_map<int, int> jumps_;   // start -> end
};

// ─────────────────────────────────────────────────────────────
// Game  –  orchestrates the turn-based loop
// ─────────────────────────────────────────────────────────────
class SnakeAndLadderGame {
public:
    SnakeAndLadderGame(Board board, Die die) : board_(move(board)), die_(die) {}

    void addPlayer(const string& name) {
        if (gameOver_) {
            throw logic_error("Cannot add players after the game has ended.");
        }
        players_.emplace_back(name);
    }

    void play() {
        if (players_.size() < 2) {
            throw logic_error("At least two players are needed.");
        }

        cout << "\n=== Snake and Ladder Game Start ===\n\n";

        while (!gameOver_) {
            for (auto& player : players_) {
                takeTurn(player);
                if (gameOver_) break;
            }
        }
    }

private:
    void takeTurn(Player& player) {
        const int rolled = die_.roll();
        const int newPos = player.position() + rolled;

        cout << player.name() << " rolled a " << rolled
             << " (at " << player.position() << ")";

        if (newPos > Board::kBoardSize) {
            cout << " — cannot move, stays at " << player.position() << "\n";
            return;
        }

        const int finalPos = board_.resolve(newPos);
        player.setPosition(finalPos);

        cout << " → moves to " << finalPos << "\n";

        if (finalPos == Board::kBoardSize) {
            cout << "\n🏆 " << player.name() << " wins!\n";
            gameOver_ = true;
        }
    }

    Board board_;
    Die die_;
    vector<Player> players_;
    bool gameOver_ = false;
};

// ─────────────────────────────────────────────────────────────
// main – demo
// ─────────────────────────────────────────────────────────────
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    try {
        Board board;

        // Snakes (head → tail)
        board.addSnake(99, 10);
        board.addSnake(65, 25);
        board.addSnake(45, 6);
        board.addSnake(52, 11);

        // Ladders (bottom → top)
        board.addLadder(2, 38);
        board.addLadder(7, 14);
        board.addLadder(28, 84);
        board.addLadder(51, 67);
        board.addLadder(71, 91);

        Die die;   // standard 6-faced

        SnakeAndLadderGame game(board, die);
        game.addPlayer("Alice");
        game.addPlayer("Bob");
        game.play();

    } catch (const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}

