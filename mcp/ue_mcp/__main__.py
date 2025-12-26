"""Module execution entry point for ue-mcp server."""

import sys

from .server import start_server


def main() -> None:
    """Entry point for console script."""
    try:
        start_server()
    except KeyboardInterrupt:
        sys.exit(0)
    except Exception as err:
        print(f"Fatal error: {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
