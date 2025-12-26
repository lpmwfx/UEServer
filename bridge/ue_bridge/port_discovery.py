"""Port discovery for UE RPC server via .ueserver/rpc.json"""

import json
import os
from pathlib import Path
from typing import Any

from .types import PortDiscoveryResult


def discover_port(project_dir: str | None = None) -> PortDiscoveryResult:
    """
    Discover UE RPC server port from .ueserver/rpc.json

    Args:
        project_dir: UE project directory (default: current working directory)

    Returns:
        PortDiscoveryResult with ok, port, pid, started fields

    The .ueserver/rpc.json file format:
        {
            "port": 45231,
            "pid": 12345,
            "started": "2025-12-26T10:30:00Z"
        }
    """
    if project_dir is None:
        project_dir = os.getcwd()

    rpc_json_path = Path(project_dir) / ".ueserver" / "rpc.json"

    # Check if file exists
    if not rpc_json_path.exists():
        return {
            "ok": False,
            "error": (
                f"UE RPC server not running: {rpc_json_path} not found. "
                "Start UE5 with UEServer plugin enabled."
            ),
        }

    # Read and parse JSON
    try:
        with open(rpc_json_path, "r", encoding="utf-8") as f:
            data: dict[str, Any] = json.load(f)
    except json.JSONDecodeError as err:
        return {
            "ok": False,
            "error": f"Invalid JSON in {rpc_json_path}: {err}",
        }
    except OSError as err:
        return {
            "ok": False,
            "error": f"Cannot read {rpc_json_path}: {err}",
        }

    # Validate required fields
    if "port" not in data:
        return {
            "ok": False,
            "error": f"Missing 'port' field in {rpc_json_path}",
        }

    if "pid" not in data:
        return {
            "ok": False,
            "error": f"Missing 'pid' field in {rpc_json_path}",
        }

    port = data["port"]
    pid = data["pid"]
    started = data.get("started", "")

    # Validate types
    if not isinstance(port, int):
        return {
            "ok": False,
            "error": f"Invalid 'port' type in {rpc_json_path}: expected int, got {type(port).__name__}",
        }

    if not isinstance(pid, int):
        return {
            "ok": False,
            "error": f"Invalid 'pid' type in {rpc_json_path}: expected int, got {type(pid).__name__}",
        }

    # Validate PID (check if process is still running)
    if not _is_process_running(pid):
        return {
            "ok": False,
            "error": (
                f"Stale port file detected: process {pid} is not running. "
                f"Remove {rpc_json_path} and restart UE5."
            ),
        }

    return {
        "ok": True,
        "port": port,
        "pid": pid,
        "started": started,
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
