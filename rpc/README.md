# UEServer RPC Plugin

TCP-based RPC server plugin for Unreal Engine 5 that exposes UI controls to external tools and AI agents.

## Features

- **Dynamic Port Allocation**: Binds to OS-assigned port (no conflicts)
- **Service Discovery**: Writes `.ueserver/rpc.json` with runtime state
- **TCP-based RPC**: JSON request/response protocol
- **Ping Operation**: Basic health check endpoint

## Installation

### Option 1: Project Plugin

1. Copy the `rpc/` directory to your UE5 project's `Plugins/` directory:
   ```bash
   cp -r rpc/ /path/to/YourProject/Plugins/UEServer/
   ```

2. Regenerate project files:
   ```bash
   # For .uproject files:
   right-click YourProject.uproject → "Generate Visual Studio project files"

   # Or use UE5 command line:
   /path/to/UE5/Engine/Build/BatchFiles/GenerateProjectFiles.sh YourProject.uproject
   ```

3. Open your project in UE5 and enable the plugin:
   - Edit → Plugins → Search "UEServer"
   - Check the "Enabled" box
   - Restart the editor

### Option 2: Engine Plugin (Global)

1. Copy to UE5 engine plugins directory:
   ```bash
   cp -r rpc/ /path/to/UE5/Engine/Plugins/UEServer/
   ```

2. Restart UE5 editor

## Usage

Once enabled, the plugin automatically:

1. Starts TCP server on module load (when UE5 launches)
2. Binds to `127.0.0.1:<dynamic-port>` (OS assigns available port)
3. Writes runtime state to `.ueserver/rpc.json` in your project directory:
   ```json
   {
     "port": 45231,
     "pid": 12345,
     "started": "2025-12-26T10:30:00Z"
   }
   ```
4. Accepts JSON RPC requests over TCP
5. Cleans up `.ueserver/rpc.json` on shutdown

### RPC Protocol

**Request Format:**
```json
{
  "id": "unique-request-id",
  "op": "operation_name",
  ...operation-specific params
}
```

**Response Format:**
```json
{
  "id": "unique-request-id",
  "op": "operation_name",
  "ok": true,
  ...operation-specific fields
}
```

**Error Response:**
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
{
  "id": "req-001",
  "op": "ping"
}
```

**Response:**
```json
{
  "id": "req-001",
  "op": "ping",
  "ok": true,
  "version": "0.1.0"
}
```

## Testing

### Manual Testing with netcat

1. Start UE5 with the plugin enabled
2. Check `.ueserver/rpc.json` for the assigned port:
   ```bash
   cat .ueserver/rpc.json
   # Output: {"port":45231,"pid":12345,"started":"2025-12-26T10:30:00Z"}
   ```
3. Send a ping request:
   ```bash
   echo '{"id":"test-1","op":"ping"}' | nc localhost 45231
   # Output: {"id":"test-1","op":"ping","ok":true,"version":"0.1.0"}
   ```

### Using the Bridge CLI

See `../bridge/README.md` for the Python bridge CLI that provides a higher-level interface.

## Architecture

```
UE5 Editor
    ├── UEServer Plugin (this)
    │   ├── TCP Server (port 0 → OS assigns dynamic port)
    │   ├── Write .ueserver/rpc.json
    │   └── Process JSON RPC requests
    │
    └── .ueserver/rpc.json
            ↓
        Bridge CLI reads port
            ↓
        External tools/MCP connect
```

## Configuration

Currently no configuration is required. The server:
- Always binds to `127.0.0.1` (localhost only, secure by default)
- Always uses dynamic port allocation (no conflicts)
- Always writes runtime state to `.ueserver/rpc.json`

Future versions may add:
- Custom port ranges
- TLS/authentication
- Additional RPC operations

## Requirements

- Unreal Engine 5.3 or later
- Platforms: Windows, Linux, macOS

## Logging

The plugin logs to the UE5 output log with the `LogTemp` category:
- Server startup/shutdown
- Port assignment
- Client connections
- RPC requests/responses
- Errors

Filter logs: `LogTemp` or search for "UEServerRPC"

## Troubleshooting

### Plugin doesn't show up in UE5
- Verify the plugin structure matches the required layout
- Check `UEServer.uplugin` is present and valid JSON
- Regenerate project files

### Server doesn't start
- Check UE5 output log for errors
- Ensure no firewall blocking localhost connections
- Verify `.ueserver/` directory is writable

### `.ueserver/rpc.json` not created
- Check UE5 output log for "Failed to write runtime state"
- Ensure project directory is writable
- Check disk space

### Connection refused from bridge
- Verify `.ueserver/rpc.json` exists and contains valid port
- Check UE5 is still running (PID matches)
- Ensure no firewall blocking localhost connections

## License

Copyright TwistedBrain. All Rights Reserved.

---

**Created by**: TwistedBrain and Claude Sonnet 4.5
**Version**: 0.1.0
**Last Updated**: 2025-12-26
