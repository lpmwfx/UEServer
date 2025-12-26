#!/usr/bin/env python3
"""Test bridge CLI without UE5 running - tests error handling."""

import json
import os
import sys
from pathlib import Path

# Add ue_bridge to path
sys.path.insert(0, str(Path(__file__).parent))

from ue_bridge.port_discovery import discover_port


def test_port_discovery_no_server():
    """Test port discovery when server is not running."""
    print("Test 1: Port discovery when UE5 is not running")
    print("-" * 50)

    # Test from a directory without .ueserver/rpc.json
    os.chdir("/tmp")
    result = discover_port()

    print(f"Result: {json.dumps(result, indent=2)}")

    if not result["ok"]:
        print("✅ Correctly detected that server is not running")
    else:
        print("❌ Should have detected missing server")

    print()


def test_port_discovery_with_mock():
    """Test port discovery with a mock port file."""
    print("Test 2: Port discovery with mock port file")
    print("-" * 50)

    # Create temporary directory with mock .ueserver/rpc.json
    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        ueserver_dir = Path(tmpdir) / ".ueserver"
        ueserver_dir.mkdir()

        rpc_json = ueserver_dir / "rpc.json"

        # Write mock data with current process PID (should be running)
        mock_data = {
            "port": 12345,
            "pid": os.getpid(),  # Current process - should be "running"
            "started": "2025-12-26T10:00:00Z"
        }

        rpc_json.write_text(json.dumps(mock_data))

        # Test discovery
        os.chdir(tmpdir)
        result = discover_port()

        print(f"Result: {json.dumps(result, indent=2)}")

        if result["ok"] and result["port"] == 12345:
            print("✅ Successfully discovered port from mock file")
        else:
            print("❌ Failed to discover port")

    print()


def test_stale_port_file():
    """Test detection of stale port file (dead PID)."""
    print("Test 3: Stale port file detection")
    print("-" * 50)

    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        ueserver_dir = Path(tmpdir) / ".ueserver"
        ueserver_dir.mkdir()

        rpc_json = ueserver_dir / "rpc.json"

        # Write mock data with PID that definitely doesn't exist
        mock_data = {
            "port": 12345,
            "pid": 999999,  # Very unlikely to be a real process
            "started": "2025-12-26T10:00:00Z"
        }

        rpc_json.write_text(json.dumps(mock_data))

        # Test discovery
        os.chdir(tmpdir)
        result = discover_port()

        print(f"Result: {json.dumps(result, indent=2)}")

        if not result["ok"] and "stale" in result.get("error", "").lower():
            print("✅ Correctly detected stale port file")
        else:
            print("❌ Should have detected stale port file")

    print()


def main():
    """Run all tests."""
    print("=" * 50)
    print("UE Bridge - Error Handling Tests")
    print("=" * 50)
    print()

    test_port_discovery_no_server()
    test_port_discovery_with_mock()
    test_stale_port_file()

    print("=" * 50)
    print("Tests complete!")
    print()
    print("To test with real UE5 server:")
    print("1. Start UE5 with UEServer plugin")
    print("2. cd /home/lpm/REPO/UETRAIL  # Your UE5 project")
    print("3. python3 -m ue_bridge ue.ping")
    print("=" * 50)


if __name__ == "__main__":
    main()
