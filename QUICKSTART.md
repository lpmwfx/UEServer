# Quick Start Guide

This guide walks you through testing the UEServer RPC + Bridge MVP.

## Prerequisites

- Unreal Engine 5.3+ installed
- Python 3.11+
- UEServer plugin installed in your UE5 project (e.g., UETRAIL)

## Step 1: Install Bridge CLI

```bash
cd /home/lpm/REPO/UEServer/bridge
pip install -e .
```

Verify installation:

```bash
python3 -m ue_bridge --help
```

Expected output:
```
ue-bridge <tool> [key=value ...]

Examples:
  ue-bridge ue.ping
...
```

## Step 2: Start UE5 with UEServer Plugin

1. Open your UE5 project (e.g., UETRAIL)
2. Ensure UEServer plugin is enabled (Edit → Plugins → Search "UEServer")
3. Start the editor

The plugin will automatically:
- Start TCP server on a dynamic port
- Create `.ueserver/rpc.json` in your project directory with port info

## Step 3: Verify Port File

From your UE5 project directory:

```bash
cd /home/lpm/REPO/UETRAIL  # Or your UE5 project directory
cat .ueserver/rpc.json
```

Expected output:
```json
{
  "port": 45231,
  "pid": 12345,
  "started": "2025-12-26T10:30:00Z"
}
```

## Step 4: Test with netcat (Optional)

Manual test of RPC server:

```bash
PORT=$(jq -r .port .ueserver/rpc.json)
echo '{"id":"test-001","op":"ping"}' | nc localhost $PORT
```

Expected response:
```json
{"id":"test-001","op":"ping","ok":true,"version":"0.1.0"}
```

## Step 5: Test Bridge CLI

From your UE5 project directory:

```bash
python3 -m ue_bridge ue.ping
```

Expected output:
```json
{"tool": "ue.ping", "ok": true, "response": {"id": "cli-...", "op": "ping", "ok": true, "version": "0.1.0"}}
```

## Step 6: Test Error Cases

### Test 1: Server Not Running

```bash
# Stop UE5
python3 -m ue_bridge ue.ping
```

Expected error:
```json
{"ok": false, "error": "UE RPC server not running: .ueserver/rpc.json not found. Start UE5 with UEServer plugin enabled."}
```

### Test 2: Stale Port File

```bash
# 1. Start UE5 to create .ueserver/rpc.json
# 2. Kill UE5 process forcefully (kill -9 <pid>)
# 3. The port file will remain (not cleaned up)

python3 -m ue_bridge ue.ping
```

Expected error:
```json
{"ok": false, "error": "Stale port file detected: process 12345 is not running. Remove .ueserver/rpc.json and restart UE5."}
```

Clean up:
```bash
rm .ueserver/rpc.json
```

## Success Criteria

MVP is complete when all tests pass:
- ✅ UE5 starts TCP server on dynamic port
- ✅ `.ueserver/rpc.json` is created with correct info
- ✅ `ue-bridge ue.ping` returns successful response
- ✅ Error handling works (server down, stale files)

## Troubleshooting

**"No module named ue_bridge"**
- Install the bridge: `cd bridge/ && pip install -e .`
- Ensure you're using the same Python environment

**"UE RPC server not running"**
- Start UE5 with UEServer plugin enabled
- Ensure you're running from the UE5 project directory (where `.ueserver/` exists)

**"Cannot reach UE RPC"**
- Check that UE5 is still running
- Verify the port in `.ueserver/rpc.json` matches the server

**Type checking fails**
- Run `mypy ue_bridge/` from the bridge directory
- All code should pass strict type checking

## Next Steps

Once MVP testing is complete:
1. Implement MCP adapter (wraps bridge CLI)
2. Add more RPC operations (list widgets, trigger actions, etc.)
3. Integrate with Claude Code via MCP

## Development Workflow

```bash
# From bridge directory
cd /home/lpm/REPO/UEServer/bridge

# Type checking
python3 -m mypy ue_bridge/

# Linting
ruff check ue_bridge/

# Manual testing
cd /home/lpm/REPO/UETRAIL  # Your UE5 project
python3 -m ue_bridge ue.ping
```
