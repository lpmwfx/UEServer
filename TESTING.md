# Testing Guide

## Manual Testing: RPC Server

### Prerequisites

- UE5.7+ installed
- UETRAIL project (or any UE5 project with UEServer plugin installed)
- `nc` (netcat) command available

### Step 1: Start UE5 with Plugin

1. Navigate to the UETRAIL project:
   ```bash
   cd /home/lpm/REPO/UETRAIL
   ```

2. Start UE5 Editor:
   ```bash
   # Option 1: GUI mode
   ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor UETRAIL.uproject

   # Option 2: Headless mode (for testing)
   ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor UETRAIL.uproject -log -stdout -unattended
   ```

3. Check the UE5 output log for UEServer startup messages:
   ```
   LogTemp: UEServer: Starting module...
   LogTemp: UEServerRPC: Listening on 127.0.0.1:<port>
   LogTemp: UEServerRPC: Runtime state written to <path>/.ueserver/rpc.json
   LogTemp: UEServer: RPC server started on port <port>
   ```

### Step 2: Verify Runtime State File

1. Check that `.ueserver/rpc.json` was created:
   ```bash
   cd /home/lpm/REPO/UETRAIL
   cat .ueserver/rpc.json
   ```

2. Expected output:
   ```json
   {
     "port": 45231,
     "pid": 12345,
     "started": "2025-12-26T10:30:00Z"
   }
   ```

3. Extract the port number:
   ```bash
   PORT=$(cat .ueserver/rpc.json | grep -o '"port":[0-9]*' | grep -o '[0-9]*')
   echo "RPC Server Port: $PORT"
   ```

### Step 3: Test Ping with netcat

1. Send a ping request:
   ```bash
   cd /home/lpm/REPO/UETRAIL
   PORT=$(cat .ueserver/rpc.json | grep -o '"port":[0-9]*' | grep -o '[0-9]*')
   echo '{"id":"test-1","op":"ping"}' | nc localhost $PORT
   ```

2. Expected response:
   ```json
   {"id":"test-1","op":"ping","ok":true,"version":"0.1.0"}
   ```

3. Check UE5 log for server-side logging:
   ```
   LogTemp: UEServerRPC: Client connected
   LogTemp: UEServerRPC: Received request: {"id":"test-1","op":"ping"}
   LogTemp: UEServerRPC: Sent response: {"id":"test-1","op":"ping","ok":true,"version":"0.1.0"}
   ```

### Step 4: Test Error Handling

1. **Invalid JSON:**
   ```bash
   echo 'not-json' | nc localhost $PORT
   ```
   Expected: `{"ok":false,"error":"Invalid JSON"}`

2. **Unknown operation:**
   ```bash
   echo '{"id":"test-2","op":"unknown"}' | nc localhost $PORT
   ```
   Expected: `{"ok":false,"op":"unknown","error":"Unknown operation: unknown"}`

3. **Missing operation field:**
   ```bash
   echo '{"id":"test-3"}' | nc localhost $PORT
   ```
   Expected: Error response (server should handle missing `op` field gracefully)

### Step 5: Test Cleanup

1. Close UE5 Editor

2. Verify `.ueserver/rpc.json` was removed:
   ```bash
   cd /home/lpm/REPO/UETRAIL
   ls -la .ueserver/
   ```
   Expected: Directory should be empty or file should be gone

3. Check UE5 log for shutdown messages:
   ```
   LogTemp: UEServer: Shutting down module...
   LogTemp: UEServerRPC: Stopping...
   LogTemp: UEServerRPC: Thread exiting
   LogTemp: UEServerRPC: Removed runtime state file
   LogTemp: UEServerRPC: Stopped
   LogTemp: UEServer: Module shut down
   ```

## Automated Test Script

Save this as `test-rpc.sh` in the UETRAIL directory:

```bash
#!/bin/bash
set -e

echo "=== UEServer RPC Test ==="

# Check if .ueserver/rpc.json exists
if [ ! -f .ueserver/rpc.json ]; then
  echo "ERROR: .ueserver/rpc.json not found. Is UE5 running with UEServer plugin?"
  exit 1
fi

# Extract port
PORT=$(cat .ueserver/rpc.json | grep -o '"port":[0-9]*' | grep -o '[0-9]*')
echo "Found RPC server on port: $PORT"

# Test 1: Ping
echo -e "\n[Test 1] Ping..."
RESPONSE=$(echo '{"id":"test-1","op":"ping"}' | nc localhost $PORT)
echo "Response: $RESPONSE"

# Validate response contains expected fields
if echo "$RESPONSE" | grep -q '"ok":true' && echo "$RESPONSE" | grep -q '"version":"0.1.0"'; then
  echo "✓ Ping test PASSED"
else
  echo "✗ Ping test FAILED"
  exit 1
fi

# Test 2: Invalid JSON
echo -e "\n[Test 2] Invalid JSON..."
RESPONSE=$(echo 'not-json' | nc localhost $PORT)
echo "Response: $RESPONSE"

if echo "$RESPONSE" | grep -q '"ok":false' && echo "$RESPONSE" | grep -q '"error"'; then
  echo "✓ Invalid JSON test PASSED"
else
  echo "✗ Invalid JSON test FAILED"
  exit 1
fi

# Test 3: Unknown operation
echo -e "\n[Test 3] Unknown operation..."
RESPONSE=$(echo '{"id":"test-3","op":"unknown"}' | nc localhost $PORT)
echo "Response: $RESPONSE"

if echo "$RESPONSE" | grep -q '"ok":false' && echo "$RESPONSE" | grep -q 'Unknown operation'; then
  echo "✓ Unknown operation test PASSED"
else
  echo "✗ Unknown operation test FAILED"
  exit 1
fi

echo -e "\n=== All tests PASSED ==="
```

Make it executable:
```bash
chmod +x test-rpc.sh
```

Run it:
```bash
cd /home/lpm/REPO/UETRAIL
./test-rpc.sh
```

## Expected Outcomes

### Success Criteria
- ✅ UE5 starts without errors
- ✅ Plugin loads and logs startup messages
- ✅ `.ueserver/rpc.json` is created with valid port
- ✅ Ping request returns `{"ok":true,"version":"0.1.0"}`
- ✅ Invalid requests return error responses
- ✅ `.ueserver/rpc.json` is removed on shutdown

### Common Issues

**Plugin not loaded:**
- Check `UETRAIL.uproject` has `UEServer` in plugins list
- Verify plugin files exist in `Plugins/UEServer/`
- Check UE5 log for plugin errors

**Port file not created:**
- Check UE5 log for "Failed to write runtime state"
- Verify UETRAIL directory is writable
- Check disk space

**Connection refused:**
- Verify port from `.ueserver/rpc.json` matches
- Check firewall not blocking localhost
- Ensure UE5 is still running

**No response from server:**
- Check UE5 log for "Client connected" message
- Verify JSON format is correct (use `jq` to validate)
- Try adding newline: `echo '{"op":"ping"}' | nc localhost $PORT`

---

**Created by**: TwistedBrain and Claude Sonnet 4.5
