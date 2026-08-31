from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import os
from threading import Lock
from time import monotonic


@dataclass(frozen=True)
class GameServer:
    server_id: str
    host: str
    port: int
    map_name: str
    last_seen: datetime
    expires_at: float

    @property
    def address(self) -> str:
        if ":" in self.host and not self.host.startswith("["):
            return f"[{self.host}]:{self.port}"
        return f"{self.host}:{self.port}"


class GameServerRegistry:
    """Thread-safe, process-local registry refreshed by Unreal heartbeats."""

    def __init__(self, ttl_seconds: int | None = None):
        configured_ttl = ttl_seconds or int(
            os.getenv("GAME_SERVER_TTL_SECONDS", "90")
        )
        self.ttl_seconds = max(configured_ttl, 10)
        self._servers: dict[str, GameServer] = {}
        self._lock = Lock()

    def register(
        self, server_id: str, host: str, port: int, map_name: str = ""
    ) -> GameServer:
        now = datetime.now(timezone.utc)
        server = GameServer(
            server_id=server_id,
            host=host,
            port=port,
            map_name=map_name,
            last_seen=now,
            expires_at=monotonic() + self.ttl_seconds,
        )
        with self._lock:
            self._remove_expired_locked()
            self._servers[server_id] = server
        return server

    def active_servers(self) -> list[GameServer]:
        with self._lock:
            self._remove_expired_locked()
            return sorted(
                self._servers.values(),
                key=lambda server: server.last_seen,
                reverse=True,
            )

    def latest(self) -> GameServer | None:
        servers = self.active_servers()
        return servers[0] if servers else None

    def _remove_expired_locked(self) -> None:
        now = monotonic()
        expired_ids = [
            server_id
            for server_id, server in self._servers.items()
            if server.expires_at <= now
        ]
        for server_id in expired_ids:
            del self._servers[server_id]


registry = GameServerRegistry()
