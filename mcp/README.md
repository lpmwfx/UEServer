# ue-mcp

Thin MCP wrapper for ue-bridge CLI.

## Architecture

```
Claude Code/MCP Client → ue-mcp → ue-bridge → UE RPC Server
                        [THIN]    [LOGIC]      [LOGIC]
```

**Important:** This is a THIN transport layer with ZERO business logic.
- All tool logic is in `ue-bridge`
- All UE logic is in `rpc/`
- MCP just forwards requests/responses

## Installation

```bash
# From UEServer root
cd mcp
pip install -r requirements.txt
pip install -e .
```

## Usage

### As Module
```bash
python -m ue_mcp
```

### As Command
```bash
ue-mcp
```

### With Claude Code

Add to your Claude Code MCP settings (`~/.claude/config.json` or project `.mcp.json`):

```json
{
  "mcpServers": {
    "ue": {
      "command": "python",
      "args": ["-m", "ue_mcp"],
      "cwd": "/path/to/your/ue/project"
    }
  }
}
```

**Important:** `cwd` must be your UE5 project directory for port discovery to work.

## Tools

All tools from `ue-bridge` are automatically registered:
- `ue.ping` - Ping UE server
- `ue.health` - Quick health check (500ms timeout)

Tools are auto-discovered from bridge - no manual registration needed.

## Development

### Type Checking
```bash
mypy ue_mcp
```

### Code Quality
```bash
ruff check ue_mcp
ruff format ue_mcp
```

## Design Principles

1. **Zero Logic** - All logic in bridge/RPC, not here
2. **Zero Duplication** - Direct import from bridge
3. **Auto-Discovery** - Tools auto-register from bridge
4. **Thin Transport** - Just MCP protocol wrapping

## Troubleshooting

**Error: "Cannot import ue_bridge"**
- Install bridge: `pip install -e ../bridge`

**Error: "UE RPC server not running"**
- Start UE5 with UEServer plugin enabled
- Check `~/.ueserver/switchboard.json` exists

**Error: "No matching UE instance found"**
- Run from UE project directory
- Or set correct `cwd` in MCP config
