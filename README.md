# Free Asset Notifier

### 프로젝트 소개

**유니티 에셋 스토어와 Fab 스토어의 기간 한정 무료 에셋을 자동으로 감지하여 디스코드로 알림을 보내는 프로그램**을 바이브 코딩으로 구현하는 프로젝트.

---

### 🚀 시작하기 (How to Use)

#### 1. 필수 준비물 (Prerequisites)
이 프로그램은 C++ 실행 파일과 Python 스크립트가 협력하여 작동합니다.
- **Python 3.x** 설치 필요
- **DrissionPage** 라이브러리 설치:
```bash
pip install DrissionPage
```

#### 2. 파일 배치 (File Setup)
깃허브에서 다운로드한 파일들을 하나의 폴더에 모아주세요. 폴더 구조는 다음과 같아야 합니다:
- `FreeAssetNotifier.exe` (실행 파일)
- `fetch_fab.py` (Fab 데이터 수집 스크립트)
- `config.txt` (사용자 생성 필요)

#### 3. 환경 설정 (Configuration)
- 실행 파일과 같은 폴더에 `config.txt` 파일을 생성합니다.
- 알림을 받을 디스코드 채널의 **웹훅(Webhook) URL**을 입력합니다.
- 여러 채널에 보내려면 한 줄에 하나씩 URL을 입력하고 저장하세요.

**[config.txt 작성 예시]**
```text
https://discord.com/api/webhooks/123456789/Your_Webhook_URL_1
https://discord.com/api/webhooks/987654321/Your_Webhook_URL_2
```

#### 4. 실행 및 테스트 (Execution)
- `FreeAssetNotifier.exe` 파일을 더블 클릭하여 실행합니다.
- 프로그램이 에셋 스토어를 확인하고, 새로운 에셋이 발견되면 즉시 디스코드로 알림을 보냅니다.
- 최초 실행 후에는 `last_asset_*.txt` 파일이 생성되어 중복 알림을 방지합니다.


---

### 개발 기간

2일 (2026.03.16 ~ 2026.03.17)

### 개발 환경

#### 사용 언어 및 프레임워크
- Language: C++, Python
- Library: DrissionPage (Web Scraping), nlohmann/json (C++ JSON Library)

#### 엔진 및 개발 도구
- Visual Studio 2022
- Google Gemini
