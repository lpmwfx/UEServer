# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UEServer is a UE5-based RPC server that exposes Unreal Engine UI controls over TCP through a structured API. The architecture enables external tools to inspect, update, and automate UI behaviors without direct in-engine scripting.

**Core Motivation**: Provide AI agents with "eyes and hands" in the Unreal Editor. While UE5 offers native CLI for batch operations, it lacks runtime interactivity. UEServer transforms UE5 into an AI-interactive runtime environment where agents can:
- Inspect UI state in real-time (widget hierarchies, properties, visibility)
- Control UI programmatically (trigger actions, navigate menus, update settings)
- Integrate with MCP and other AI tools for conversational workflow automation

## Architecture

The system follows a three-tier architecture:

1. **UE5 RPC Server** (`rpc/`)
   - Hosts the Unreal Engine runtime
   - Owns the authoritative UI model and API contract
   - Exposes UI state and actions over TCP-based RPC

2. **Bridge CLI** (`bridge/`)
   - Stdio CLI application that communicates with the UE5 server
   - Single integration point for tools and protocols
   - Handles stdio/stderr for local CLI interactions and logging
   - Sends RPC requests over TCP to the UE5 server

3. **MCP Adapter** (`mcp/`)
   - Thin transport layer providing MCP access to the UE UI API
   - Uses the Bridge CLI (no direct RPC implementation)
   - Does not contain domain logic; delegates to the bridge

### Data Flow

```
External Client/MCP → (stdio) → Bridge CLI → (TCP) → UE5 RPC Server → UE UI Runtime
```

## Technology Stack

- **rpc/**: C++ (UE5 plugin/module)
- **bridge/**: Python with mypy (runs from UE5 project directory)
- **mcp/**: Python with mypy (MCP SDK integration)

## Port Discovery

The RPC server uses **dynamic port allocation** for multi-instance support:

1. RPC server binds to port 0 (OS assigns available port)
2. Writes runtime state to `.ueserver/rpc.json` in project directory
3. Bridge CLI reads `.ueserver/rpc.json` to discover the port
4. Cleanup: RPC removes file on shutdown; `.ueserver/` is gitignored

This allows multiple UE5 projects to run simultaneously without port conflicts.

## Related Projects

**BlenderServer** (`/home/lpm/REPO/BlenderServer`) is a sister project with the same architecture for Blender automation. Reference it for:
- Python bridge/MCP patterns and structure
- CLI tool design and stdio handling
- Type definitions and mypy usage
- Testing strategies

Key difference: BlenderServer uses WebSocket (fixed port via env var); UEServer uses TCP (dynamic port via `.ueserver/rpc.json`).

## Design Principles

- **Centralized Logic**: Domain logic lives in the UE5 RPC server and Bridge CLI
- **Thin MCP Layer**: MCP adapter is transport-focused; no duplicate business logic
- **Stable Contracts**: Use explicit API contracts to avoid UI/engine coupling
- **Automation-Friendly**: Clear logging and deterministic responses

## Development Status

This is an early-stage project. The directory structure is in place but implementation is pending. When developing:

- Maintain separation of concerns between the three components
- Keep the Bridge CLI as the sole integration point
- Ensure the MCP adapter remains thin and delegates to the bridge
- Design RPC APIs to be stable and explicit
