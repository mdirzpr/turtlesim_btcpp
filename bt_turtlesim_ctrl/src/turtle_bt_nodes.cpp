// ============================================================================
// TURTLESIM BT NODES — Implementation
// ============================================================================
#include "turtle_bt_nodes.h"

#include <algorithm>
#include <chrono>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void getMainPose(const BT::Blackboard::Ptr& bb,
                        double& x, double& y, double& theta)
{
    if (!bb->get("main_x",     x))     x     = 0.0;
    if (!bb->get("main_y",     y))     y     = 0.0;
    if (!bb->get("main_theta", theta)) theta  = 0.0;
}

static double wrapAngle(double a)
{
    while (a >  M_PI) a -= 2.0 * M_PI;
    while (a < -M_PI) a += 2.0 * M_PI;
    return a;
}

// ===========================================================================
// HasTargetTurtles
// ===========================================================================

BT::NodeStatus HasTargetTurtles::tick()
{
    std::vector<TurtleTarget> list;
    (void)config().blackboard->get("turtle_list", list);
    return list.empty() ? BT::NodeStatus::FAILURE : BT::NodeStatus::SUCCESS;
}

// ===========================================================================
// IsTurtleCaught
// ===========================================================================

BT::NodeStatus IsTurtleCaught::tick()
{
    double tx{0.0}, ty{0.0}, catch_dist{0.5};
    getInput("target_x",       tx);
    getInput("target_y",       ty);
    getInput("catch_distance", catch_dist);

    double mx{0.0}, my{0.0}, mtheta{0.0};
    getMainPose(config().blackboard, mx, my, mtheta);

    return (std::hypot(tx - mx, ty - my) < catch_dist)
        ? BT::NodeStatus::SUCCESS
        : BT::NodeStatus::FAILURE;
}

// ===========================================================================
// SelectNextTurtle
// ===========================================================================

BT::NodeStatus SelectNextTurtle::tick()
{
    std::vector<TurtleTarget> list;
    (void)config().blackboard->get("turtle_list", list);

    if (list.empty()) {
        return BT::NodeStatus::FAILURE;
    }

    std::string strategy{"sequential"};
    getInput("strategy", strategy);

    auto* node = config().blackboard->get<rclcpp::Node*>("ros_node");
    TurtleTarget selected;

    if (strategy == "closest") {
        double mx{0.0}, my{0.0}, mtheta{0.0};
        getMainPose(config().blackboard, mx, my, mtheta);

        double min_dist = std::numeric_limits<double>::max();
        for (const auto& t : list) {
            double d = std::hypot(t.x - mx, t.y - my);
            if (d < min_dist) {
                min_dist = d;
                selected = t;
            }
        }
        RCLCPP_INFO(node->get_logger(),
            "🎯  [SelectNextTurtle] closest  → '%-10s'  dist=%.2f m  |  queue: %zu",
            selected.name.c_str(), min_dist, list.size());
    } else {
        selected = list.front();
        RCLCPP_INFO(node->get_logger(),
            "🎯  [SelectNextTurtle] sequential → '%-10s'  |  queue: %zu",
            selected.name.c_str(), list.size());
    }

    setOutput("target_name", selected.name);
    setOutput("target_x",    selected.x);
    setOutput("target_y",    selected.y);
    return BT::NodeStatus::SUCCESS;
}

// ===========================================================================
// MoveTurtleToTarget
// ===========================================================================

MoveTurtleToTarget::MoveTurtleToTarget(const std::string& name,
                                       const BT::NodeConfig& config)
    : BT::StatefulActionNode(name, config)
{
    node_ = config.blackboard->get<rclcpp::Node*>("ros_node");
    cmd_vel_pub_ = node_->create_publisher<geometry_msgs::msg::Twist>(
        "/turtle1/cmd_vel", 10);
}

BT::NodeStatus MoveTurtleToTarget::onStart()
{
    std::string tname{"?"};
    getInput("target_name", tname);
    RCLCPP_INFO(node_->get_logger(),
        "🏃  [MoveTurtleToTarget] Chasing '%-10s'…", tname.c_str());
    return stepTowardTarget() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
}

BT::NodeStatus MoveTurtleToTarget::onRunning()
{
    return stepTowardTarget() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
}

void MoveTurtleToTarget::onHalted()
{
    geometry_msgs::msg::Twist stop;
    cmd_vel_pub_->publish(stop);
    RCLCPP_WARN(node_->get_logger(), "⛔  [MoveTurtleToTarget] halted — turtle stopped");
}

