"""
UE MCP Server - Thin MCP wrapper for ue-bridge.

This is a THIN transport layer with ZERO business logic.
All logic lives in ue-bridge CLI.

Architecture:
    MCP Client → MCP Server (this file) → ue-bridge CLI → UE RPC Server
                    [THIN]                  [LOGIC]         [LOGIC]
"""

import asyncio
import json
import sys
from pathlib import Path
from typing import Any, Sequence

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import TextContent, Tool

# Import bridge modules directly
# This ensures zero duplication and automatic updates
try:
    # Add bridge to path for direct import if needed
    bridge_path = Path(__file__).parent.parent.parent / "bridge"
    if str(bridge_path) not in sys.path:
        sys.path.insert(0, str(bridge_path))

    from ue_bridge.port_discovery import discover_port
    from ue_bridge.tcp_client import create_ue_client
    from ue_bridge.tools import TOOL_HANDLERS, TOOL_METADATA, TOOL_NAMES
except ImportError as e:
    print(f"ERROR: Cannot import ue_bridge: {e}", file=sys.stderr)
    print("Make sure bridge is installed: pip install -e ../bridge/", file=sys.stderr)
    sys.exit(1)


def start_server() -> None:
    """
    Start the MCP server on stdio.

    This function:
    1. Creates MCP server instance
    2. Auto-registers all tools from ue-bridge
    3. Starts stdio transport
    4. Delegates ALL logic to bridge
    """
    server = Server("ue-mcp")

    # Register list_tools handler
    @server.list_tools()  # type: ignore[no-untyped-call]
    async def list_tools() -> list[Tool]:
        """
        List all available tools from bridge - auto-discovered.

        Uses TOOL_METADATA for structured schema information.
        """
        return [
            Tool(
                name=meta["name"],
                description=meta["description"],
                inputSchema=meta["schema"],
            )
            for meta in TOOL_METADATA
        ]

    # Register single call_tool handler for ALL tools
    @server.call_tool()
    async def call_tool(name: str, arguments: dict[str, Any]) -> Sequence[TextContent]:
        """
        MCP tool wrapper - thin passthrough to bridge.

        This is the ONLY logic in MCP: serialize bridge response to MCP format.
        Everything else (port discovery, TCP, RPC) is in bridge.
        """
        handler = TOOL_HANDLERS.get(name)
        if not handler:
            return [TextContent(type="text", text=json.dumps({"ok": False, "error": f"Unknown tool: {name}"}))]

        try:
            # Discover port - bridge handles all the logic
            port_result = discover_port()
            if not port_result.get("ok"):
                error_msg = port_result.get("error", "Port discovery failed")
                error_json = json.dumps({"ok": False, "error": error_msg})
                return [TextContent(type="text", text=error_json)]

            # Create context
            client = create_ue_client(
                port=port_result["port"],
                host="127.0.0.1",
                timeout_ms=2000,
            )
            ctx = client["context"]

            # Call bridge handler - this is where all logic happens
            result = await handler(arguments or {}, ctx)

            # Return as MCP text content
            return [TextContent(type="text", text=json.dumps(result))]

        except Exception as e:
            error_response = {
                "ok": False,
                "error": str(e),
                "hint": "Ensure UE5 is running with UEServer plugin enabled",
            }
            return [TextContent(type="text", text=json.dumps(error_response))]

    print(f"[ue-mcp] Starting MCP server with {len(TOOL_NAMES)} tools", file=sys.stderr)
    print(f"[ue-mcp] Tools: {', '.join(TOOL_NAMES)}", file=sys.stderr)

    # Start stdio transport
    async def run_server() -> None:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream,
                write_stream,
                server.create_initialization_options(),
            )

    asyncio.run(run_server())
