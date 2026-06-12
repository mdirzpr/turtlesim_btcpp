################################################################################
# 🤖 BehaviorTree ROS 2 Example - Multi-Stage Dockerfile
#
# Author: Mahdi Rezapour (mahdi.rezapour@dfki.de)
# Created: 2024
# Updated: 2025-02
#
# Description:
#   Multi-stage Docker build for BehaviorTree demonstration using:
#   - ROS 2 Jazzy (Robot Operating System)
#   - BehaviorTree.CPP (Behavior tree execution engine)
#   - Groot2 (Behavior tree visualization)
#
# Stages:
#   1. base - Core dependencies and external packages
#   2. dev  - Development environment with non-root user
#
# Build Args:
#   ROS_DISTRO    - ROS 2 distribution (default: jazzy)
#   GROOT2_VERSION- Groot2 version (default: v1.6.1)
#   USERNAME      - Development user name (default: devuser)
#   UID/GID       - User/Group ID (default: 1000)
################################################################################

ARG ROS_DISTRO=jazzy
ARG GROOT2_VERSION=v1.6.1

################################################################################
# 📦 STAGE 1: BASE IMAGE
#
# Purpose:
#   - Foundation layer with ROS 2 and all external dependencies
#   - Contains BehaviorTree.CPP and Groot2
#   - Shared by all subsequent stages to maximize layer reuse
#   - Built in /bt_ros2_ws workspace (base workspace)
#
# Key Components:
#   - ROS 2 Jazzy base image
#   - System libraries and build tools
#   - External ROS packages (BehaviorTree.CPP)
#   - Groot2 behavior tree visualization tool
################################################################################

FROM ros:jazzy AS base

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV ROS_DISTRO=${ROS_DISTRO}
SHELL ["/bin/bash", "-c"]

