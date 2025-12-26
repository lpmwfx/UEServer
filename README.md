# UEServer

TCP-based RPC server for Unreal Engine 5 that exposes UI controls to external tools and AI agents.

## Overview

UEServer transforms UE5 from a batch-scriptable tool into an **AI-interactive runtime environment**. It provides AI agents with "eyes and hands" in the Unreal Editor through a structured RPC API.

### Motivation: AI UI/UX

While UE5 offers native CLI capabilities for batch operations (building, cooking, running Python scripts), these are one-way, non-interactive processes. AI agents need:

- **Real-time UI inspection**: Query widget hierarchies, read UI state, inspect properties
- **Interactive control**: Trigger UI actions, navigate menus, update settings
- **Bidirectional feedback**: Send commands and receive structured responses
- **Persistent connection**: Maintain a session with a running UE5 instance

### Use Cases

- **MCP Integration**: Claude Code and other AI tools can interact with UE5 projects through the Model Context Protocol
- **Automated Testing**: AI agents test UI flows, validate UX patterns, and verify widget behavior
- **Workflow Automation**: Repetitive editor tasks become scriptable through conversational AI
- **UX Analysis**: AI inspects and provides feedback on UI complexity, accessibility, and usability
- **Interactive Debugging**: Real-time UI state inspection without manual navigation

## Architecture

Three-tier architecture:

```
External Client / MCP
        |
     (stdio)
        |
   Bridge CLI
        |
      (TCP)
        |
 UE5 RPC Server
        |
   UE UI Runtime
```

### Components

1. **UE5 RPC Server** (`rpc/`)
   - C++ plugin running in UE5 runtime
   - Exposes UI state and actions over TCP-based RPC
   - Owns the authoritative UI model and API contract

2. **Bridge CLI** (`bridge/`)
   - Python stdio CLI application
   - Communicates with UE5 server over TCP
   - Single integration point for tools and protocols

3. **MCP Adapter** (`mcp/`)
   - Python MCP wrapper using the Bridge CLI
   - Thin transport layer (no domain logic)
   - Delegates all operations to the bridge

## Port Discovery & Multi-Instance Support

UEServer uses **dynamic port allocation** to allow multiple UE5 instances to run simultaneously:

1. **RPC Server** binds to port 0 (OS assigns available port)
2. Writes runtime state to `.ueserver/rpc.json`:
   ```json
   {
     "port": 45231,
     "pid": 12345,
     "started": "2025-12-26T10:30:00Z"
   }
   ```
3. **Bridge CLI** reads `.ueserver/rpc.json` to discover the port
4. Cleanup: RPC server removes file on shutdown

## Quick Start

### 1. Install RPC Server Plugin

```bash
# Copy plugin to your UE5 project
cp -r rpc/ /path/to/YourProject/Plugins/UEServer/

# Or install globally to UE5 engine
cp -r rpc/ /path/to/UE5/Engine/Plugins/UEServer/
```

Enable the plugin in your project:
- Edit → Plugins → Search "UEServer"
- Check "Enabled" and restart

See `rpc/README.md` for detailed installation instructions.

### 2. Test RPC Server

Start UE5 and verify the plugin is running:

```bash
cd /path/to/YourProject

# Check runtime state file
cat .ueserver/rpc.json
# Output: {"port":45231,"pid":12345,"started":"2025-12-26T10:30:00Z"}

# Test ping with netcat
PORT=$(cat .ueserver/rpc.json | grep -o '"port":[0-9]*' | grep -o '[0-9]*')
echo '{"id":"test-1","op":"ping"}' | nc localhost $PORT
# Output: {"id":"test-1","op":"ping","ok":true,"version":"0.1.0"}
```

See `TESTING.md` for comprehensive testing guide.

### 3. Install Bridge CLI (Coming Soon)

```bash
cd bridge/
pip install -e .

# Run ping command
bridge-cli ue.ping
```

See `bridge/README.md` for bridge documentation.

### 4. Install MCP Adapter (Coming Soon)

```bash
cd mcp/
pip install -e .

# Start MCP server
python -m ue_mcp
```

