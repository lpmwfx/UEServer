"""Port discovery for UE RPC server via ~/.ueserver/switchboard.json"""

import json
import os
from pathlib import Path
from typing import Any

from .types import PortDiscoveryResult


def discover_port(project_dir: str | None = None) -> PortDiscoveryResult:
    """
    Discover UE RPC server port from ~/.ueserver/switchboard.json

    Args:
        project_dir: UE project directory (default: current working directory)

    Returns:
        PortDiscoveryResult with ok, port, pid, started fields

    The ~/.ueserver/switchboard.json file format:
        {
            "instances": [
                {
                    "pid": 12345,
                    "port": 45231,
                    "project": "/path/to/project.uproject",
                    "project_name": "MyProject",
                    "started": "2025-12-26T10:30:00Z"
                }
            ]
        }
    """
    if project_dir is None:
        project_dir = os.getcwd()

    # Switchboard file is in ~/.ueserver/
    switchboard_path = Path.home() / ".ueserver" / "switchboard.json"

    # Check if file exists
    if not switchboard_path.exists():
        return {
            "ok": False,
            "error": (
                f"UE RPC server not running: {switchboard_path} not found. "
                "Start UE5 with UEServer plugin enabled."
            ),
        }

    # Read and parse JSON
    try:
        with open(switchboard_path, "r", encoding="utf-8") as f:
            data: dict[str, Any] = json.load(f)
    except json.JSONDecodeError as err:
        return {
            "ok": False,
            "error": f"Invalid JSON in {switchboard_path}: {err}",
        }
    except OSError as err:
        return {
            "ok": False,
            "error": f"Cannot read {switchboard_path}: {err}",
        }

    # Validate switchboard structure
    if "instances" not in data:
        return {
            "ok": False,
            "error": f"Missing 'instances' field in {switchboard_path}",
        }

    instances = data["instances"]
    if not isinstance(instances, list):
        return {
            "ok": False,
            "error": f"Invalid 'instances' type in {switchboard_path}: expected list",
        }

    if len(instances) == 0:
        return {
            "ok": False,
            "error": "No UE instances running in switchboard",
        }

    # Find instance for current project directory
    project_dir_path = Path(project_dir).resolve()

    for instance in instances:
        # Check if this instance matches our project
        instance_project = instance.get("project", "")
        if instance_project:
            instance_project_path = Path(instance_project).resolve()
            # Match if the project file is in our project directory
            if str(instance_project_path).startswith(str(project_dir_path)):
                port = instance.get("port")
                pid = instance.get("pid")
                started = instance.get("started", "")

                # Validate required fields
                if port is None:
                    return {
                        "ok": False,
                        "error": "Instance missing 'port' field",
                    }
                if pid is None:
                    return {
                        "ok": False,
                        "error": "Instance missing 'pid' field",
                    }

                # Validate types
                if not isinstance(port, int):
                    return {
                        "ok": False,
                        "error": f"Invalid port type: expected int, got {type(port).__name__}",
                    }
                if not isinstance(pid, int):
                    return {
                        "ok": False,
                        "error": f"Invalid pid type: expected int, got {type(pid).__name__}",
                    }

                # Validate PID
                if not _is_process_running(pid):
                    return {
                        "ok": False,
                        "error": (
                            f"Stale instance detected: process {pid} is not running. "
                            f"Restart UE5 or clean {switchboard_path}."
                        ),
                    }

                return {
                    "ok": True,
                    "port": port,
                    "pid": pid,
                    "started": started,
                }

    # If only one instance, use it (fallback for convenience)
    if len(instances) == 1:
        instance = instances[0]
        port = instance.get("port")
        pid = instance.get("pid")
        started = instance.get("started", "")

        if port and pid and _is_process_running(pid):
            return {
                "ok": True,
                "port": port,
                "pid": pid,
                "started": started,
            }

    return {
        "ok": False,
        "error": (
            f"No matching UE instance found for {project_dir}. "
            f"Found {len(instances)} instance(s) in switchboard."
        ),
    }


def _is_process_running(pid: int) -> bool:
    """
    Check if a process with given PID is running.

    Args:
        pid: Process ID to check

    Returns:
        True if process is running, False otherwise
    """
    try:
        # Send signal 0 to check if process exists (doesn't actually send a signal)
        os.kill(pid, 0)
        return True
    except OSError:
        return False
