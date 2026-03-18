# Free Asset Notifier

### 프로젝트 소개

유니티 에셋 스토어와 Fab 스토어의 기간 한정 무료 에셋을 자동으로 감지하여 디스코드로 알림을 보내는 프로그램입니다.
GitHub Actions를 통해 매일 정해진 시간에 서버에서 자동으로 실행되므로, 별도의 개인 PC 구동이 필요 없는 완전 자동화 봇입니다.

---

### 🚀 시작하기 (How to Use)

#### 1. 리포지토리 포크 (Fork)
- 이 리포지토리 우측 상단의 `Fork` 버튼을 눌러 본인의 GitHub 계정으로 복사합니다.

#### 2. 디스코드 봇 설정 (Discord Setup)
- [Discord Developer Portal](https://discord.com/developers/applications)에서 봇을 생성합니다.
- **Bot Token**과 알림을 보낼 **Channel ID**를 미리 메모해두세요.

#### 3. GitHub Secrets 등록 (Configuration)
포크한 리포지토리의 Settings > Secrets and variables > Actions 메뉴에서 다음 두 항목을 추가합니다.
- `DISCORD_BOT_TOKEN`: 메모한 디스코드 봇 토큰 입력
- `DISCORD_CHANNEL_ID`: 알림을 받을 디스코드 채널 ID 입력

#### 4. 워크플로우 활성화 (Activation)
- 상단 **Actions** 탭으로 이동하여 `I understand my workflows, go ahead and enable them` 버튼을 눌러 액션을 활성화합니다.
- 좌측 **Daily Asset Update Check** 워크플로우를 선택한 후, `Run workflow`를 눌러 즉시 테스트할 수 있습니다.

#### 5. 자동 실행 (Execution)
- 설정이 완료되면 **매일 오전 9시**(**KST**)에 GitHub 서버가 자동으로 에셋을 체크하고 알림을 보냅니다.
- 중복 알림 방지를 위한 캐시 파일(`last_*.txt`)은 GitHub Actions가 실행 후 자동으로 커밋하여 저장소에 업데이트합니다.

#### 유의 사항
- **봇 발송 제한**: 디스코드 API의 Rate Limit에 걸릴 경우 메시지 전송이 지연될 수 있습니다. 테스트 시 짧은 시간 내에 반복 실행은 피해주세요.
- **인코딩**: 한글 메시지 전송을 위해 모든 소스 코드는 UTF-8 with BOM 인코딩을 준수합니다.

---

### 개발 기간

3일 (2026.03.16 ~ 2026.03.18)

### 개발 환경

#### 사용 언어 및 프레임워크
- **C++17**: 메인 로직 처리 및 디스코드 API 통신
- **Python 3.10**: DrissionPage를 이용한 Fab 스토어 동적 웹 크롤링
- **nlohmann/json**: C++ JSON 데이터 파싱 및 직렬화
- **DrissionPage**: 헤드리스 브라우저 기반 웹 자동화 라이브러리

#### 인프라  도구
- **GitHub Actions**: 매일 자동 실행(Cron) 및 빌드 환경 제공 (Windows runner)
- **MSVC (cl.exe)**: GitHub 서버 내 C++ 컴파일러
- **Visual Studio 2022**: 로컬 개발 및 디버깅
- **Google Gemini 3 Flash**: 프로젝트 설계 및 문제 해결 파트너
