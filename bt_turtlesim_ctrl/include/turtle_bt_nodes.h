#pragma once

// ============================================================================
// TURTLESIM BT NODES
// ============================================================================
// BehaviorTree.CPP v4 nodes that control turtle1 to hunt prey turtles.
//
// Condition nodes:
//   HasTargetTurtles   — checks if the prey list is non-empty
//   IsTurtleCaught     — checks if turtle1 is close enough to the current target
//
// Action nodes:
//   SelectNextTurtle   — chooses a target from the list using a strategy
//                        "sequential" : first-in-first-out (FIFO)
//                        "closest"    : nearest Euclidean distance
//   MoveTurtleToTarget — drives turtle1 toward a target position (proportional control)
//   CatchTurtle        — calls /kill service, removes turtle from the prey list
//   WaitForTurtle      — idles for timeout_sec, then returns FAILURE (used as a
//                        polite busy-wait when no prey is available)
//
// Blackboard keys (internal, not exposed as ports):
//   "ros_node"    rclcpp::Node*               set by controller node
//   "turtle_list" std::vector<TurtleTarget>   maintained by controller subscriptions
//   "main_x/y/theta" double                  updated by /turtle1/pose subscription
//   "strategy"    std::string                 set from launch parameter
// ============================================================================

#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/action_node.h>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <turtlesim/srv/kill.hpp>

#include <string>
#include <vector>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// Shared data type stored on the blackboard
// ---------------------------------------------------------------------------
struct TurtleTarget {
    std::string name;
    double x{0.0};
    double y{0.0};
};

// ===========================================================================
// CONDITION NODES
// ===========================================================================

// Returns SUCCESS when the prey list has at least one entry.
class HasTargetTurtles : public BT::ConditionNode {
public:
    HasTargetTurtles(const std::string& name, const BT::NodeConfig& config)
        : BT::ConditionNode(name, config) {}

    BT::NodeStatus tick() override;

    static BT::PortsList providedPorts() { return {}; }
};

// Returns SUCCESS when turtle1 is within catch_distance of the current target.
class IsTurtleCaught : public BT::ConditionNode {
public:
    IsTurtleCaught(const std::string& name, const BT::NodeConfig& config)
        : BT::ConditionNode(name, config) {}

    BT::NodeStatus tick() override;

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<double>("catch_distance", 0.5,
                "Distance (meters) at which a turtle is considered caught"),
            BT::InputPort<double>("target_x", "X coordinate of the current target"),
            BT::InputPort<double>("target_y", "Y coordinate of the current target"),
        };
    }
};

// ===========================================================================
// ACTION NODES
// ===========================================================================

// Selects the next target turtle from the prey list.
//   strategy="sequential" — picks the oldest entry (FIFO)
//   strategy="closest"    — picks the nearest turtle to turtle1
// Writes target_name, target_x, target_y to the blackboard via output ports.
class SelectNextTurtle : public BT::SyncActionNode {
public:
    SelectNextTurtle(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    BT::NodeStatus tick() override;

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<std::string>("strategy", "sequential",
                "Catching strategy: 'sequential' (FIFO) or 'closest' (nearest first)"),
            BT::OutputPort<std::string>("target_name", "Name of the selected prey turtle"),
            BT::OutputPort<double>("target_x",         "X position of the selected prey"),
            BT::OutputPort<double>("target_y",         "Y position of the selected prey"),
        };
    }
};

// Drives turtle1 toward (target_x, target_y) using proportional control.
// Returns RUNNING while moving, SUCCESS when within catch_distance.
// Publishes geometry_msgs/Twist to /turtle1/cmd_vel every tick.
class MoveTurtleToTarget : public BT::StatefulActionNode {
public:
    MoveTurtleToTarget(const std::string& name, const BT::NodeConfig& config);

    BT::NodeStatus onStart()   override;
    BT::NodeStatus onRunning() override;
    void           onHalted()  override;

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<double>("target_x",       "Target X position"),
            BT::InputPort<double>("target_y",       "Target Y position"),
            BT::InputPort<std::string>("target_name", "Name of target (used for logging)"),
            BT::InputPort<double>("catch_distance", 0.5,
                "Stop and return SUCCESS when within this radius"),
            BT::InputPort<double>("linear_gain",    1.5,
                "Proportional gain for forward velocity"),
            BT::InputPort<double>("angular_gain",   4.0,
                "Proportional gain for angular velocity"),
            BT::InputPort<double>("max_linear_speed",  2.0,
                "Upper clamp on forward speed (m/s)"),
            BT::InputPort<double>("max_angular_speed", 3.0,
                "Upper clamp on turn rate (rad/s)"),
            BT::InputPort<double>("heading_tolerance", 0.4,
                "Heading error (rad) above which the turtle turns in place "
                "instead of driving forward"),
        };
    }

private:
    rclcpp::Node* node_{nullptr};
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;

    // Compute and publish a velocity command. Returns true when target is reached.
    bool stepTowardTarget();
};

// Calls the turtlesim /kill service and removes the turtle from the blackboard
// prey list.  Implemented as a StatefulActionNode: the request is fired once in
// onStart() and polled non-blockingly in onRunning(), so the tree keeps ticking
// (and Groot2 stays live) while the service call is in flight.  Returns SUCCESS
// once the kill is acknowledged or after a short timeout; either way the prey is
// removed from the list so the hunter never gets stuck on it.
class CatchTurtle : public BT::StatefulActionNode {
public:
    CatchTurtle(const std::string& name, const BT::NodeConfig& config);

    BT::NodeStatus onStart()   override;
    BT::NodeStatus onRunning() override;
    void           onHalted()  override {}

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<std::string>("target_name", "Name of the turtle to eliminate"),
        };
    }

private:
    void removeFromPreyList(const std::string& name);

    rclcpp::Node* node_{nullptr};
    rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;
    rclcpp::Time   deadline_;
    std::string    target_name_;
    bool           kill_acknowledged_{false};
};

// Idles for timeout_sec seconds, then returns FAILURE.
// Used inside a Fallback to implement a polite wait when the prey list is empty.
class WaitForTurtle : public BT::StatefulActionNode {
public:
    WaitForTurtle(const std::string& name, const BT::NodeConfig& config);

    BT::NodeStatus onStart()   override;
    BT::NodeStatus onRunning() override;
    void           onHalted()  override {}

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<double>("timeout_sec", 1.0,
                "Time to wait (seconds) before returning FAILURE"),
        };
    }

private:
    rclcpp::Node*  node_{nullptr};
    rclcpp::Time   start_time_;
};
