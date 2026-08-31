from fastapi import FastAPI, Request
from pydantic import BaseModel, Field
import pymysql

from db import get_connection
from server_registry import registry

app = FastAPI(title="UnrealProject Auth Server")


class AuthRequest(BaseModel):
    user_id: str = Field(min_length=1)
    passwd: str = Field(min_length=1)


class AuthResponse(BaseModel):
    result: bool
    message: str = ""
    idx: int = 0
    nickname: str = ""
    level: int = 0
    game_server_address: str = ""


class ServerRegistrationRequest(BaseModel):
    server_id: str = Field(min_length=1, max_length=128)
    port: int = Field(default=7777, ge=1, le=65535)
    map_name: str = Field(default="", max_length=255)
    advertised_host: str = Field(default="", max_length=255)


class GameServerResponse(BaseModel):
    server_id: str
    address: str
    map_name: str = ""
    last_seen: str


class ServerRegistrationResponse(BaseModel):
    result: bool
    server: GameServerResponse
    expires_in_seconds: int


def to_server_response(server) -> GameServerResponse:
    return GameServerResponse(
        server_id=server.server_id,
        address=server.address,
        map_name=server.map_name,
        last_seen=server.last_seen.isoformat(),
    )


@app.post("/servers/register", response_model=ServerRegistrationResponse)
def register_server(req: ServerRegistrationRequest, request: Request):
    # 광고 주소를 지정하지 않으면 웹서버가 실제 요청 발신 주소를 사용한다.
    remote_host = request.client.host if request.client else "127.0.0.1"
    host = req.advertised_host.strip() or remote_host
    server = registry.register(req.server_id, host, req.port, req.map_name)
    return ServerRegistrationResponse(
        result=True,
        server=to_server_response(server),
        expires_in_seconds=registry.ttl_seconds,
    )


@app.get("/servers", response_model=list[GameServerResponse])
def list_servers():
    return [to_server_response(server) for server in registry.active_servers()]


@app.post("/signup", response_model=AuthResponse)
def signup(req: AuthRequest):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            try:
                cur.execute(
                    "INSERT INTO member (user_id, passwd, nickname, level)"
                    " VALUES (%s, %s, %s, 1)",
                    (req.user_id, req.passwd, req.user_id),
                )
            except pymysql.err.IntegrityError:
                return AuthResponse(result=False, message="이미 존재하는 아이디입니다")

            new_idx = cur.lastrowid

        conn.commit()
    finally:
        conn.close()

    return AuthResponse(
        result=True, idx=new_idx, nickname=req.user_id, level=1
    )


@app.post("/login", response_model=AuthResponse)
def login(req: AuthRequest):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT idx, nickname, level FROM member"
                " WHERE user_id = %s AND passwd = %s",
                (req.user_id, req.passwd),
            )
            row = cur.fetchone()
    finally:
        conn.close()

    if row is None:
        return AuthResponse(
            result=False, message="아이디 또는 비밀번호가 올바르지 않습니다"
        )

    game_server = registry.latest()
    return AuthResponse(
        result=True,
        idx=row["idx"],
        nickname=row["nickname"],
        level=row["level"],
        game_server_address=game_server.address if game_server else "",
        message="" if game_server else "현재 접속 가능한 게임 서버가 없습니다",
    )