bool MoveTurtleToTarget::stepTowardTarget()
{
    double tx{0.0}, ty{0.0}, catch_dist{0.5}, kp_lin{1.5}, kp_ang{4.0};
    double max_lin{2.0}, max_ang{3.0}, heading_tol{0.4};
    getInput("target_x",          tx);
    getInput("target_y",          ty);
    getInput("catch_distance",    catch_dist);
    getInput("linear_gain",       kp_lin);
    getInput("angular_gain",      kp_ang);
    getInput("max_linear_speed",  max_lin);
    getInput("max_angular_speed", max_ang);
    getInput("heading_tolerance", heading_tol);

    double mx{0.0}, my{0.0}, mtheta{0.0};
    getMainPose(config().blackboard, mx, my, mtheta);

    const double dx   = tx - mx;
    const double dy   = ty - my;
    const double dist = std::hypot(dx, dy);

    if (dist < catch_dist) {
        geometry_msgs::msg::Twist stop;
        cmd_vel_pub_->publish(stop);
        return true;
    }

    const double target_angle = std::atan2(dy, dx);
    const double angle_error  = wrapAngle(target_angle - mtheta);

    geometry_msgs::msg::Twist cmd;
    if (std::abs(angle_error) > heading_tol) {
        cmd.linear.x = 0.0;   // turn in place until roughly aligned
    } else {
        cmd.linear.x = std::min(kp_lin * dist, max_lin);
    }
    cmd.angular.z = std::max(-max_ang, std::min(max_ang, kp_ang * angle_error));

    cmd_vel_pub_->publish(cmd);
    return false;
}

// ===========================================================================
// CatchTurtle
// ===========================================================================

CatchTurtle::CatchTurtle(const std::string& name, const BT::NodeConfig& config)
    : BT::StatefulActionNode(name, config)
{
    node_        = config.blackboard->get<rclcpp::Node*>("ros_node");
    kill_client_ = node_->create_client<turtlesim::srv::Kill>("/kill");
}

void CatchTurtle::removeFromPreyList(const std::string& name)
{
    std::vector<TurtleTarget> list;
    (void)config().blackboard->get("turtle_list", list);
    list.erase(
        std::remove_if(list.begin(), list.end(),
            [&](const TurtleTarget& t) { return t.name == name; }),
        list.end());
    config().blackboard->set("turtle_list", list);

    RCLCPP_INFO(node_->get_logger(), "📋  Prey remaining: %zu", list.size());
}

BT::NodeStatus CatchTurtle::onStart()
{
    getInput("target_name", target_name_);
    kill_acknowledged_ = false;

    RCLCPP_INFO(node_->get_logger(),
        "🪤  [CatchTurtle] Eliminating '%-10s'…", target_name_.c_str());

    if (!kill_client_->service_is_ready()) {
        RCLCPP_WARN(node_->get_logger(),
            "⚠️   /kill service not available — dropping '%s' from list",
            target_name_.c_str());
        removeFromPreyList(target_name_);
        return BT::NodeStatus::SUCCESS;
    }

    auto request  = std::make_shared<turtlesim::srv::Kill::Request>();
    request->name = target_name_;

    // Fire-and-poll: the callback runs on the same thread the controller loop
    // spins, so a plain bool flag is race-free here.
    kill_client_->async_send_request(request,
        [this](rclcpp::Client<turtlesim::srv::Kill>::SharedFuture) {
            kill_acknowledged_ = true;
        });

    deadline_ = node_->now() + rclcpp::Duration::from_seconds(2.0);
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus CatchTurtle::onRunning()
{
    if (kill_acknowledged_) {
        RCLCPP_INFO(node_->get_logger(),
            "💀  '%-10s' eliminated!", target_name_.c_str());
        removeFromPreyList(target_name_);
        return BT::NodeStatus::SUCCESS;
    }

    if (node_->now() >= deadline_) {
        RCLCPP_WARN(node_->get_logger(),
            "⚠️   Kill service timed out for '%s' — dropping from list",
            target_name_.c_str());
        removeFromPreyList(target_name_);
        return BT::NodeStatus::SUCCESS;
    }

    return BT::NodeStatus::RUNNING;
}

// ===========================================================================
// WaitForTurtle
// ===========================================================================

WaitForTurtle::WaitForTurtle(const std::string& name, const BT::NodeConfig& config)
    : BT::StatefulActionNode(name, config)
{
    node_ = config.blackboard->get<rclcpp::Node*>("ros_node");
}

BT::NodeStatus WaitForTurtle::onStart()
{
    start_time_ = node_->now();
    RCLCPP_INFO(node_->get_logger(), "😴  [WaitForTurtle] Canvas empty — waiting for prey…");
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus WaitForTurtle::onRunning()
{
    double timeout_sec{1.0};
    getInput("timeout_sec", timeout_sec);

    if ((node_->now() - start_time_).seconds() >= timeout_sec) {
        return BT::NodeStatus::FAILURE;
    }
    return BT::NodeStatus::RUNNING;
}
