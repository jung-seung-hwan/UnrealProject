import unittest

from server_registry import GameServerRegistry


class GameServerRegistryTests(unittest.TestCase):
    def test_register_and_read_latest_server(self):
        registry = GameServerRegistry(ttl_seconds=90)

        registered = registry.register("server-a", "10.0.0.5", 7777, "Lobby")

        self.assertEqual(registered.address, "10.0.0.5:7777")
        self.assertEqual(registry.latest(), registered)

    def test_same_server_id_is_refreshed(self):
        registry = GameServerRegistry(ttl_seconds=90)
        registry.register("server-a", "10.0.0.5", 7777, "Lobby")

        refreshed = registry.register("server-a", "203.0.113.10", 7788, "Game")

        self.assertEqual(len(registry.active_servers()), 1)
        self.assertEqual(refreshed.address, "203.0.113.10:7788")

    def test_ipv6_address_is_bracketed(self):
        registry = GameServerRegistry(ttl_seconds=90)

        registered = registry.register("server-v6", "2001:db8::10", 7777)

        self.assertEqual(registered.address, "[2001:db8::10]:7777")


if __name__ == "__main__":
    unittest.main()
