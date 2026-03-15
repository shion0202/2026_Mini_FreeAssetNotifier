# Free Asset Notifier

### 🚀 시작하기 전 설정

이 프로그램이 정상적으로 동작하려면 실행 파일(`.exe`)과 같은 폴더에 `config.txt` 파일이 반드시 존재해야 합니다.

1. 실행 파일(`FreeAssetNotifier.exe`)이 위치한 폴더로 이동합니다.
2. 해당 폴더에 **`config.txt`** 파일을 새롭게 생성합니다.
3. 알림을 받을 디스코드 채널의 웹훅(Webhook) URL을 입력하고 저장합니다.
   - **여러 채널**에 동시에 알림을 보내려면, 한 줄에 하나씩 웹훅 URL을 입력하세요.

**[config.txt 작성 예시]**
```text
https://discord.com/api/webhooks/123456789/Your_Webhook_URL_1
https://discord.com/api/webhooks/987654321/Your_Webhook_URL_2
```

### 프로젝트 소개

"디스코드 웹훅과 연동하여 유니티 에셋 스토어와 Fab의 기간 한정 무료 에셋이 갱신될 때마다 알림을 보내는 프로그램"을 바이브 코딩으로 구현하는 프로젝트.

### 개발 기간

개발 중 (2026.03.11 ~)

### 개발 환경

#### 사용 언어 및 프레임워크
- C++

#### 엔진 및 개발 도구
- Visual Studio 2022
- Google Gemini
