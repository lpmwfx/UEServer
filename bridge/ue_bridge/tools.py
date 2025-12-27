"""Tool handlers for UE Bridge."""

import asyncio
import os
import subprocess
import time
from pathlib import Path
from typing import Any, cast

from .tcp_client import call_ue
from .types import ToolContext, ToolHandler, UEResponse


async def ue_start(args: dict[str, Any], ctx: ToolContext) -> UEResponse:
    """
    Start UE5 with UEServer plugin and wait for RPC server to be ready.

    Args:
        args: {
            "project_path": Optional path to .uproject file (default: auto-detect from CWD),
            "editor_path": Optional path to UnrealEditor executable (default: auto-detect),
            "wait_timeout": Timeout in seconds to wait for server (default: 30)
        }
        ctx: Tool context (not used for startup)

    Returns:
        Response with server info:
            {"ok": true, "port": 12345, "pid": 67890, "project": "..."}
    """
    # Get paths
    project_path = args.get("project_path")
    editor_path = args.get("editor_path")
    wait_timeout = args.get("wait_timeout", 30)

    # Auto-detect project path from CWD
    if not project_path:
        cwd = Path.cwd()
        uproject_files = list(cwd.glob("*.uproject"))
        if not uproject_files:
            return {
                "ok": False,
                "error": "No .uproject file found in current directory. Run from project directory or provide project_path.",
            }
        project_path = str(uproject_files[0])

    # Auto-detect editor path
    if not editor_path:
        # Try known location
        default_editor = Path("/home/lpm/PROD/UnrealEngine-5.7.1/Engine/Binaries/Linux/UnrealEditor")
        if default_editor.exists():
            editor_path = str(default_editor)
        else:
            return {
                "ok": False,
                "error": f"UnrealEditor not found at {default_editor}. Provide editor_path.",
            }

    # Verify paths exist
    if not Path(project_path).exists():
        return {"ok": False, "error": f"Project file not found: {project_path}"}

    if not Path(editor_path).exists():
        return {"ok": False, "error": f"UnrealEditor not found: {editor_path}"}

    # Get switchboard path
    home = Path.home()
    switchboard_path = home / ".ueserver" / "switchboard.json"

    # Start UE5 in background
    try:
        # Launch UE5
        process = subprocess.Popen(
            [editor_path, project_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,  # Detach from parent
        )

        # Wait for switchboard file to be created with valid entry
        start_time = time.time()
        while time.time() - start_time < wait_timeout:
            await asyncio.sleep(0.5)

            if not switchboard_path.exists():
                continue

            # Read switchboard
            try:
                import json

                with open(switchboard_path) as f:
                    data = json.load(f)

                instances = data.get("instances", [])
                if instances:
                    # Return first live instance
                    instance = instances[0]
                    return {
                        "ok": True,
                        "port": instance["port"],
                        "pid": instance["pid"],
                        "project": instance.get("project"),
                        "started": instance.get("started"),
                        "message": "UE5 started successfully",
                    }
            except (json.JSONDecodeError, OSError, KeyError):
                continue

        # Timeout - UE5 may still be starting but we give up waiting
        return {
            "ok": False,
            "error": f"Timeout waiting for UE5 to start (waited {wait_timeout}s). Check if UEServer plugin is enabled.",
            "hint": "UE5 process may still be launching. Check manually with: cat ~/.ueserver/switchboard.json",
        }

    except Exception as e:
        return {
            "ok": False,
            "error": f"Failed to start UE5: {str(e)}",
        }


async def ue_ping(args: dict[str, Any], ctx: ToolContext) -> UEResponse:
    """
    Ping UE RPC server to check connectivity.

    Args:
        args: Empty dict (no arguments required)
        ctx: Tool context with host, port, timeout

    Returns:
        Response from UE server:
            {"id": "...", "op": "ping", "ok": true, "version": "0.1.0"}
    """
    return await call_ue("ping", {}, ctx)


async def ue_health(args: dict[str, Any], ctx: ToolContext) -> UEResponse:
    """
    Quick health check with short timeout (500ms).

    Use this before longer operations to verify server is responsive.

    Args:
        args: Empty dict (no arguments required)
        ctx: Tool context (timeout is overridden to 500ms)

    Returns:
        Response from UE server or error if unreachable:
            {"ok": true, "status": "healthy"} or {"ok": false, "error": "..."}
    """
    # Override timeout to 500ms for quick health check
    health_ctx = cast(ToolContext, {**ctx, "timeout_ms": 500})

    try:
        response = await call_ue("ping", {}, health_ctx)
        return {
            "ok": True,
            "status": "healthy",
            "server_response": response,
        }
    except (TimeoutError, ConnectionError) as err:
        return {
            "ok": False,
            "status": "unreachable",
            "error": str(err),
        }


async def ue_ui_get_tree(args: dict[str, Any], ctx: ToolContext) -> UEResponse:
    """
    Get the Slate UI widget tree from Unreal Editor.

    Returns hierarchical structure of all visible UI windows and widgets.

    Args:
        args: {
            "max_depth": Optional maximum recursion depth (default: 10)
        }
        ctx: Tool context with host, port, timeout

    Returns:
        Response with UI tree:
            {
                "ok": true,
                "windows": [...array of window objects...],
                "window_count": N
            }

        Each widget object contains:
            {
                "type": "SWindow",
                "visible": true,
                "enabled": true,
                "geometry": {"x": 100, "y": 50, "width": 800, "height": 600},
                "text": "Optional text content",
                "children": [...nested widgets...],
                "child_count": N
            }
    """
    max_depth = args.get("max_depth", 10)
    return await call_ue("ui.get_tree", {"max_depth": max_depth}, ctx)


# Tool registry
TOOL_HANDLERS: dict[str, ToolHandler] = {
    "ue.start": ue_start,
    "ue.ping": ue_ping,
    "ue.health": ue_health,
    "ue.ui.get_tree": ue_ui_get_tree,
}

TOOL_NAMES = sorted(TOOL_HANDLERS.keys())
