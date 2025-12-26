"""Tool handlers for UE Bridge."""

from typing import Any

from .tcp_client import call_ue
from .types import ToolContext, ToolHandler, UEResponse


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


# Tool registry
TOOL_HANDLERS: dict[str, ToolHandler] = {
    "ue.ping": ue_ping,
}

TOOL_NAMES = sorted(TOOL_HANDLERS.keys())
