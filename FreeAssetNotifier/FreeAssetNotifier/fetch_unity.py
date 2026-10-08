# -*- coding: utf-8 -*-
# 유니티 무료 에셋 섹션이 JS로 그려지는 경우를 위해 브라우저로 렌더링한 HTML을 unity_source.html에 저장
# (C++ 쪽 ParseUnityAsset이 이 파일을 다시 파싱함)
from DrissionPage import ChromiumPage, ChromiumOptions
import time

URL = 'https://assetstore.unity.com/publisher-sale'

def fetch_unity_html():
    co = ChromiumOptions()
    co.set_argument('--headless')
    co.set_argument('--no-sandbox')
    co.set_argument('--disable-gpu')
    co.set_argument('--lang=en-US')
    co.set_argument('--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36')

    page = None
    try:
        page = ChromiumPage(co)
        page.get(URL)

        # 무료 에셋 섹션이 렌더링될 때까지 최대 20초 대기
        for _ in range(20):
            if 'asset giveaway' in page.html.lower():
                break
            time.sleep(1)
        time.sleep(2)  # 이미지/날짜 등 나머지 요소 로딩 여유

        with open("unity_source.html", "w", encoding="utf-8") as f:
            f.write(page.html)
        print("Unity page rendered and saved.")
    except Exception as e:
        print(f"Unity browser fetch failed: {e}")
    finally:
        if page:
            page.quit()

if __name__ == "__main__":
    fetch_unity_html()
