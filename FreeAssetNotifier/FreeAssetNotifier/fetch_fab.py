# -*- coding: utf-8-sig -*-
from DrissionPage import ChromiumPage, ChromiumOptions
import os
import time

def fetch_fab_info():
    co = ChromiumOptions()
    co.set_argument('--headless')
    co.set_argument('--no-sandbox')
    co.set_argument('--disable-gpu')
    co.set_argument('--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36')
    co.set_argument('--log-level=3')

    user_home = os.path.expanduser("~")
    whale_paths = [
        os.path.join(user_home, r'AppData\Local\Naver\Naver Whale\Application\whale.exe'),
        r'C:\Program Files\Naver\Naver Whale\Application\whale.exe',
        r'C:\Program Files (x86)\Naver\Naver Whale\Application\whale.exe'
    ]
    found_path = next((p for p in whale_paths if os.path.exists(p)), None)
    
    if found_path:
        co.set_browser_path(found_path)
    else:
        with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
            f.write("ERROR: Browser not found.")
        return

    page = None
    try:
        page = ChromiumPage(co)
        page.get('https://www.fab.com/ko/limited-time-free')

        # --- 수정된 조건부 대기 로직 ---
        # 1. 에셋 제목 클래스가 로드될 때까지 최대 15초 대기
        target_css = 'css:div.fabkit-Typography-ellipsisWrapper'
        
        # ele_display 대신 wait 메서드를 사용하여 해당 요소가 나타날 때까지 기다립니다.
        if page.ele(target_css, timeout=15):
            # 요소가 발견되면 안전하게 1초만 더 쉬어줍니다.
            time.sleep(1)

            # 데이터 추출
            title_el = page.ele(target_css)
            asset_name = title_el.text.strip() if title_el else "Unknown Asset"

            img_el = page.ele('css:div.fabkit-Thumbnail-root img')
            img_url = img_el.attr('src') if img_el else ""

            # 상단에 크게 적힌 기간 텍스트 추출
            time_el = page.ele('t:h2@@text():기간 한정 무료')
            full_time_text = time_el.text.strip() if time_el else "정보 없음"

            with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
                f.write(f"{asset_name}\n{img_url}\n{full_time_text}")
        else:
            with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
                f.write("ERROR: Page elements not found within timeout.")

    except Exception as e:
        with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
            f.write(f"ERROR: {str(e)}")
    finally:
        if page:
            page.quit()

if __name__ == "__main__":
    fetch_fab_info()