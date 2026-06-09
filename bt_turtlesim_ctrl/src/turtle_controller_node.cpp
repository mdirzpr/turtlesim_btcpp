// ============================================================================
// TURTLE CONTROLLER NODE
// ============================================================================
#include "rclcpp/rclcpp.hpp"
#include "ament_index_cpp/get_package_share_directory.hpp"

#include "behaviortree_cpp/bt_factory.h"
#include "behaviortree_cpp/xml_parsing.h"
#include "behaviortree_cpp/loggers/groot2_publisher.h"

#include "turtlesim/msg/pose.hpp"
#include "bt_turtlesim_interfaces/msg/turtle_target.hpp"

#include "turtle_bt_nodes.h"

#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;

class TurtleControllerNode : public rclcpp::Node
{
public:
    TurtleControllerNode()
        : Node("turtle_controller")
    {
        this->declare_parameter<std::string>("tree_xml_file", "");
        this->declare_parameter<std::string>("strategy", "sequential");
        this->declare_parameter<int>("groot2_port", 1669);

        tree_xml_file_ = this->get_parameter("tree_xml_file").as_string();
        strategy_      = this->get_parameter("strategy").as_string();
        groot2_port_   = this->get_parameter("groot2_port").as_int();

        RCLCPP_INFO(this->get_logger(), "╔══════════════════════════════════════════════════╗");
        RCLCPP_INFO(this->get_logger(), "║       🐢  Turtlesim BT Controller  🌳            ║");
        RCLCPP_INFO(this->get_logger(), "╠══════════════════════════════════════════════════╣");
        RCLCPP_INFO(this->get_logger(), "║  Strategy : %-37s║", strategy_.c_str());
        RCLCPP_INFO(this->get_logger(), "║  Groot2   : port %-32d║", groot2_port_);
        RCLCPP_INFO(this->get_logger(), "╚══════════════════════════════════════════════════╝");

        // ------------------------------------------------------------------
        // Blackboard
        // ------------------------------------------------------------------
        blackboard_ = BT::Blackboard::create();
        blackboard_->set("ros_node",    static_cast<rclcpp::Node*>(this));
        blackboard_->set("strategy",    strategy_);
        blackboard_->set("turtle_list", std::vector<TurtleTarget>{});
        blackboard_->set("main_x",      0.0);
        blackboard_->set("main_y",      0.0);
        blackboard_->set("main_theta",  0.0);

        // ------------------------------------------------------------------
        // /turtle1/pose  →  update blackboard main pose
        // ------------------------------------------------------------------
        pose_sub_ = this->create_subscription<turtlesim::msg::Pose>(
            "/turtle1/pose", 10,
            [this](const turtlesim::msg::Pose::SharedPtr msg) {
                blackboard_->set("main_x",     static_cast<double>(msg->x));
                blackboard_->set("main_y",     static_cast<double>(msg->y));
                blackboard_->set("main_theta", static_cast<double>(msg->theta));
            });

        // ------------------------------------------------------------------
        // /new_turtle  →  add entry to blackboard turtle_list
        // ------------------------------------------------------------------
        new_turtle_sub_ = this->create_subscription<
            bt_turtlesim_interfaces::msg::TurtleTarget>(
            "/new_turtle", 10,
            [this](const bt_turtlesim_interfaces::msg::TurtleTarget::SharedPtr msg) {
                std::vector<TurtleTarget> list;
                (void)blackboard_->get("turtle_list", list);

                TurtleTarget t;
                t.name = msg->name;
                t.x    = static_cast<double>(msg->x);
                t.y    = static_cast<double>(msg->y);
                list.push_back(t);

                blackboard_->set("turtle_list", list);
                RCLCPP_INFO(this->get_logger(),
                    "🐢  New prey spotted: '%-10s' at (x=%.2f, y=%.2f)  |  🎯 queue: %zu",
                    t.name.c_str(), t.x, t.y, list.size());
            });

        setup_behavior_tree();
    }

    void execute()
    {
        RCLCPP_INFO(this->get_logger(), "🟢  BT running — Ctrl+C to stop");

        auto status = BT::NodeStatus::RUNNING;

        while (rclcpp::ok()) {
            rclcpp::spin_some(this->get_node_base_interface());

            if (status == BT::NodeStatus::RUNNING) {
                status = tree_.tickExactlyOnce();
            } else {
                static bool logged = false;
                if (!logged) {
                    if (status == BT::NodeStatus::SUCCESS)
                        RCLCPP_INFO(this->get_logger(), "✅  Tree finished: SUCCESS");
                    else
                        RCLCPP_WARN(this->get_logger(), "❌  Tree finished: FAILURE");
                    RCLCPP_INFO(this->get_logger(),
                        "🌳  Groot2 still active on port %d", groot2_port_);
                    logged = true;
                }
            }

            rclcpp::sleep_for(std::chrono::milliseconds(20));
        }
    }

    void cleanup() { groot_pub_.reset(); }

private:
    void setup_behavior_tree()
    {
        factory_.registerNodeType<HasTargetTurtles>("HasTargetTurtles");
        factory_.registerNodeType<IsTurtleCaught>("IsTurtleCaught");
        factory_.registerNodeType<SelectNextTurtle>("SelectNextTurtle");
        factory_.registerNodeType<MoveTurtleToTarget>("MoveTurtleToTarget");
        factory_.registerNodeType<CatchTurtle>("CatchTurtle");
        factory_.registerNodeType<WaitForTurtle>("WaitForTurtle");

        factory_.registerBehaviorTreeFromFile(tree_xml_file_);
        tree_ = factory_.createTree("TurtleHunter", blackboard_);

        groot_pub_ = std::make_unique<BT::Groot2Publisher>(tree_, groot2_port_);
        RCLCPP_INFO(this->get_logger(),
            "🌳  BT loaded — Groot2 publisher active on port %d", groot2_port_);
    }

    std::string tree_xml_file_;
    std::string strategy_;
    int         groot2_port_{1669};

    BT::BehaviorTreeFactory              factory_;
    BT::Blackboard::Ptr                  blackboard_;
    BT::Tree                             tree_;
    std::unique_ptr<BT::Groot2Publisher> groot_pub_;

    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr                        pose_sub_;
    rclcpp::Subscription<bt_turtlesim_interfaces::msg::TurtleTarget>::SharedPtr  new_turtle_sub_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControllerNode>();
    node->execute();
    node->cleanup();
    node.reset();
    rclcpp::shutdown();
    return 0;
}
