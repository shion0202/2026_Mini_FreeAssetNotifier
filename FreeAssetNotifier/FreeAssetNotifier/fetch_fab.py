# -*- coding: utf-8-sig -*-
from DrissionPage import ChromiumPage, ChromiumOptions
import os
import time

def fetch_fab_info():
    co = ChromiumOptions()
    
    # --- GitHub Actions 및 일반 환경 공통 설정 ---
    co.set_argument('--headless')
    co.set_argument('--no-sandbox')
    co.set_argument('--disable-gpu')
    co.set_argument('--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36')
    co.set_argument('--log-level=3')

    # --- 브라우저 경로 설정 로직 ---
    # 1. GITHUB_ACTIONS 환경 변수가 있으면 경로 지정을 생략 (기본 크롬 사용)
    if os.environ.get('GITHUB_ACTIONS'):
        print("Running on GitHub Actions: Using default Chrome.")
    else:
        # 2. 로컬 환경일 경우 네이버 웨일 경로 탐색
        user_home = os.path.expanduser("~")
        whale_paths = [
            os.path.join(user_home, r'AppData\Local\Naver\Naver Whale\Application\whale.exe'),
            r'C:\Program Files\Naver\Naver Whale\Application\whale.exe',
            r'C:\Program Files (x86)\Naver\Naver Whale\Application\whale.exe'
        ]
        found_path = next((p for p in whale_paths if os.path.exists(p)), None)
        
        if found_path:
            co.set_browser_path(found_path)
            print(f"Local environment: Whale found at {found_path}")
        else:
            # 웨일이 없으면 DrissionPage가 시스템 크롬을 찾도록 내버려 둠 (에러 출력 대신 로그만)
            print("Whale not found, trying to find system default browser.")

    page = None
    try:
        page = ChromiumPage(co)
        page.get('https://www.fab.com/ko/limited-time-free')

        # 에셋 제목 클래스가 로드될 때까지 최대 15초 대기
        target_css = 'css:div.fabkit-Typography-ellipsisWrapper'
        
        if page.ele(target_css, timeout=15):
            time.sleep(2) # 서버 환경에서는 안전하게 조금 더 대기

            # 데이터 추출
            title_el = page.ele(target_css)
            asset_name = title_el.text.strip() if title_el else "Unknown Asset"

            img_el = page.ele('css:div.fabkit-Thumbnail-root img')
            img_url = img_el.attr('src') if img_el else ""

            # 기간 텍스트 추출
            time_el = page.ele('t:h2@@text():기간 한정 무료')
            full_time_text = time_el.text.strip() if time_el else "정보 없음"

            with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
                f.write(f"{asset_name}\n{img_url}\n{full_time_text}")
            print(f"Successfully fetched: {asset_name}")
        else:
            with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
                f.write("ERROR: Page elements not found within timeout.")

    except Exception as e:
        with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
            f.write(f"ERROR: {str(e)}")
        print(f"Error occurred: {e}")
    finally:
        if page:
            page.quit()

if __name__ == "__main__":
    fetch_fab_info()