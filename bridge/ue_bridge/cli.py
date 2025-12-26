"""CLI interface for UE Bridge - stdio and command-line modes."""

import asyncio
import json
import sys
from typing import Any

from .port_discovery import discover_port
from .tools import TOOL_HANDLERS, TOOL_NAMES
from .types import ToolContext, UEResponse


def _is_ok(resp: UEResponse) -> bool:
    """Check if response indicates success."""
    return resp.get("ok", True) is True


async def invoke_tool(
    tool_name: str | None,
    args: dict[str, Any],
    ctx: ToolContext,
) -> dict[str, Any]:
    """
    Invoke tool handler.

    Args:
        tool_name: Tool name (e.g., 'ue.ping')
        args: Tool arguments
        ctx: Execution context

    Returns:
        JSON response with id, tool, ok, error, response fields
    """
    if not tool_name:
        return {
            "ok": False,
            "error": "missing tool",
            "available_tools": TOOL_NAMES,
        }

    handler = TOOL_HANDLERS.get(tool_name)
    if not handler:
        return {
            "tool": tool_name,
            "ok": False,
            "error": f"unknown tool: {tool_name}",
            "available_tools": TOOL_NAMES,
        }

    try:
        resp = await handler(args, ctx)
        ok = _is_ok(resp)
        return {
            "tool": tool_name,
            "ok": ok,
            "error": resp.get("error") if not ok else None,
            "response": resp,
        }
    except Exception as err:
        return {
            "tool": tool_name,
            "ok": False,
            "error": str(err),
        }


async def stdio_loop(ctx: ToolContext) -> None:
    """
    Read JSON-RPC requests from stdin, write responses to stdout.

    Format:
        Input: {"tool": "ue.ping", "args": {}}
        Output: {"tool": "ue.ping", "ok": true, "response": {...}}
    """
    buffer = ""

    for line in sys.stdin:
        buffer += line

        # Process complete lines
        while "\n" in buffer:
            line_text, buffer = buffer.split("\n", 1)
            line_text = line_text.strip()

            if not line_text:
                continue

            # Parse JSON
            try:
                invocation = json.loads(line_text)
            except json.JSONDecodeError:
                _write_response({"ok": False, "error": "invalid JSON input"})
                continue

            # Invoke tool
            result = await invoke_tool(
                invocation.get("tool"),
                invocation.get("args", {}),
                ctx,
            )
            _write_response(result)


async def cli_once(args: list[str], ctx: ToolContext) -> None:
    """
    Run single CLI command.

    Examples:
        ue-bridge ue.ping
    """
    if not args:
        _write_response({"ok": False, "error": "no tool specified"})
        return

    tool_name = args[0]
    parsed_args = _parse_cli_args(args[1:])

    result = await invoke_tool(tool_name, parsed_args, ctx)
    _write_response(result)


def _parse_cli_args(argv: list[str]) -> dict[str, Any]:
    """
    Parse CLI arguments: key=value → {"key": value}.

    Examples:
        name=Cube → {"name": "Cube"}
        enabled=true → {"enabled": True}
    """
    result: dict[str, Any] = {}

    for entry in argv:
        if "=" not in entry:
            continue

        key, _, raw_val = entry.partition("=")
        result[key] = _parse_arg_value(raw_val)

    return result


def _parse_arg_value(val: str) -> Any:
    """
    Parse CLI value - auto-detect type.

    Examples:
        "true" → True
        "false" → False
        "42" → 42
        "3.14" → 3.14
        "hello" → "hello"
    """
    if val == "true":
        return True
    if val == "false":
        return False

    # Try int
    try:
        return int(val)
    except ValueError:
        pass

    # Try float
    try:
        return float(val)
    except ValueError:
        pass

    # String
    return val


def _write_response(obj: dict[str, Any]) -> None:
    """Write JSON response to stdout."""
    print(json.dumps(obj), flush=True)


def _print_help() -> None:
    """Print concise CLI help."""
    help_text = """ue-bridge <tool> [key=value ...] [--timeout=MS]

Examples:
  ue-bridge ue.ping
  ue-bridge ue.health
  ue-bridge ue.ping --timeout=5000

Options:
  --timeout=MS    Request timeout in milliseconds (default: 2000)

Notes:
  - UE Bridge discovers port from ~/.ueserver/switchboard.json
  - Start UE5 with UEServer plugin enabled first
  - Use ue-bridge from your UE5 project directory
  - ue.health uses 500ms timeout for quick checks
"""
    print(help_text.strip())


def create_context(timeout_ms: int = 2000) -> ToolContext:
    """
    Create ToolContext by discovering port from .ueserver/rpc.json

    Args:
        timeout_ms: Request timeout in milliseconds (default: 2000)

    Returns:
        ToolContext with host, port, timeout

    Raises:
        SystemExit: If port discovery fails
    """
    discovery = discover_port()

    if not discovery.get("ok"):
        error_msg = discovery.get("error", "Unknown port discovery error")
        print(json.dumps({"ok": False, "error": error_msg}), file=sys.stderr, flush=True)
        sys.exit(1)

    port = discovery.get("port")
    if port is None:
        print(
            json.dumps({"ok": False, "error": "Port discovery succeeded but no port returned"}),
            file=sys.stderr,
            flush=True,
        )
        sys.exit(1)

    context: ToolContext = {
        "host": "127.0.0.1",
        "port": port,
        "timeout_ms": timeout_ms,
    }

    return context


async def async_main() -> None:
    """Async main entry point."""
    if len(sys.argv) > 1:
        # CLI mode: ue-bridge ue.ping
        if sys.argv[1] in ("-h", "--help", "help"):
            _print_help()
            return

        # Parse timeout from args
        timeout_ms = 2000
        args = sys.argv[1:]
        filtered_args = []

        for arg in args:
            if arg.startswith("--timeout="):
                try:
                    timeout_ms = int(arg.split("=", 1)[1])
                except (ValueError, IndexError):
                    print(
                        json.dumps({"ok": False, "error": "Invalid --timeout value"}),
                        file=sys.stderr,
                        flush=True,
                    )
                    sys.exit(1)
            else:
                filtered_args.append(arg)

        # Discover port first
        ctx = create_context(timeout_ms=timeout_ms)
        await cli_once(filtered_args, ctx)
    else:
        # If running in a TTY with no args, show help.
        if sys.stdin.isatty():
            _print_help()
            return

        # Stdio mode: echo '{"tool":"ue.ping"}' | ue-bridge
        ctx = create_context()
        await stdio_loop(ctx)


def main() -> None:
    """Main entry point."""
    try:
        asyncio.run(async_main())
    except KeyboardInterrupt:
        pass
    except Exception as err:
        print(json.dumps({"ok": False, "error": str(err)}), flush=True, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
