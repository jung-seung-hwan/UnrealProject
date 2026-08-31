# AI 기반 게임 서버 등록·자동 접속 기능

## 개요

이 기능은 FastAPI 웹서버를 게임 서버 디렉터리로 사용한다.

1. Unreal이 Listen Server 또는 Dedicated Server 월드를 시작한다.
2. `UWebApiSubsystem`이 `POST /servers/register`로 자신의 포트와 맵을 등록한다.
3. 이후 30초마다 heartbeat를 보내 등록을 갱신한다.
4. 클라이언트가 로그인하면 FastAPI가 최신 활성 게임 서버의 `host:port`를 함께 반환한다.
5. Unreal 클라이언트는 로그인 응답의 주소를 `GameServerAddress`에 저장한다.
6. `ConnectServerButton`을 누르면 저장된 게임 서버 주소로 이동한다.

등록이 90초 동안 갱신되지 않은 게임 서버는 자동으로 목록에서 제외된다. 이 값은 웹서버의 `GAME_SERVER_TTL_SECONDS` 환경변수로 변경할 수 있다.

## 웹서버 설정

`Server/.env.example`을 `Server/.env`로 복사하고 실제 DB 값을 입력한다.

```dotenv
DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=root
DB_PASSWORD=실제비밀번호
DB_NAME=seul
GAME_SERVER_TTL_SECONDS=90
```

`Server/.env`는 Git에서 제외되며 GitHub에 업로드되지 않는다.

의존성 설치 및 실행:

```powershell
cd Server
python -m venv .venv
.venv\Scripts\pip.exe install -r requirements.txt
.\run.bat
```

기본 웹서버 주소는 `http://127.0.0.1:8080`이다.

## Unreal 설정

`Config/DefaultGame.ini`의 기본값:

```ini
[WebApi]
DefaultWebServerHost=127.0.0.1
WebServerPort=8080
GameServerPort=7777
RegistrationHeartbeatSeconds=30.0
AdvertisedHost=
```

- Title 화면의 `ServerIP` 입력란은 **게임 서버 주소가 아니라 FastAPI 웹서버 주소**다.
- 같은 PC 또는 LAN 테스트에서는 `AdvertisedHost`를 비워 둔다. FastAPI가 요청 발신 IP를 등록한다.
- NAT, 프록시 또는 클라우드 환경에서는 외부에서 접속 가능한 주소를 `AdvertisedHost`에 넣는다.
- 실행 인자로 설정을 덮어쓸 수 있다.

```text
-WebServer=웹서버주소 -AdvertisedHost=외부게임서버주소 -Port=7777
```

예시:

```text
UnrealProject.exe /Game/Maps/Lobby?listen -log -WebServer=203.0.113.20 -AdvertisedHost=198.51.100.10 -Port=7777
```

Title 화면에서 로그인 후 `StartServerButton`을 누르는 기존 흐름도 지원한다. 이 경우 Lobby가 Listen Server로 열리면서 자동 등록된다.

## HTTP API

### 게임 서버 등록

```http
POST /servers/register
Content-Type: application/json

{
  "server_id": "프로세스별-고유-ID",
  "port": 7777,
  "map_name": "Lobby",
  "advertised_host": ""
}
```

`advertised_host`가 비어 있으면 웹서버가 본 요청 발신 IP를 사용한다.

### 활성 게임 서버 확인

```http
GET /servers
```

### 로그인 응답

```json
{
  "result": true,
  "message": "",
  "idx": 1,
  "nickname": "player",
  "level": 1,
  "game_server_address": "192.168.0.10:7777"
}
```

활성 서버가 없을 때도 로그인은 성공하지만 `game_server_address`는 빈 문자열이며 접속 버튼은 비활성화된다.

## 테스트 순서

1. MySQL과 FastAPI 웹서버를 실행한다.
2. Unreal 인스턴스 A에서 웹서버 주소를 입력하고 로그인한다.
3. `StartServerButton`을 눌러 Lobby Listen Server를 연다.
4. 브라우저 또는 curl로 `GET http://웹서버:8080/servers`를 호출해 주소가 등록됐는지 확인한다.
5. Unreal 인스턴스 B에서 같은 웹서버로 로그인한다.
6. 로그인 메시지에 게임 서버 주소가 표시되고 접속 버튼이 활성화되는지 확인한다.
7. 접속 버튼을 눌러 인스턴스 A의 Lobby로 이동하는지 확인한다.
8. 게임 서버 종료 후 90초가 지나면 `/servers` 결과에서 제거되는지 확인한다.

## 운영 시 주의사항

- 현재 서버 레지스트리는 FastAPI 프로세스 메모리에 저장된다. `run.bat`처럼 단일 worker로 실행해야 한다.
- 여러 웹 worker 또는 여러 웹서버 인스턴스를 운영할 때는 Redis 같은 공유 저장소로 교체해야 한다.
- 방화벽과 공유기에서 게임 포트(기본 UDP 7777)를 열어야 외부 클라이언트가 접속할 수 있다.
- 기존 회원 비밀번호 저장 방식은 학습용 구현이다. 실제 서비스 전에는 bcrypt/Argon2 해시와 TLS(HTTPS)를 적용해야 한다.
