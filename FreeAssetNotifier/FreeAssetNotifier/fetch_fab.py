import os
import time
import re
import subprocess
from datetime import datetime
from DrissionPage import ChromiumPage, ChromiumOptions

# 마지막으로 전송한 에셋 이름을 저장하여 중복 방지
CACHE_FILE = "last_fab.txt"

def get_browser_options():
    options = ChromiumOptions()
    # 크롬 경로 자동 탐색
    browser_paths = [
        r'C:\Program Files\Google\Chrome\Application\chrome.exe',
        r'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe',
        os.path.expanduser(r'~\AppData\Local\Google\Chrome\Application\chrome.exe')
    ]
    for path in browser_paths:
        if os.path.exists(path):
            options.set_paths(browser_path=path)
            break

    options.headless(False) # 시각적 확인 및 안정성을 위해 창 모드 유지
    options.set_argument('--no-sandbox')
    options.set_argument('--disable-blink-features=AutomationControlled')
    options.set_argument('--lang=ko-KR')
    return options

def sanitize_for_cmd(text):
    """CMD 인자 전달 시 인코딩 오류 및 특수문자 방지"""
    if not text: return "N/A"
    
    # 날짜 변환 (3월 24일 -> 2026/03/24)
    current_year = datetime.now().year
    date_match = re.search(r'(\d+)월\s*(\d+)일', text)
    if date_match:
        month = date_match.group(1).zfill(2)
        day = date_match.group(2).zfill(2)
        return f"{current_year}/{month}/{day}"
    
    cleaned = re.sub(r'[^\w\s\d\-\[\]\.\/\:]', '', text)
    return cleaned.strip()

def fetch_fab_free_asset():
    options = get_browser_options()
    page = None
    
    try:
        page = ChromiumPage(addr_or_opts=options)
        page.get("https://www.fab.com/ko/limited-time-free")
        
        print(" - [Fab] Checking for updates...")
        if page.wait.ele_displayed('tag:h2', timeout=30):
            time.sleep(5) # 동적 콘텐츠 로딩 대기

            try:
                # 데이터 스크래핑
                raw_date = page.ele('tag:h2').text
                date_text = sanitize_for_cmd(raw_date)

                title_ele = page.ele('css:div.fabkit-Typography-ellipsisWrapper')
                asset_name = sanitize_for_cmd(title_ele.text) if title_ele else ""

                if not asset_name:
                    print(" - [Error] Could not find asset title.")
                    return

                img_ele = page.ele('css:div[class*="Thumbnail-root"] img')
                img_url = img_ele.attr('src') if img_ele else "N/A"
                if img_url.startswith('//'): img_url = 'https:' + img_url

                # 중복 체크 로직
                last_asset = ""
                if os.path.exists(CACHE_FILE):
                    with open(CACHE_FILE, "r", encoding="utf-8") as f:
                        last_asset = f.read().strip()

                if last_asset == asset_name:
                    print(f" - Up to date. (Current: {asset_name})")
                    return

                # 알림 전송 (인자 5개: 스토어, 이름, 날짜, 이미지, 쿠폰)
                print(f" - New Asset Detected: {asset_name}")
                store_name = "Fab Store"
                cmd = f'python send_and_publish.py "{store_name}" "{asset_name}" "{date_text}" "{img_url}" "N/A"'
                
                result = subprocess.run(cmd, shell=True, timeout=40)
                
                if result.returncode == 0:
                    with open(CACHE_FILE, "w", encoding="utf-8") as f:
                        f.write(asset_name)
                    print(" - [Success] Notification sent and cache updated.")
                else:
                    print(f" - [Fail] Discord script returned error code: {result.returncode}")

            except Exception as e:
                print(f" - [Error] Data extraction failed: {e}")
        else:
            print(" - [Error] Fab page elements not found.")
            
    except Exception as e:
        print(f" - [Critical Error] {e}")
    finally:
        if page:
            page.quit()

if __name__ == "__main__":
    fetch_fab_free_asset()