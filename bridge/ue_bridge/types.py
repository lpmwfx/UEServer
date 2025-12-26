"""Type definitions for UE Bridge."""

from typing import Any, NotRequired, Protocol, TypedDict


class ToolContext(TypedDict):
    """Context for tool execution."""

    port: int
    """TCP port for UE RPC server"""

    host: str
    """Host address (default: 127.0.0.1)"""

    timeout_ms: int
    """Request timeout in milliseconds"""

    request_id: NotRequired[str]
    """Optional request ID override"""


# Use dict instead of TypedDict for flexibility with dynamic keys
UEResponse = dict[str, Any]
"""Response from UE RPC server - allows any fields"""


class ToolHandler(Protocol):
    """Protocol for tool handler functions."""

    def __call__(
        self, args: dict[str, Any], ctx: ToolContext
    ) -> Any:  # Returns Coroutine, but Protocol can't express that well
        """
        Handle tool invocation.

        Args:
            args: Tool arguments from user
            ctx: Execution context with host, port, timeout

        Returns:
            Coroutine that resolves to response dictionary
        """
        ...


class PortDiscoveryResult(TypedDict, total=False):
    """Result from port discovery."""

    ok: bool
    """Whether port was discovered successfully"""

    port: int
    """Discovered port number"""

    pid: int
    """Server process ID"""

    started: str
    """Server start timestamp (ISO 8601)"""

    error: str
    """Error message if ok=False"""


class ToolMetadata(TypedDict):
    """Metadata for a tool (for auto-registration)."""

    name: str
    """Tool name (e.g., 'ue.ping')"""

    description: str
    """Human-readable description"""

    schema: dict[str, Any]
    """JSON schema for tool arguments"""
