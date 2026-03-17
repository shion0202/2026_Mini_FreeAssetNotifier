# fetch_fab.py
from DrissionPage import ChromiumPage, ChromiumOptions
import sys
import os
import time

def fetch_fab_info():
    co = ChromiumOptions()
    co.set_argument('--headless')
    co.set_argument('--no-sandbox')
    co.set_argument('--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36')

    user_home = os.path.expanduser("~")
    whale_paths = [
        os.path.join(user_home, r'AppData\Local\Naver\Naver Whale\Application\whale.exe'),
        r'C:\Program Files\Naver\Naver Whale\Application\whale.exe',
        r'C:\Program Files (x86)\Naver\Naver Whale\Application\whale.exe'
    ]
    found_path = next((p for p in whale_paths if os.path.exists(p)), None)
    if found_path: co.set_browser_path(found_path)
    else: return

    page = None
    try:
        page = ChromiumPage(co)
        page.get('https://www.fab.com/ko/limited-time-free')
        time.sleep(10) 

        # 1. 첫 번째 에셋 이름 추출
        title_element = page.ele('css:div.fabkit-Typography-ellipsisWrapper', timeout=10)
        
        # 2. 첫 번째 에셋 이미지 URL 추출
        img_element = page.ele('css:div.fabkit-Thumbnail-root img', timeout=10)
        
        asset_name = title_element.text.strip() if title_element else "Unknown Asset"
        # 이미지 태그의 src 속성 가져오기
        img_url = img_element.attr('src') if img_element else ""

        # 두 정보를 줄바꿈으로 구분하여 저장
        with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
            f.write(f"{asset_name}\n{img_url}")
            
    except Exception as e:
        with open("temp_fab.txt", "w", encoding="utf-8-sig") as f:
            f.write(f"ERROR: {str(e)}")
    finally:
        if page: page.quit()

if __name__ == "__main__":
    fetch_fab_info()