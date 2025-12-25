# Project Description

This repository defines a UE5-based RPC server that exposes Unreal Engine UI
controls over TCP through a structured API. The goal is to make UE UI elements
remote-controllable so external tools can inspect, update, and automate UI
behaviors without direct in-engine scripting.

## Motivation: AI UI/UX

The primary motivation for UEServer is to provide **AI agents with programmatic access to Unreal Engine's UI**. While UE5 offers native CLI capabilities for batch operations (building, cooking, running Python scripts), these are one-way, non-interactive processes. AI agents need:

- **Real-time UI inspection**: Query widget hierarchies, read UI state, inspect properties
- **Interactive control**: Trigger UI actions, navigate menus, update settings
- **Bidirectional feedback**: Send commands and receive structured responses
- **Persistent connection**: Maintain a session with a running UE5 instance

### AI UI/UX Capabilities

UEServer gives AI agents "eyes and hands" in the Unreal Editor:

**Eyes (Inspection)**:
- Read widget hierarchies and UI state
- Query component properties and visibility
- Inspect menu structures and available actions
- Monitor UI changes and events

**Hands (Control)**:
- Trigger button clicks and menu navigation
- Update settings and properties
- Automate repetitive workflows
- Execute UI-driven operations

### Use Cases

- **MCP Integration**: Claude Code and other AI tools can interact with UE5 projects through the Model Context Protocol
- **Automated Testing**: AI agents test UI flows, validate UX patterns, and verify widget behavior
- **Workflow Automation**: Repetitive editor tasks become scriptable through conversational AI
- **UX Analysis**: AI inspects and provides feedback on UI complexity, accessibility, and usability
- **Interactive Debugging**: Real-time UI state inspection without manual navigation

### Comparison to Native CLI

| Capability | Native UE5 CLI | UEServer RPC |
|------------|----------------|--------------|
| Batch operations | ✓ (UAT, Commandlets) | ✓ |
| Python scripting | ✓ (Editor scripts) | ✓ |
| Runtime interactivity | ✗ (fire-and-forget) | ✓ |
| UI state inspection | ✗ | ✓ |
| External tool integration | ✗ | ✓ (MCP, APIs) |
| Bidirectional communication | ✗ | ✓ |
| Persistent connection | ✗ | ✓ |

UEServer transforms UE5 from a batch-scriptable tool into an **AI-interactive runtime environment**.

## Components

- UE5 RPC Server (`rpc/`)
  - Hosts the Unreal Engine runtime.
  - Exposes UI state and actions over a TCP-based RPC API.
  - Owns the authoritative UI model and the API contract.

- Bridge CLI (`bridge/`)
  - A stdio CLI application that speaks RPC with the UE5 server.
  - Acts as the single integration point for tools and protocols.
  - Handles stdio/stderr for local CLI interactions and logging.

- MCP Adapter (`mcp/`)
  - Uses the Bridge CLI to provide MCP access to the UE UI API.
  - Does not implement domain logic itself; delegates to the bridge.

## Technology Stack

- **UE5 RPC Server** (`rpc/`): C++
  - Required by Unreal Engine 5
  - Implements UE5 plugin/module running in engine runtime
  - Handles performance-critical UI operations

- **Bridge CLI** (`bridge/`): Python with mypy
  - Type-safe Python for robust CLI development
  - Strong stdio/stderr and TCP socket support
  - Fast iteration for protocol handling
  - Runs from UE5 project directory (uses CWD for project context)

- **MCP Adapter** (`mcp/`): Python with mypy
  - Python MCP SDK integration
  - Subprocess management for Bridge CLI communication
  - Consistent type safety with mypy across the stack

## Port Discovery & Multi-Instance Support

The RPC server uses **dynamic port allocation** to allow multiple UE5 instances to run simultaneously without port conflicts.

### Mechanism

1. **RPC Server Startup**:
   - Binds to port 0 (OS assigns an available port)
   - Writes runtime state to `.ueserver/rpc.json` in the project directory:
     ```json
     {
       "port": 45231,
       "pid": 12345,
       "started": "2025-12-25T23:45:00Z"
     }
     ```

2. **Bridge Discovery**:
   - Bridge CLI runs from the UE5 project directory
   - Reads `.ueserver/rpc.json` to discover the RPC server's port
   - Connects to `localhost:<port>` for that specific project instance

3. **Cleanup**:
   - RPC server removes `.ueserver/rpc.json` on clean shutdown
   - Bridge can validate the PID to detect stale port files
   - `.ueserver/` is gitignored (runtime state only)

### Benefits

- Multiple UE5 projects can run simultaneously without conflicts
- Bridge automatically discovers the correct server instance
- Works for both CLI-launched and GUI-launched UE5 instances
- Simple file-based discovery (no additional services required)

## Data Flow

1. External client or MCP requests UI data or actions.
2. The MCP adapter invokes the Bridge CLI over stdio.
3. The Bridge CLI sends RPC requests over TCP to the UE5 server.
4. The UE5 server executes UI operations and returns responses.
5. The Bridge CLI forwards results back to the MCP or CLI caller.

## Architecture Diagram

```text
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

## Related Projects

### BlenderServer

UEServer is modeled after **BlenderServer** (`/home/lpm/REPO/BlenderServer`), a sister project that provides the same three-tier architecture for Blender automation:

- **blender-server/**: Python WebSocket RPC server running inside Blender
- **blender-bridge/**: Python stdio CLI bridge (central tool hub)
- **blender-mcp/**: Python MCP wrapper using the bridge

**Key Similarities**:
- Three-tier architecture (server → bridge → MCP)
- Bridge as central integration point
- Thin MCP adapter that delegates to bridge
- Python with mypy for type safety in CLI/MCP layers

**Key Differences**:
- BlenderServer uses WebSocket protocol; UEServer uses TCP
- BlenderServer currently uses fixed port (env var); UEServer improves with dynamic ports
- BlenderServer is Python throughout; UEServer uses C++ for the engine-side server

**When to Reference BlenderServer**:
- Python bridge/MCP patterns and structure
- CLI tool design and stdio handling
- Type definitions and validation patterns (mypy usage)
- Testing strategies for bridge/MCP integration

The BlenderServer codebase serves as a proven reference implementation for the bridge and MCP components.

## Design Principles

- Centralize domain logic in the UE5 RPC server and Bridge CLI.
- Keep MCP thin and transport-focused; no duplicate business logic.
- Use stable, explicit API contracts to avoid UI/engine coupling.
- Favor clear logging and deterministic responses for automation.
