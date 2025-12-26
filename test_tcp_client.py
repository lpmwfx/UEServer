#!/usr/bin/env python3
"""Test TCP client functionality."""

import sys
import asyncio
sys.path.insert(0, 'bridge')

from ue_bridge import tcp_client

async def test_tcp_client():
    """Test TCP client with ping operation."""
    try:
        # Create context
        ctx = {
            'port': 42079,
            'host': '127.0.0.1',
            'timeout_ms': 2000
        }
        
        result = await tcp_client.call_ue('ping', {}, ctx)
        print('TCP client test result:', result)
        return result
    except Exception as e:
        print('TCP client test error:', str(e))
        return None

if __name__ == '__main__':
    result = asyncio.run(test_tcp_client())
    if result:
        print('✅ TCP client test passed')
    else:
        print('❌ TCP client test failed')
