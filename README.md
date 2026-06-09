# 🤖 BehaviorTree.CPP ROS 2 Demo

A **BehaviorTree.CPP v4** showcase running in a containerized ROS 2 Jazzy environment with live Groot2 visualization.

| Package | What it does | Groot2 port |
|---------|-------------|-------------|
| `bt_turtlesim_ctrl` | Turtlesim prey-hunting demo — a hunter turtle chases and eliminates spawned prey turtles using a behavior tree with two selectable strategies | **1669** |

---

## 🚀 Quick Start

### 1. Build the Docker image

```bash
docker compose build
```

### 2. Start a container shell

```bash
# Interactive (recommended)
docker compose run --rm dev bash

# Or background + attach
docker compose up -d dev
docker compose exec dev bash
```

### 3. Build the ROS 2 workspace

```bash
cd /dev_ws
colcon build --symlink-install
source install/setup.bash
```

### 4. Run a demo

```bash
# 🐢 Turtlesim prey-hunter (sequential strategy)
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py

# 🐢 Turtlesim prey-hunter (closest-first strategy)
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py strategy:=closest
```

Groot2 opens automatically. Click **Connect** and enter the port for the demo.

---

## 🐢 Turtlesim Prey-Hunting Demo

### How it works

```
turtle_spawner  ──(/new_turtle)──►  turtle_controller  ──(/turtle1/cmd_vel)──►  turtlesim
                                                        ◄──(/turtle1/pose)──────
                                                        ──(/kill)──────────────►
```

1. **`turtle_spawner`** creates a new prey turtle at a random position on the canvas every 5–7 seconds (configurable).
2. **`turtle_controller`** runs a behavior tree that drives `turtle1` to catch and eliminate each prey.
3. Two hunting strategies are available — selected at launch time:

| Strategy | Behavior |
|----------|----------|
| `sequential` | Catch turtles in the order they appeared (FIFO) |
| `closest` | Always chase the nearest turtle first (greedy) |

### Launch options

```bash
# Default (sequential, spawn every 5–7 s, Groot2 enabled)
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py

# Closest-first strategy
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py strategy:=closest

# Faster spawning for testing
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py min_interval:=2.0 max_interval:=4.0

# Without Groot2
ros2 launch bt_turtlesim_ctrl turtlesim_bt.launch.py enable_groot:=false
```

### BT nodes

#### Condition nodes
| Node | Returns SUCCESS when… |
|------|----------------------|
| `HasTargetTurtles` | Prey list is non-empty |
| `IsTurtleCaught` | `turtle1` is within `catch_distance` (0.5 m) of the target |

#### Action nodes
| Node | Type | Description |
|------|------|-------------|
| `SelectNextTurtle` | Sync | Picks the next target from the prey list using `strategy` port |
| `MoveTurtleToTarget` | Stateful | Proportional controller — turns then drives toward the target |
| `CatchTurtle` | Sync | Calls `/kill` service and removes turtle from the prey list |
| `WaitForTurtle` | Stateful | Idles for `timeout_sec` when the canvas is empty, then FAILURE |

### Behavior tree structure

```
Repeat(∞)
  ForceSuccess           ← keeps the loop alive even when no prey exists
    Sequence "HuntOneTurtle"
      Fallback "WaitForPrey"
        HasTargetTurtles        ← succeed immediately if prey exists
        WaitForTurtle(1 s)      ← polite idle when canvas is empty
      SelectNextTurtle          ← strategy hard-coded in the XML
      MoveTurtleToTarget        ← proportional controller
      CatchTurtle               ← /kill + remove from list
```

Both strategy XMLs are in `bt_turtlesim_ctrl/bt_structures/`. Open `turtlesim.btproj` in Groot2 to edit or inspect them.

---

## 🌳 Groot2 Visualization

Groot2 launches automatically with every demo. To connect manually:

```bash
~/Groot2.AppImage
```

| Demo | Port |
|------|------|
| `bt_turtlesim_ctrl` | `localhost:1669` |

1. Open Groot2 → click **Connect**
2. Enter the port above
3. The live tree will appear with node status colors

---

## 📁 Project Structure

```
bt_turtlesim/
├── docker/
│   └── Dockerfile                    # Multi-stage build (base + dev)
├── docker-compose.yaml
├── dependencies.repos                # BehaviorTree.CPP source dependency
│
├── interfaces/                       # bt_turtlesim_interfaces package
│   └── msg/TurtleTarget.msg          # name + x + y of a spawned prey
│
└── bt_turtlesim_ctrl/                # Turtlesim prey-hunting package
    ├── include/turtle_bt_nodes.h     # All BT node declarations
    ├── src/
    │   ├── turtle_bt_nodes.cpp       # BT node implementations
    │   ├── turtle_controller_node.cpp# BT executor + ROS subscriptions
    │   └── turtle_spawner_node.cpp   # Periodic prey spawner
    ├── bt_structures/
    │   ├── turtle_hunter_sequential.xml
    │   ├── turtle_hunter_closest.xml
    │   └── turtlesim.btproj          # Groot2 project file
    ├── launch/turtlesim_bt.launch.py
    └── config/params.yaml
```

---

## 🐳 Docker Reference

| Command | Description |
|---------|-------------|
| `docker compose build` | Build the image |
| `docker compose run --rm dev bash` | Start interactive container |
| `docker compose up -d dev` | Start in background |
| `docker compose exec dev bash` | Attach to running container |
| `docker compose down` | Stop and remove containers |
| `docker compose logs -f dev` | View logs |

To open a second terminal in a running container:
```bash
docker compose exec dev bash
source /dev_ws/install/setup.bash
```

---

## ⚠️ Troubleshooting

**X11 / Groot2 display issues** — run on the host before starting the container:
```bash
xhost +local:docker
```

**ROS 2 domain isolation** — the container uses `ROS_DOMAIN_ID=42` to avoid conflicts with other ROS 2 nodes on the network.

**Groot2 builtin model warnings** — harmless; suppressed by routing Groot2 output to `log` in the launch file.

**Build order** — `bt_turtlesim_interfaces` must be built before `bt_turtlesim_ctrl`. A plain `colcon build` handles this automatically.

---

## 📝 License

MIT — see [LICENSE](LICENSE) for details.
