"""TCP RPC client for UE server communication."""

import asyncio
import json
import socket
import uuid
from typing import Any, cast

from .types import ToolContext, UEResponse


def create_ue_client(
    port: int | None = None,
    host: str | None = None,
    timeout_ms: int | None = None,
) -> dict[str, Any]:
    """
    Create UE client with context.

    Args:
        port: TCP port (discovered from .ueserver/rpc.json if not provided)
        host: Host address (default: 127.0.0.1)
        timeout_ms: Request timeout in milliseconds (default: 2000)

    Returns:
        Dict with 'context' key containing ToolContext
    """
    # Note: port is required, but we allow None here for API consistency
    # Caller should discover port using port_discovery.discover_port()
    if port is None:
        raise ValueError("Port is required. Use port_discovery.discover_port() first.")

    context: ToolContext = {
        "port": port,
        "host": host or "127.0.0.1",
        "timeout_ms": timeout_ms or 2000,
    }

    return {"context": context}


async def call_ue(
    op: str,
    params: dict[str, Any],
    ctx: ToolContext,
) -> UEResponse:
    """
    Call UE RPC server over TCP.

    Args:
        op: Operation name (e.g., 'ping')
        params: Operation parameters
        ctx: Tool context with host, port, timeout

    Returns:
        Response dictionary from UE server

    Raises:
        asyncio.TimeoutError: If request times out
        ConnectionError: If TCP connection fails
        ValueError: If server returns invalid JSON

    Protocol:
        Request:  {"id": "req-001", "op": "ping", ...params}
        Response: {"id": "req-001", "op": "ping", "ok": true, ...fields}
    """
    request_id = ctx.get("request_id") or _generate_request_id()

    # Build payload
    payload: dict[str, Any] = {
        "id": request_id,
        "op": op,
        **params,
    }

    # Convert to JSON and add newline (line-based protocol)
    request_json = json.dumps(payload) + "\n"
    request_bytes = request_json.encode("utf-8")

    # Call with timeout
    timeout_sec = ctx["timeout_ms"] / 1000

    try:
        # Create TCP connection
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(ctx["host"], ctx["port"]),
            timeout=timeout_sec,
        )

        try:
            # Send request
            writer.write(request_bytes)
            await writer.drain()

            # Receive response (read until newline)
            response_bytes = await asyncio.wait_for(
                reader.readline(),
                timeout=timeout_sec,
            )

            response_data = response_bytes.decode("utf-8").strip()

            # Parse JSON
            try:
                return cast(UEResponse, json.loads(response_data))
            except json.JSONDecodeError as err:
                raise ValueError(f"Invalid JSON from UE server: {err}") from err

        finally:
            # Clean up connection
            writer.close()
            await writer.wait_closed()

    except asyncio.TimeoutError as err:
        raise TimeoutError(
            f"Timeout after {timeout_sec}s contacting UE RPC at {ctx['host']}:{ctx['port']}. "
            f"Ensure UE5 is running with UEServer plugin enabled."
        ) from err

    except (ConnectionRefusedError, OSError) as err:
        raise ConnectionError(
            f"Cannot reach UE RPC at {ctx['host']}:{ctx['port']}: {err}. "
            f"Ensure UE5 server is running."
        ) from err


def _generate_request_id() -> str:
    """Generate unique request ID."""
    return f"cli-{uuid.uuid4().hex[:12]}"
