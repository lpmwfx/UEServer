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
    health_ctx = {**ctx, "timeout_ms": 500}

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


# Tool registry
TOOL_HANDLERS: dict[str, ToolHandler] = {
    "ue.ping": ue_ping,
    "ue.health": ue_health,
}

TOOL_NAMES = sorted(TOOL_HANDLERS.keys())
