# UE Bridge

Python bridge to Unreal Engine TCP RPC server - Central hub for AI systems.

## Overview

UE Bridge is a Python CLI tool that provides programmatic access to Unreal Engine through the UEServer RPC plugin. It discovers the UE5 server port dynamically and exposes a simple command-line interface for AI tools and automation.

## Architecture

```
External Client/MCP → (stdio) → UE Bridge CLI → (TCP) → UE5 RPC Server
```

- **Transport**: TCP over localhost (127.0.0.1)
- **Protocol**: Line-based JSON (`{"id":"...", "op":"...", ...}\n`)
- **Port Discovery**: Dynamic via `.ueserver/rpc.json` in project directory

## Installation

### From Source (Development)

```bash
cd bridge/
pip install -e .
```

### For Production

```bash
cd bridge/
pip install .
```

## Usage

### Prerequisites

1. Start UE5 with UEServer plugin enabled
2. Run commands from your UE5 project directory (where `.ueserver/rpc.json` exists)

### CLI Mode

```bash
# Ping the UE server
ue-bridge ue.ping

# Output:
# {"tool": "ue.ping", "ok": true, "response": {"id": "cli-...", "op": "ping", "ok": true, "version": "0.1.0"}}
```

### Stdio Mode (for MCP integration)

```bash
# Single request
echo '{"tool":"ue.ping","args":{}}' | ue-bridge

# Interactive mode (reads stdin line-by-line)
ue-bridge < requests.jsonl
```

### Python API

```python
import asyncio
from ue_bridge.port_discovery import discover_port
from ue_bridge.tcp_client import call_ue

async def main():
    # Discover port
    discovery = discover_port()
    if not discovery["ok"]:
        print(f"Error: {discovery['error']}")
        return

    port = discovery["port"]

    # Create context
    ctx = {
        "host": "127.0.0.1",
        "port": port,
        "timeout_ms": 2000,
    }

    # Call UE server
    response = await call_ue("ping", {}, ctx)
    print(response)

asyncio.run(main())
```

## Available Tools

- `ue.ping` - Health check and version info

More tools will be added for UI inspection and control.

## Port Discovery

UE Bridge automatically discovers the RPC server port from `.ueserver/rpc.json`:

```json
{
  "port": 45231,
  "pid": 12345,
  "started": "2025-12-26T10:30:00Z"
}
```

This file is created by the UEServer plugin on startup and removed on shutdown.

### Troubleshooting

**"UE RPC server not running"**
- Start UE5 with UEServer plugin enabled
- Ensure you're running from the UE5 project directory

**"Stale port file detected"**
- The UE5 process crashed without cleaning up
- Remove `.ueserver/rpc.json` manually and restart UE5

**"Cannot reach UE RPC"**
- Check that UE5 is still running
- Verify the port in `.ueserver/rpc.json` matches the server

## Development

### Type Checking

```bash
mypy ue_bridge/
```

### Linting

```bash
ruff check ue_bridge/
```

### Testing

```bash
# Start UE5 with UEServer plugin first
pytest
```

## Protocol Reference

### Request Format

```json
{
  "id": "cli-abc123",
  "op": "ping"
}
```

### Response Format

```json
{
  "id": "cli-abc123",
  "op": "ping",
  "ok": true,
  "version": "0.1.0"
}
```

### Error Response

```json
{
  "id": "cli-abc123",
  "op": "unknown_op",
  "ok": false,
  "error": "Unknown operation: unknown_op"
}
```

## License

See top-level LICENSE file.
