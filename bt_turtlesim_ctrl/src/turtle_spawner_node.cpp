// ============================================================================
// TURTLE SPAWNER NODE
// ============================================================================
#include "rclcpp/rclcpp.hpp"
#include "turtlesim/srv/spawn.hpp"
#include "bt_turtlesim_interfaces/msg/turtle_target.hpp"

#include <random>
#include <chrono>
#include <string>

using namespace std::chrono_literals;

class TurtleSpawnerNode : public rclcpp::Node
{
public:
    TurtleSpawnerNode()
        : Node("turtle_spawner"),
          turtle_count_(0),
          rng_(std::random_device{}())
    {
        this->declare_parameter<double>("min_interval",        2.0);
        this->declare_parameter<double>("max_interval",        4.0);
        this->declare_parameter<int>   ("initial_spawn_count", 3);

        const double min_s   = this->get_parameter("min_interval").as_double();
        const double max_s   = this->get_parameter("max_interval").as_double();
        const int    initial = this->get_parameter("initial_spawn_count").as_int();

        spawn_client_   = this->create_client<turtlesim::srv::Spawn>("/spawn");
        new_turtle_pub_ = this->create_publisher<
            bt_turtlesim_interfaces::msg::TurtleTarget>("/new_turtle", 10);

        RCLCPP_INFO(this->get_logger(), "╔══════════════════════════════════════════════════╗");
        RCLCPP_INFO(this->get_logger(), "║        🥚  Turtle Spawner  🐢                    ║");
        RCLCPP_INFO(this->get_logger(), "╠══════════════════════════════════════════════════╣");
        RCLCPP_INFO(this->get_logger(), "║  Initial burst : %-3d turtles%-21s║", initial, "");
        RCLCPP_INFO(this->get_logger(), "║  Then every    : %.1f – %.1f seconds%-14s║",
            min_s, max_s, "");
        RCLCPP_INFO(this->get_logger(), "║  Topic         : /new_turtle                     ║");
        RCLCPP_INFO(this->get_logger(), "╚══════════════════════════════════════════════════╝");

        // Wait briefly for turtlesim to come up, then fire the initial burst.
        // Each burst turtle is staggered by 0.4 s to avoid service call collisions.
        for (int i = 0; i < initial; ++i) {
            auto delay = std::chrono::duration<double>(0.5 + i * 0.4);
            burst_timers_.push_back(
                this->create_wall_timer(delay, [this]() {
                    burst_timers_.front()->cancel();
                    burst_timers_.erase(burst_timers_.begin());
                    do_spawn();
                    if (burst_timers_.empty()) {
                        RCLCPP_INFO(this->get_logger(),
                            "🐣  Initial burst complete — switching to periodic mode");
                        schedule_next_spawn();
                    }
                }));
        }

        // If no initial burst requested, go straight to periodic mode.
        if (initial <= 0) {
            schedule_next_spawn();
        }
    }

private:
    void schedule_next_spawn()
    {
        const double min_s = this->get_parameter("min_interval").as_double();
        const double max_s = this->get_parameter("max_interval").as_double();
        std::uniform_real_distribution<double> dist(min_s, max_s);
        const double interval_s = dist(rng_);

        RCLCPP_INFO(this->get_logger(), "⏱️   Next spawn in %.1f s", interval_s);

        periodic_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(interval_s),
            [this]() {
                periodic_timer_->cancel();
                do_spawn();
                schedule_next_spawn();
            });
    }

    void do_spawn()
    {
        if (!spawn_client_->wait_for_service(1s)) {
            RCLCPP_WARN(this->get_logger(),
                "⚠️   /spawn service not available — skipping cycle");
            return;
        }

        ++turtle_count_;
        const std::string name = "prey_" + std::to_string(turtle_count_);

        std::uniform_real_distribution<float> pos(1.0f, 10.0f);
        std::uniform_real_distribution<float> angle(0.0f,
            static_cast<float>(2.0 * M_PI));

        const float x     = pos(rng_);
        const float y     = pos(rng_);
        const float theta = angle(rng_);

        auto request   = std::make_shared<turtlesim::srv::Spawn::Request>();
        request->name  = name;
        request->x     = x;
        request->y     = y;
        request->theta = theta;

        RCLCPP_INFO(this->get_logger(),
            "🥚  Spawning '%-10s' at (x=%.2f, y=%.2f, θ=%.2f rad)…",
            name.c_str(), x, y, theta);

        spawn_client_->async_send_request(
            request,
            [this, name, x, y](
                rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future)
            {
                const auto response = future.get();
                if (response->name.empty()) {
                    RCLCPP_ERROR(this->get_logger(),
                        "❌  Spawn FAILED for '%s'", name.c_str());
                    return;
                }
                RCLCPP_INFO(this->get_logger(),
                    "✅  '%s' alive at (%.2f, %.2f) — total spawned: %d",
                    response->name.c_str(), x, y, turtle_count_);

                bt_turtlesim_interfaces::msg::TurtleTarget msg;
                msg.name = response->name;
                msg.x    = x;
                msg.y    = y;
                new_turtle_pub_->publish(msg);
            });
    }

    rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr              spawn_client_;
    rclcpp::Publisher<
        bt_turtlesim_interfaces::msg::TurtleTarget>::SharedPtr    new_turtle_pub_;

    // Staggered one-shot timers for the initial burst
    std::vector<rclcpp::TimerBase::SharedPtr>                     burst_timers_;
    // Recurring periodic timer
    rclcpp::TimerBase::SharedPtr                                  periodic_timer_;

    int           turtle_count_;
    std::mt19937  rng_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<TurtleSpawnerNode>());
    rclcpp::shutdown();
    return 0;
}
