"""
UE MCP Server - Thin MCP wrapper for ue-bridge.

This is a THIN transport layer with ZERO business logic.
All logic lives in ue-bridge CLI.

Architecture:
    MCP Client → MCP Server (this file) → ue-bridge CLI → UE RPC Server
                    [THIN]                  [LOGIC]         [LOGIC]
"""

import sys
from typing import Any

from mcp import Server
from mcp.server.stdio import stdio_server

# Import tool metadata directly from bridge
# This ensures zero duplication and automatic updates
try:
    # Add bridge to path for direct import
    sys.path.insert(0, str(__file__).rsplit("/", 3)[0] + "/bridge")
    from ue_bridge.tools import TOOL_HANDLERS, TOOL_NAMES
except ImportError as e:
    print(f"ERROR: Cannot import ue_bridge: {e}", file=sys.stderr)
    print("Make sure bridge is installed: pip install -e bridge/", file=sys.stderr)
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

    # Auto-register all tools from bridge
    # Tools are thin wrappers - bridge does all the work
    for tool_name in TOOL_NAMES:
        register_tool(server, tool_name)

    print(f"[ue-mcp] Starting MCP server with {len(TOOL_NAMES)} tools", file=sys.stderr)
    print(f"[ue-mcp] Tools: {', '.join(TOOL_NAMES)}", file=sys.stderr)

    # Start stdio server
    stdio_server(server)


def register_tool(server: Server, tool_name: str) -> None:
    """
    Register a single tool as a thin MCP wrapper.

    Args:
        server: MCP server instance
        tool_name: Tool name from bridge (e.g., 'ue.ping')

    The wrapper:
    - Gets handler from bridge
    - Calls it directly
    - Returns result as MCP text content
    - NO parsing, NO logic, just forwarding
    """
    handler = TOOL_HANDLERS.get(tool_name)

    if not handler:
        print(f"[ue-mcp] WARNING: No handler for tool '{tool_name}'", file=sys.stderr)
        return

    # Create tool description from docstring
    description = handler.__doc__.strip().split("\n")[0] if handler.__doc__ else tool_name

    @server.call_tool()
    async def call_tool(name: str, arguments: dict[str, Any]) -> list[Any]:
        """
        MCP tool wrapper - thin passthrough to bridge.

        This is the ONLY logic in MCP: serialize bridge response to MCP format.
        Everything else (port discovery, TCP, RPC) is in bridge.
        """
        if name != tool_name:
            return [{"type": "text", "text": f"Unknown tool: {name}"}]

        try:
            # Call bridge handler directly - this is where all the logic is
            # We need a context, but bridge CLI mode handles port discovery
            # For now, use empty context - bridge will handle it
            from ue_bridge.port_discovery import discover_port
            from ue_bridge.tcp_client import create_ue_client

            # Discover port
            port_result = discover_port()
            if not port_result.get("ok"):
                error_msg = port_result.get("error", "Port discovery failed")
                return [
                    {
                        "type": "text",
                        "text": f"{{\"ok\": false, \"error\": \"{error_msg}\"}}",
                    }
                ]

            # Create context
            client = create_ue_client(
                port=port_result["port"],
                host="127.0.0.1",
                timeout_ms=2000,
            )
            ctx = client["context"]

            # Call handler
            result = await handler(arguments or {}, ctx)

            # Return as MCP text content
            import json

            return [{"type": "text", "text": json.dumps(result)}]

        except Exception as e:
            import json

            error_response = {
                "ok": False,
                "error": str(e),
                "hint": "Ensure UE5 is running with UEServer plugin enabled",
            }
            return [{"type": "text", "text": json.dumps(error_response)}]
