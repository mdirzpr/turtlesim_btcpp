# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Two packages in this repo:

1. **`bt_turtlesim_ctrl`** — turtlesim-based prey-hunting demo. A spawner node drops prey turtles every 2–4 s (launch default); the BT controller drives `turtle1` to catch and eliminate them. Two hunting strategies are selectable at launch time. Groot2 on port **1669**.

2. **`bt_turtlesim_interfaces`** — custom ROS 2 message (`TurtleTarget.msg`) shared between the spawner and controller.

## Development Environment

All development happens inside a Docker container. Source packages are bind-mounted under `/dev_ws/src/`.

```bash
# Build the Docker image (only needed once or after Dockerfile changes)
docker compose build

# Start an interactive dev shell
docker compose run --rm dev bash

# Inside container — build the full workspace
# Build interfaces first (bt_turtlesim_ctrl depends on it)
cd /dev_ws && colcon build --symlink-install
source install/setup.bash

# Build a single package
colcon build --symlink-install --packages-select bt_turtlesim_ctrl
```

## Running

### bt_turtlesim_ctrl (prey-hunting demo)

```bash
# Sequential strategy — catch turtles in the order they appeared (FIFO)
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py

# Closest-first strategy — always chase the nearest prey
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py strategy:=closest

# Faster spawning (for testing)
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py min_interval:=2.0 max_interval:=4.0
```

Connect Groot2 to `localhost:1669` while the controller is running. Open `bt_turtlesim_ctrl/bt_structures/turtlesim.btproj` to edit trees in Groot2.

## Architecture

### bt_turtlesim_ctrl architecture

`turtle_controller_node.cpp` is the BT executor entry point. It registers the six BT nodes with a `BehaviorTreeFactory`, loads the strategy XML, seeds the blackboard, starts a `Groot2Publisher` on port 1669, then drives the tree with `tree_.tickExactlyOnce()` in a loop that sleeps 20 ms between ticks (~50 Hz). `turtle_spawner_node.cpp` is a separate node that periodically spawns prey turtles and publishes them on `/new_turtle`.

The `TreeNodesModel` section at the bottom of each strategy XML declares all port definitions for Groot2 display; keep it in sync when adding new nodes.

**Data flow:**
```
turtle_spawner  ──/new_turtle (TurtleTarget)──►  turtle_controller
turtlesim       ──/turtle1/pose──────────────►  turtle_controller
turtle_controller ──/turtle1/cmd_vel──────────►  turtlesim
turtle_controller ──/kill (srv)───────────────►  turtlesim
```

**Blackboard keys** (internal; not ports):

| Key | Type | Set by |
|-----|------|--------|
| `ros_node` | `rclcpp::Node*` | controller constructor |
| `turtle_list` | `std::vector<TurtleTarget>` | `/new_turtle` callback + `CatchTurtle` |
| `main_x/y/theta` | `double` | `/turtle1/pose` callback |
| `strategy` | `std::string` | launch parameter |

**BT node roster:**

| Node | Type | Description |
|------|------|-------------|
| `HasTargetTurtles` | Condition | SUCCESS when prey list non-empty |
| `IsTurtleCaught` | Condition | SUCCESS when within `catch_distance` |
| `SelectNextTurtle` | SyncAction | picks target; `strategy` port = "sequential"/"closest" |
| `MoveTurtleToTarget` | StatefulAction | proportional controller on `/turtle1/cmd_vel` |
| `CatchTurtle` | SyncAction | calls `/kill` service, removes from prey list |
| `WaitForTurtle` | StatefulAction | idles for `timeout_sec` then FAILURE (polite busy-wait) |

**Tree loop pattern:**
```
Repeat(∞)
  ForceSuccess          ← keeps loop alive when no prey exists
    Sequence
      Fallback
        HasTargetTurtles
        WaitForTurtle   ← 1-second idle when canvas is empty
      SelectNextTurtle  ← strategy hard-coded in the XML
      MoveTurtleToTarget
      CatchTurtle
```

### Adding a new BT node to bt_turtlesim_ctrl

1. Declare the class in `bt_turtlesim_ctrl/include/turtle_bt_nodes.h`
2. Implement it in `bt_turtlesim_ctrl/src/turtle_bt_nodes.cpp`
3. Register it in `turtle_controller_node.cpp` via `factory_.registerNodeType<MyNode>("MyNode")`
4. Add a `<MyNode>` entry under `<TreeNodesModel>` in both XML files if it has ports

### Configuration

**`bt_turtlesim_ctrl/config/params.yaml`:**
- `turtle_spawner.initial_spawn_count` — prey spawned immediately at startup (default 3, staggered ~0.4 s apart)
- `turtle_spawner.min_interval` / `max_interval` — spawn timing in seconds. Note: `params.yaml` and the launch file disagree — launch defaults are 2.0/4.0 and take effect for `ros2 launch`; `params.yaml` (0.1/1.0) only applies if the node is run directly with that file.
- `turtle_controller.strategy` — default strategy (overridden by the `strategy` port value hard-coded in the XML)

## Docker details

- **Network/IPC:** host mode required for ROS 2 DDS discovery
- **X11:** the container forwards `DISPLAY` for Groot2 GUI; ensure `xhost +local:docker` is run on the host if Groot2 fails to open
- **ROS_DOMAIN_ID=42** — set in `docker-compose.yaml` to avoid collisions with other ROS 2 processes
- **Build cache:** colcon artifact volumes (`dev_ws_build`, `dev_ws_install`) are persisted across container restarts

## Key file locations

| Path | Purpose |
|------|---------|
| `interfaces/msg/TurtleTarget.msg` | Custom message: prey turtle name + position |
| `interfaces/CMakeLists.txt` | Builds bt_turtlesim_interfaces (must build before ctrl) |
| `bt_turtlesim_ctrl/include/turtle_bt_nodes.h` | All turtlesim BT node declarations |
| `bt_turtlesim_ctrl/src/turtle_bt_nodes.cpp` | BT node implementations |
| `bt_turtlesim_ctrl/src/turtle_controller_node.cpp` | BT executor + ROS subscriptions |
| `bt_turtlesim_ctrl/src/turtle_spawner_node.cpp` | Periodic prey spawner |
| `bt_turtlesim_ctrl/bt_structures/turtle_hunter_sequential.xml` | Sequential strategy tree |
| `bt_turtlesim_ctrl/bt_structures/turtle_hunter_closest.xml` | Closest-first strategy tree |
| `bt_turtlesim_ctrl/bt_structures/turtlesim.btproj` | Groot2 project file |
| `bt_turtlesim_ctrl/launch/turtlesim_bt.launch.py` | Turtlesim demo launch file |
| `docker/Dockerfile` | Multi-stage image; installs turtlesim + project packages |
| `dependencies.repos` | vcstool spec for BehaviorTree.CPP source dependency |