################################################################################
# 🔧 SYSTEM DEPENDENCIES
#
# Package groups:
#   - Build tools: curl, git, build-essential, cmake
#   - GUI libraries: libcanberra-gtk*, libqt5svg5-dev, libzmq5-dev
#   - FUSE support: fuse3, libfuse2 (required for AppImage execution)
#   - Python: python3-pip, python3-opencv, python3-tk, python3-pyqt5
#   - XML parsing: libxml2-dev, libxslt1-dev
#   - ROS middleware: rmw-cyclonedds-cpp (DDS implementation)
################################################################################

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl git build-essential cmake \
    libcanberra-gtk-module libcanberra-gtk3-module \
    fuse3 libfuse2 libqt5svg5-dev libzmq5-dev\
    python3-pip python3-opencv python3-tk python3-pyqt5.qtwebengine \
    python3-dev libxml2-dev libxslt1-dev iputils-ping \
    ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
    ros-${ROS_DISTRO}-turtlesim \
    && rm -rf /var/lib/apt/lists/*

# Set CycloneDDS as default RMW implementation (better performance for local communication)
ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

################################################################################
# 🐍 PYTHON DEPENDENCIES
#
# Installed system-wide with --break-system-packages (Ubuntu 23.04+ requirement):
#   - matplotlib: Plotting and visualization
#   - transforms3d: 3D transformations for robotics (quaternions, rotation matrices)
################################################################################
RUN pip3 install --break-system-packages matplotlib transforms3d

################################################################################
# 🏗️ BASE WORKSPACE: /bt_ros2_ws
#
# Purpose:
#   - Build external dependencies that rarely change
#   - Installed once in base layer for fast development rebuilds
#   - Source: dependencies.repos (vcstool format)
#
# Contents (from dependencies.repos):
#   - BehaviorTree.CPP: Core behavior tree execution engine
#
# Build process:
#   1. Import repositories with vcstool (vcs import)
#   2. Install dependencies with rosdep
#   3. Build with colcon (creates install/, build/, log/ directories)
################################################################################
ARG PROJECT_WS=bt_ros2_ws
RUN mkdir -p /${PROJECT_WS}/src
WORKDIR /${PROJECT_WS}/src

# Copy and import external repository definitions
COPY dependencies.repos .
RUN vcs import < dependencies.repos

# Build base workspace with all external dependencies
WORKDIR /${PROJECT_WS}
RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
    && apt-get update -y \
    # Install ROS dependencies automatically detected by rosdep
    && rosdep install --from-paths src --ignore-src --rosdistro ${ROS_DISTRO} -y \
    # Install additional required packages
    && apt-get update && apt-get install -y \
    libboost-all-dev \
    ros-${ROS_DISTRO}-std-msgs \
    ros-${ROS_DISTRO}-sensor-msgs \
    ros-${ROS_DISTRO}-geometry-msgs \
    ros-${ROS_DISTRO}-rosidl-default-generators \
    # Build all packages with symlink-install for faster rebuilds
    && colcon build --symlink-install

################################################################################
# 🌳 GROOT2 BEHAVIOR TREE VISUALIZER
#
# Purpose:
#   - Real-time visualization of behavior tree execution
#   - Connects to running ROS nodes via ZMQ (bt_turtlesim_ctrl uses port 1669)
#   - AppImage format for standalone execution
#
# Version: Configurable via GROOT2_VERSION build arg (default: v1.6.1)
# Location: /root/Groot2.AppImage (moved to user home in dev stage)
# Execution: Requires FUSE and X11 display forwarding
################################################################################
ARG GROOT2_VERSION
WORKDIR /root/
RUN curl --fail -L -o Groot2.AppImage https://s3.us-west-1.amazonaws.com/download.behaviortree.dev/groot2_linux_installer/Groot2-${GROOT2_VERSION}-x86_64.AppImage \
 && chmod a+x Groot2.AppImage

################################################################################
# 🖥️ X11 DISPLAY CONFIGURATION
#
# Purpose:
#   - Configure environment to suppress GUI warnings in container
#   - Required for running graphical applications (Groot2, RViz, etc.)
#
# Settings:
#   - XDG_RUNTIME_DIR: Runtime directory for user services
#   - NO_AT_BRIDGE: Disable accessibility bridge warnings
#
# ⚠️ IMPORTANT: Before running GUI apps, execute on the HOST:
#   xhost +local:docker
#   (or use the provided setup_display.sh script)
################################################################################
RUN mkdir /tmp/runtime-root
ENV XDG_RUNTIME_DIR "/tmp/runtime-root"
RUN chmod -R 0700 /tmp/runtime-root
ENV NO_AT_BRIDGE 1

################################################################################
# 🚀 ENTRYPOINT SCRIPT
#
# Purpose:
#   - Auto-source ROS 2 and workspace environments on container start
#   - Executed for both interactive shells and command execution
#
# Script location: /entrypoint.sh
# Sources (in order):
#   1. /opt/ros/${ROS_DISTRO}/setup.bash (ROS 2 base)
#   2. /bt_ros2_ws/install/setup.bash (external dependencies)
#   3. /dev_ws/install/setup.bash (development packages - dev stage only)
################################################################################
WORKDIR /${PROJECT_WS}
COPY ./docker/entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT [ "/entrypoint.sh" ]

################################################################################
# 🛠️ STAGE 2: DEVELOPMENT IMAGE
#
# Purpose:
#   - Interactive development environment with non-root user
#   - Contains development tools (debugger, editors)
#   - Overlay workspace (/dev_ws) for project packages
#   - Volume mounts for live code editing
#
# Key Features:
#   - Non-root user with sudo access (security best practice)
#   - Persistent build artifacts via volume mounts
#   - Source code mounted for live development
#   - All base dependencies inherited from base stage
################################################################################
FROM base AS dev

################################################################################
# 👤 USER CONFIGURATION
#
# Build arguments (can be overridden via docker-compose or --build-arg):
#   - USERNAME: Development user name (default: devuser)
#   - UID: User ID (default: 1000, matches host user for file permissions)
#   - GID: Group ID (default: same as UID)
#
# User setup:
#   - Create user with matching UID/GID for seamless file permissions
#   - Grant passwordless sudo access for package installation
#   - Transfer ownership of workspaces from root to dev user
################################################################################
ARG USERNAME=devuser
ARG UID=1000
ARG GID=${UID}

################################################################################
# 🔧 DEVELOPMENT TOOLS
#
# Additional packages for debugging and development:
#   - gdb/gdbserver: C++ debugging
#   - nano: Text editor for quick edits
#   - libcurl4-openssl-dev: HTTP client library (for API communication)
#   - nlohmann-json3-dev: Modern C++ JSON library
################################################################################
RUN apt-get update && apt-get install -y --no-install-recommends \
    gdb gdbserver nano \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y libcurl4-openssl-dev nlohmann-json3-dev && \
    rm -rf /var/lib/apt/lists/*

################################################################################
# 👤 USER CREATION
#
# Process:
#   1. Remove pre-existing 'ubuntu' user (common in Ubuntu base images)
#   2. Create new user group with specified GID
#   3. Create new user with specified UID and home directory
#   4. Grant passwordless sudo privileges
#   5. Set proper ownership of home directory
################################################################################

# Handle pre-existing user with UID=1000 (common in Ubuntu images)
RUN id ubuntu 2>/dev/null \
   && (touch /var/mail/ubuntu \
   && chown ubuntu /var/mail/ubuntu \
   && userdel -r ubuntu) || true

# Create development user with sudo privileges
RUN groupadd --gid $GID $USERNAME \
    && useradd --uid ${GID} --gid ${UID} --create-home ${USERNAME} \
    && echo ${USERNAME} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME} \
    && mkdir -p /home/${USERNAME} \
    && chown -R ${UID}:${GID} /home/${USERNAME}

# Transfer base workspace ownership to dev user
RUN chown -R ${UID}:${GID} /${PROJECT_WS}/

################################################################################
# 🌳 GROOT2 SETUP FOR NON-ROOT USER
#
# Steps:
#   1. Create 'fuse' group (required for AppImage FUSE filesystem)
#   2. Add dev user to 'fuse' group
#   3. Move Groot2 from /root to user home directory
#   4. Set proper ownership for execution
################################################################################
RUN groupadd fuse && usermod -aG fuse ${USERNAME}
RUN mv /root/Groot2.AppImage /home/${USERNAME}/Groot2.AppImage \
    && chown ${UID}:${GID} /home/${USERNAME}/Groot2.AppImage

################################################################################
# 📁 OVERLAY WORKSPACE: /dev_ws
#
# Purpose:
#   - Development workspace for project-specific packages
#   - Overlays base workspace (/bt_ros2_ws) for dependency resolution
#   - Mounted as volume for live code editing (see docker-compose.yaml)
#
# Initial build (in image):
#   - Copies bt_turtlesim_ctrl + interfaces packages
#   - Builds with colcon to verify compilation
#
# Runtime (with volume mounts):
#   - Source code mounted from host (live editing)
#   - Build artifacts in .colcon/ (persistent across container restarts)
#   - Rebuild with: colcon build --symlink-install
################################################################################
RUN mkdir -p /dev_ws/src
WORKDIR /dev_ws

# Copy project packages for initial build
COPY ./interfaces/        ./src/bt_turtlesim_interfaces/
COPY ./bt_turtlesim_ctrl/ ./src/bt_turtlesim_ctrl/

# Build overlay workspace (interfaces must be built before bt_turtlesim_ctrl)
RUN source /${PROJECT_WS}/install/setup.bash \
    && rosdep install --from-paths src --ignore-src --rosdistro ${ROS_DISTRO} -y \
    && colcon build --symlink-install

# Transfer dev workspace ownership to dev user
RUN chown -R ${UID}:${GID} /dev_ws/

################################################################################
# ✅ FINAL USER SETUP
#
# Steps:
#   1. Switch to non-root user for all subsequent operations
#   2. Auto-source entrypoint in .bashrc for interactive shells
#
# Entrypoint sourcing order:
#   - ROS 2 base environment
#   - Base workspace (/bt_ros2_ws)
#   - Dev workspace (/dev_ws)
################################################################################
USER ${USERNAME}
RUN echo "source /entrypoint.sh" >> /home/${USERNAME}/.bashrc
