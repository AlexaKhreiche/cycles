#include "api.h"
#include "utils.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <queue>
#include <random>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace cycles;

class BotClient {
  Connection connection;
  std::string name;
  GameState state;
  Player my_player;
  std::mt19937 rng;

  // Check if moving in the specified direction is valid
  bool is_valid_move(Direction direction) {
    auto new_pos = my_player.position + getDirectionVector(direction);
    if (!state.isInsideGrid(new_pos)) {
      return false;
    }
    if (state.getGridCell(new_pos) != 0) {
      return false;
    }
    return true;
  }

  // Calculate Euclidean distance between two positions
  double calculate_distance(const sf::Vector2i& pos1, const sf::Vector2i& pos2) const {
    int dx = pos1.x - pos2.x;
    int dy = pos1.y - pos2.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  // Find the minimum distance from the current position to any opponent
  double min_distance_to_opponents(const sf::Vector2i& position) const {
    double min_dist = std::numeric_limits<double>::max();
    for (const auto& player : state.players) {
      if (player.name == name) continue; // Skip self
      double dist = calculate_distance(position, player.position);
      if (dist < min_dist) {
        min_dist = dist;
      }
    }
    return min_dist;
  }

  Direction decideMove() {
    std::vector<Direction> valid_moves;

    // Collect all valid moves
    for (int dir_value = 0; dir_value < 4; ++dir_value) {
      Direction dir = getDirectionFromValue(dir_value);
      if (is_valid_move(dir)) {
        valid_moves.push_back(dir);
      }
    }

    if (valid_moves.empty()) {
      spdlog::error("{}: No valid moves available", name);
      exit(1);
    }

    // Vector to store pairs of (Direction, Minimum Distance to Opponents)
    std::vector<std::pair<Direction, double>> move_distances;

    for (auto dir : valid_moves) {
      auto new_pos = my_player.position + getDirectionVector(dir);
      double min_dist = min_distance_to_opponents(new_pos);
      move_distances.emplace_back(dir, min_dist);
      spdlog::debug("{}: Direction {} has min distance {:.2f}", name, getDirectionValue(dir), min_dist);
    }

    // Find the maximum of the minimum distances
    double max_min_dist = -1.0;
    for (const auto& move : move_distances) {
      if (move.second > max_min_dist) {
        max_min_dist = move.second;
      }
    }

    // Collect all directions that have the maximum minimum distance
    std::vector<Direction> best_moves;
    for (const auto& move : move_distances) {
      if (std::abs(move.second - max_min_dist) < 1e-6) { // Comparing floating points
        best_moves.push_back(move.first);
      }
    }

    // Randomly select among the best moves
    std::uniform_int_distribution<int> dist(0, static_cast<int>(best_moves.size()) - 1);
    Direction best_direction = best_moves[dist(rng)];
    spdlog::debug("{}: Selected direction {} with min distance {:.2f}", name,
                  getDirectionValue(best_direction), max_min_dist);
    return best_direction;
  }

  void receiveGameState() {
    state = connection.receiveGameState();
    for (const auto& player : state.players) {
      if (player.name == name) {
        my_player = player;
        break;
      }
    }
  }

  void sendMove() {
    spdlog::debug("{}: Sending move", name);
    auto move = decideMove();
    connection.sendMove(move);
  }

 public:
  BotClient(const std::string& botName) : name(botName) {
    std::random_device rd;
    rng.seed(rd());
    connection.connect(name);
    if (!connection.isActive()) {
      spdlog::critical("{}: Connection failed", name);
      exit(1);
    }
  }

  void run() {
    while (connection.isActive()) {
      receiveGameState();
      sendMove();
    }
  }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
    return 1;
  }
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::debug);
#endif
  std::string botName = argv[1];
  BotClient bot(botName);
  bot.run();
  return 0;
}