#!/bin/bash
# Enhanced entrypoint for ROS Docker containers with visual feedback

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Unicode checkmark in box
CHECK="✅"
CROSS="❌"
INFO="ℹ️ "

# Set workspace variable
BASE_WS=${BASE_WS:-bt_ros2_ws}

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}   BT Engine Development Environment${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Source ROS 2
if source /opt/ros/${ROS_DISTRO}/setup.bash 2>/dev/null; then
  echo -e "${CHECK} ${GREEN}Sourced ROS 2 ${ROS_DISTRO}${NC}"
else
  echo -e "${CROSS} ${RED}Failed to source ROS 2 ${ROS_DISTRO}${NC}"
fi

# Source the base workspace, if built
if [ -f /${BASE_WS}/install/setup.bash ]; then
  if source /${BASE_WS}/install/setup.bash 2>/dev/null; then
    echo -e "${CHECK} ${GREEN}Sourced base workspace: ${BASE_WS}${NC}"
  else
    echo -e "${CROSS} ${RED}Failed to source base workspace: ${BASE_WS}${NC}"
  fi
else
  echo -e "${INFO}${YELLOW}Base workspace not built yet: ${BASE_WS}${NC}"
fi

# Source the dev workspace, if built
if [ -f /dev_ws/install/setup.bash ]; then
  if source /dev_ws/install/setup.bash 2>/dev/null; then
    echo -e "${CHECK} ${GREEN}Sourced dev workspace: /dev_ws${NC}"
  else
    echo -e "${CROSS} ${RED}Failed to source dev workspace: /dev_ws${NC}"
  fi
else
  echo -e "${INFO}${YELLOW}Dev workspace not built yet: /dev_ws${NC}"
fi

# Run example script if exists
if [ -f /docker/example_run_all.sh ]; then
  /docker/example_run_all.sh
fi

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}Ready! ${NC}Type your commands below or run:"
echo -e "  ${BLUE}colcon build --packages-select bt_engine${NC}"
echo -e "  ${BLUE}ros2 launch bt_engine opcua_test.launch.py${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Execute the command passed into this entrypoint
exec "$@"
