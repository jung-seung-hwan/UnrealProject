import unittest
from unittest.mock import patch

import main
from server_registry import GameServerRegistry


class FakeCursor:
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        return False

    def execute(self, query, params):
        self.query = query
        self.params = params

    def fetchone(self):
        return {"idx": 7, "nickname": "pilot", "level": 3}


class FakeConnection:
    def cursor(self):
        return FakeCursor()

    def close(self):
        pass


class LoginResponseTests(unittest.TestCase):
    def test_login_contains_latest_game_server_address(self):
        test_registry = GameServerRegistry(ttl_seconds=90)
        test_registry.register("server-a", "192.0.2.10", 7777, "Lobby")

        with (
            patch.object(main, "registry", test_registry),
            patch.object(main, "get_connection", return_value=FakeConnection()),
        ):
            response = main.login(main.AuthRequest(user_id="pilot", passwd="pw"))

        self.assertTrue(response.result)
        self.assertEqual(response.game_server_address, "192.0.2.10:7777")

    def test_login_succeeds_without_active_game_server(self):
        test_registry = GameServerRegistry(ttl_seconds=90)

        with (
            patch.object(main, "registry", test_registry),
            patch.object(main, "get_connection", return_value=FakeConnection()),
        ):
            response = main.login(main.AuthRequest(user_id="pilot", passwd="pw"))

        self.assertTrue(response.result)
        self.assertEqual(response.game_server_address, "")
        self.assertIn("접속 가능한 게임 서버", response.message)


if __name__ == "__main__":
    unittest.main()
