/***********************************************************
 *
 * @file: rrt.cpp
 * @breif: Contains the Rapidly-Exploring Random Tree(RRT) planner class
 * @author: Yang Haodong
 * @update: 2023-1-19
 * @version: 1.1
 *
 * Copyright (c) 2022， Yang Haodong
 * All rights reserved.
 * --------------------------------------------------------
 *
 **********************************************************/
#include <cmath>
#include <random>

#include "rrt.h"

namespace rrt_planner
{
/**
 * @brief  Constructor
 * @param   nx          pixel number in costmap x direction
 * @param   ny          pixel number in costmap y direction
 * @param   sample_num  andom sample points
 * @param   max_dist    max distance between sample points
 */
RRT::RRT(int nx, int ny, double resolution, int sample_num, double max_dist)
  : GlobalPlanner(nx, ny, resolution), sample_num_(sample_num), max_dist_(max_dist)
{
}

/**
 * @brief RRT implementation
 *
 * @param gloal_costmap global costmap
 * @param start         start node
 * @param goal          goal node
 * @param path          optimal path consists of Node
 * @param expand        containing the node been search during the process
 * @return  true if path found, else false
 */
bool RRT::plan(const unsigned char* gloal_costmap, const Node& start, const Node& goal, std::vector<Node>& path,
               std::vector<Node>& expand)
{
  path.clear();
  expand.clear();

  this->sample_list_.clear();
  // copy
  this->start_ = start, this->goal_ = goal;
  this->costs_ = gloal_costmap;
  this->sample_list_.insert(start);
  expand.push_back(start);

  // main loop
  int iteration = 0;
  while (iteration < this->sample_num_)
  {
    // generate a random node in the map
    Node sample_node = this->_generateRandomNode();

    // obstacle
    if (gloal_costmap[sample_node.id_] >= this->lethal_cost_ * this->factor_)
      continue;

    // visited
    if (this->sample_list_.find(sample_node) != this->sample_list_.end())
      continue;

    // regular the sample node
    Node new_node = this->_findNearestPoint(this->sample_list_, sample_node);
    if (new_node.id_ == -1)
      continue;
    else
    {
      this->sample_list_.insert(new_node);
      expand.push_back(new_node);
    }

    // goal found
    if (_checkGoal(new_node))
    {
      path = this->_convertClosedListToPath(this->sample_list_, start, goal);
      return true;
    }

    iteration++;
  }
  return false;
}

/**
 * @brief Generates a random node
 * @return Generated node
 */
Node RRT::_generateRandomNode()
{
  // obtain a random number from hardware
  std::random_device rd;
  // seed the generator
  std::mt19937 eng(rd());
  // define the range
  std::uniform_real_distribution<float> p(0, 1);
  // heuristic
  if (p(eng) > 0.05)
  {
    // generate node
    std::uniform_int_distribution<int> distr(0, this->ns_ - 1);
    const int id = distr(eng);
    int x, y;
    this->index2Grid(id, x, y);
    return Node(x, y, 0, 0, id, 0);
  }
  else
    return Node(this->goal_.x_, this->goal_.y_, 0, 0, this->goal_.id_, 0);
}

/**
 * @brief Regular the sample node by the nearest node in the sample list
 * @param list  samplee list
 * @param node  sample node
 * @return nearest node
 */
Node RRT::_findNearestPoint(std::unordered_set<Node, NodeIdAsHash, compare_coordinates> list, const Node& node)
{
  Node nearest_node, new_node(node);
  double min_dist = std::numeric_limits<double>::max();

  for (const auto node_ : list)
  {
    // calculate distance
    double new_dist = this->_dist(node_, new_node);

    // update nearest node
    if (new_dist < min_dist)
    {
      nearest_node = node_;
      new_node.pid_ = nearest_node.id_;
      new_node.g_ = new_dist + node_.g_;
      min_dist = new_dist;
    }
  }

  // distance longer than the threshold
  if (min_dist > this->max_dist_)
  {
    // connect sample node and nearest node, then move the nearest node
    // forward to sample node with `max_distance` as result
    double theta = this->_angle(nearest_node, new_node);
    new_node.x_ = nearest_node.x_ + (int)(this->max_dist_ * cos(theta));
    new_node.y_ = nearest_node.y_ + (int)(this->max_dist_ * sin(theta));
    new_node.id_ = this->grid2Index(new_node.x_, new_node.y_);
    new_node.g_ = this->max_dist_ + nearest_node.g_;
  }

  // obstacle check
  if (_isAnyObstacleInPath(new_node, nearest_node))
    new_node.id_ = -1;

  return new_node;
}

/**
 * @brief Check if there is any obstacle between the 2 nodes.
 * @param n1        Node 1
 * @param n2        Node 2
 * @return bool value of whether obstacle exists between nodes
 */
bool RRT::_isAnyObstacleInPath(const Node& n1, const Node& n2)
{
  double theta = this->_angle(n1, n2);
  double dist = this->_dist(n1, n2);

  // distance longer than the threshold
  if (dist > this->max_dist_)
    return true;

  // sample the line between two nodes and check obstacle
  int n_step = (int)(dist / this->resolution_);
  for (int i = 0; i < n_step; i++)
  {
    float line_x = (float)n1.x_ + (float)(i * this->resolution_ * cos(theta));
    float line_y = (float)n1.y_ + (float)(i * this->resolution_ * sin(theta));
    if (this->costs_[this->grid2Index((int)line_x, (int)line_y)] >= this->lethal_cost_ * this->factor_)
      return true;
  }
  return false;
}

/**
 * @brief Check if goal is reachable from current node
 * @param new_node Current node
 * @return bool value of whether goal is reachable from current node
 */
bool RRT::_checkGoal(const Node& new_node)
{
  auto dist = this->_dist(new_node, this->goal_);
  if (dist > this->max_dist_)
    return false;

  if (!_isAnyObstacleInPath(new_node, this->goal_))
  {
    Node goal(this->goal_.x_, this->goal_.y_, dist + new_node.g_, 0, this->grid2Index(this->goal_.x_, this->goal_.y_),
              new_node.id_);
    this->sample_list_.insert(goal);
    return true;
  }
  return false;
}

/**
 * @brief Calculate distance between the 2 nodes.
 * @param n1        Node 1
 * @param n2        Node 2
 * @return distance between nodes
 */
double RRT::_dist(const Node& node1, const Node& node2)
{
  return std::sqrt(std::pow(node1.x_ - node2.x_, 2) + std::pow(node1.y_ - node2.y_, 2));
}

/**
 * @brief Calculate the angle of x-axis between the 2 nodes.
 * @param n1        Node 1
 * @param n2        Node 2
 * @return he angle of x-axis between the 2 node
 */
double RRT::_angle(const Node& node1, const Node& node2)
{
  return atan2(node2.y_ - node1.y_, node2.x_ - node1.x_);
}
}  // namespace rrt_planner