See `mcp/README.md` for MCP integration guide.

## RPC Protocol

### Request Format
```json
{
  "id": "unique-request-id",
  "op": "operation_name",
  ...operation-specific params
}
```

### Response Format
```json
{
  "id": "unique-request-id",
  "op": "operation_name",
  "ok": true,
  ...operation-specific fields
}
```

### Error Response
```json
{
  "id": "unique-request-id",
  "op": "operation_name",
  "ok": false,
  "error": "error message"
}
```

### Current Operations

#### ping
Health check endpoint.

**Request:**
```json
{"id":"req-001","op":"ping"}
```

**Response:**
```json
{"id":"req-001","op":"ping","ok":true,"version":"0.1.0"}
```

More operations coming soon (list widgets, trigger actions, inspect properties).

## Technology Stack

- **rpc/**: C++ (required by UE5 plugin architecture)
- **bridge/**: Python with mypy (type-safe CLI development)
- **mcp/**: Python with mypy (MCP SDK integration)

## Related Projects

**BlenderServer** (`/home/lpm/REPO/BlenderServer`) is a sister project with the same three-tier architecture for Blender automation. It serves as a reference implementation for the bridge and MCP components.

**Key Similarities:**
- Three-tier architecture (server → bridge → MCP)
- Bridge as central integration point
- Thin MCP adapter that delegates to bridge
- Python with mypy for type safety in CLI/MCP layers

**Key Differences:**
- BlenderServer uses WebSocket protocol; UEServer uses TCP
- BlenderServer uses fixed port (env var); UEServer uses dynamic ports
- BlenderServer is Python throughout; UEServer uses C++ for the engine-side server

## Design Principles

- **Centralized Logic**: Domain logic lives in the UE5 RPC server and Bridge CLI
- **Thin MCP Layer**: MCP adapter is transport-focused; no duplicate business logic
- **Stable Contracts**: Use explicit API contracts to avoid UI/engine coupling
- **Automation-Friendly**: Clear logging and deterministic responses

## Current Status

### Completed ✅
- **RPC Server (C++ plugin)**
  - Dynamic port allocation and service discovery
  - TCP server with JSON RPC protocol
  - Ping operation
  - Runtime state management (.ueserver/rpc.json)
  - Comprehensive documentation

- **Python Bridge CLI**
  - Port discovery from .ueserver/rpc.json
  - TCP client with async I/O
  - CLI interface (`ue-bridge ue.ping`)
  - Stdio mode for MCP integration
  - Full type safety (mypy strict)
  - Error handling for all failure modes

### Ready for Testing 🧪
- End-to-end integration (RPC + Bridge)
- See [QUICKSTART.md](QUICKSTART.md) for testing guide

### Planned 📋
- MCP Adapter implementation
- Additional RPC operations (UI inspection, control)
- Claude Code MCP integration

See `TODO` for detailed task breakdown.

## Requirements

- Unreal Engine 5.3 or later
- Python 3.11+ (for bridge and MCP)
- Platforms: Windows, Linux, macOS

## Documentation

- [QUICKSTART.md](QUICKSTART.md) - Quick start testing guide
- [CLAUDE.md](CLAUDE.md) - Guidance for Claude Code development
- [doc/project-description.md](doc/project-description.md) - Detailed architecture and design
- [rpc/README.md](rpc/README.md) - RPC server plugin documentation
- [bridge/README.md](bridge/README.md) - Bridge CLI documentation
- [TESTING.md](TESTING.md) - Manual and automated testing guide
- [TODO](TODO) - Current phase task breakdown
- [CHANGELOG](CHANGELOG) - Development history

## Security

- Server always binds to localhost (127.0.0.1) only
- No external network exposure by default
- For remote access, use SSH tunneling (not public binding)
- Future: TLS and authentication support planned

## Contributing

This is an early-stage project under active development. See `TODO` for current priorities.

## License

Copyright TwistedBrain. All Rights Reserved.

---

**Created by**: TwistedBrain and Claude Sonnet 4.5
**Version**: 0.1.0
**Last Updated**: 2025-12-26
