#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <functional>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

struct AssetInfo {
    string storeName;
    string name;
    string link;
    string coupon;
    string imageUrl;
};

// 스토어별 설정을 관리하는 구조체
struct StoreConfig {
    string storeName;
    string url;
    string tempFile;
    string cacheFile; // 스토어별 캐시 파일 (예: last_unity.txt, last_fab.txt)

    // 파싱 함수를 담는 변수 (함수 포인터 역할)
    // string(파일명)을 받아서 AssetInfo를 반환하는 함수 형태
    function<AssetInfo(const string&)> parseFunc;
};

// config.txt에서 여러 개의 웹훅 URL을 읽어오는 함수
vector<string> LoadWebhookUrls(const string& filename) {
    vector<string> urls;
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            // 앞뒤 공백 제거 (Trim)
            line.erase(0, line.find_first_not_of(" \n\r\t"));
            if (line.find_last_not_of(" \n\r\t") != string::npos) {
                line.erase(line.find_last_not_of(" \n\r\t") + 1);
            }

            // 빈 줄이 아니고 http로 시작하는 경우에만 추가
            if (!line.empty() && line.find("http") == 0) {
                urls.push_back(line);
            }
        }
        file.close();
    }
    return urls;
}

// 페이지 소스 다운로드 (Windows 내장 curl 사용)
bool DownloadPageSource(const string& url, const string& filename) {
    // 헤더를 너무 많이 넣기보다, 가장 일반적인 크롬 브라우저 정보 하나만 사용해봅니다.
    string command = "curl -s -L -k "; // -k는 SSL 인증서 무시 (혹시 모를 에러 방지)
    command += "-A \"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\" ";
    command += "\"" + url + "\" -o " + filename;

    int result = system(command.c_str());
    return (result == 0);
}

// 디스코드 웹훅 전송 (curl 사용)
void SendDiscordNotification(const vector<string>& webhookUrls, const AssetInfo& info) {
    if (info.name.empty()) return;

    try {
        json payload;
        payload["content"] = "🔔 **새로운 무료 에셋!**";

        json embed = json::object();
        embed["title"] = info.name;
        embed["url"] = info.link;
        // Fab 스토어 감지 로직 보완
        embed["color"] = (info.storeName.find("Fab") != string::npos) ? 3066993 : 2236962;

        embed["fields"] = json::array({
            { {"name", "🎁 쿠폰 코드"}, {"value", "`" + info.coupon + "`"}, {"inline", true} },
            { {"name", "🛒 스토어"}, {"value", info.storeName}, {"inline", true} }
            });

        if (!info.imageUrl.empty()) embed["image"] = { {"url", info.imageUrl} };
        payload["embeds"] = json::array({ embed });

        string tempJsonFile = "temp_payload.json";

        // JSON 생성 시 인코딩 예외를 방지하기 위한 dump 설정
        // 유효하지 않은 UTF-8 문자가 있어도 프로그램이 중단되지 않게 합니다.
        string jsonString = payload.dump(-1, ' ', false, json::error_handler_t::replace);

        ofstream o(tempJsonFile);
        if (o.is_open()) {
            o << jsonString;
            o.close();

            for (const string& url : webhookUrls) {
                string command = "curl -s -H \"Content-Type: application/json\" -X POST -d @\"" + tempJsonFile + "\" \"" + url + "\"";
                system(command.c_str());
            }
        }
        remove(tempJsonFile.c_str());
    }
    catch (const exception& e) {
        // 에러 발생 시 프로그램 종료 대신 메시지만 출력
        cout << " - [Critical Error] JSON Process: " << e.what() << endl;
    }
}

// HTML에서 에셋 이름, 링크, 쿠폰 코드를 추출하는 함수
AssetInfo ParseUnityAsset(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return {};
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    AssetInfo info;
    info.storeName = "Unity Asset Store";

    // 기준점이 되는 문구 찾기
    string anchor = "ASSET GIVEAWAY";
    size_t anchorPos = content.find(anchor);
    if (anchorPos == string::npos) {
        anchor = "asset giveaway";
        anchorPos = content.find(anchor);
        if (anchorPos == string::npos) return info;
    }

    // 에셋 이름 추출 (<h2> 태그)
    size_t h2Start = content.find("<h2", anchorPos);
    if (h2Start != string::npos) {
        size_t nameStart = content.find(">", h2Start) + 1;
        size_t h2End = content.find("</h2>", nameStart);
        if (h2End != string::npos) {
            info.name = content.substr(nameStart, h2End - nameStart);
        }
    }

    // 에셋 페이지 링크 추출
    size_t linkTagPos = content.find("<a href=\"/packages/", anchorPos);
    if (linkTagPos != string::npos) {
        size_t hrefStart = content.find("href=\"", linkTagPos) + 6;
        size_t hrefEnd = content.find("\"", hrefStart);
        if (hrefEnd != string::npos) {
            string relativePath = content.substr(hrefStart, hrefEnd - hrefStart);
            info.link = "https://assetstore.unity.com" + relativePath;
        }
    }

    // 쿠폰 코드 추출
    regex couponRegex("coupon code ([A-Z0-9]+)");
    smatch match;
    auto searchStart = content.cbegin() + anchorPos;
    auto searchEnd = content.cend();

    if (regex_search(searchStart, searchEnd, match, couponRegex)) {
        info.coupon = match[1].str();
    }

    // 에셋 대표 이미지 URL 추출 (앵커보다 위쪽 섹션부터 검색)
    size_t sectionStart = content.rfind("<section data-type=\"CalloutSlim\"", anchorPos);
    if (sectionStart == string::npos) sectionStart = 0;

    regex imgTagRegex("<img([^>]+)>");
    smatch tagMatch;

    auto imgSearchStart = content.cbegin() + sectionStart;
    auto imgSearchEnd = content.cend();

    bool found = false;
    while (regex_search(imgSearchStart, imgSearchEnd, tagMatch, imgTagRegex)) {
        string tagContent = tagMatch[1].str();

        if (tagContent.find("object-cover") != string::npos) {
            regex srcRegex("src=\"([^\"]+)\"");
            smatch srcMatch;
            if (regex_search(tagContent, srcMatch, srcRegex)) {
                string rawUrl = srcMatch[1].str();

                if (rawUrl.find(".svg") == string::npos) {
                    if (rawUrl.find("//") == 0) info.imageUrl = "https:" + rawUrl;
                    else info.imageUrl = rawUrl;

                    found = true;
                    break;
                }
            }
        }
        imgSearchStart = tagMatch[0].second;

        // 기준점 근처까지만 검색하여 엉뚱한 이미지 방지
        if (static_cast<size_t>(distance(content.cbegin(), imgSearchStart)) > anchorPos + 1000) break;
    }

    if (!found) cout << "Asset Image not found in the target section." << endl;

    return info;
}

AssetInfo ParseFabAsset(const string& filename) {
    AssetInfo info;
    info.storeName = "Fab Store";
    info.coupon = "N/A";
    info.link = "https://www.fab.com/ko/limited-time-free";

    system("python fetch_fab.py");

    ifstream resFile("temp_fab.txt");
    if (resFile.is_open()) {
        string nameLine, imgLine;

        // 첫 번째 줄: 에셋 이름
        if (getline(resFile, nameLine)) {
            // BOM 제거 로직 (생략 가능하나 안전을 위해 유지)
            if (nameLine.size() >= 3 && (unsigned char)nameLine[0] == 0xEF) nameLine.erase(0, 3);
            info.name = nameLine;
        }

        // 두 번째 줄: 이미지 URL
        if (getline(resFile, imgLine)) {
            info.imageUrl = imgLine;
        }

        resFile.close();
        if (info.name.find("ERROR") == string::npos) {
            cout << " - [Success] Captured Asset: " << info.name << endl;
            if (!info.imageUrl.empty()) cout << " - [Success] Image URL found." << endl;
        }
        remove("temp_fab.txt");
    }
    return info;
}

int main() {
    const string configFileName = "config.txt";

    // 웹훅 URL 로드
    vector<string> webhookUrls = LoadWebhookUrls(configFileName);
    if (webhookUrls.empty()) {
        cout << "[Error] No Webhook URLs found in " << configFileName << endl;
        return 1;
    }

    // 모든 스토어의 파싱 규칙과 캐시 파일을 정의
    vector<StoreConfig> stores = {
        {
            "Unity Asset Store",
            "https://assetstore.unity.com/ko-KR/publisher-sale",
            "unity_source.html",
            "last_unity.txt",
            ParseUnityAsset
        },
        {
            "Fab",
            "https://www.fab.com/ko/limited-time-free",
            "fab_source.html",
            "last_fab.txt",
            ParseFabAsset
        }
    };

    for (const auto& store : stores) {
        cout << "[" << store.storeName << "] Checking for updates..." << endl;

        AssetInfo current;
        bool downloadSuccess = true;

        // Fab Store는 파이썬 헬퍼가 직접 처리하므로 C++ 다운로드 과정을 건너뜁니다.
        if (store.storeName == "Fab") {
            current = store.parseFunc("");
        }
        else {
            // 유니티 등 일반 스토어는 기존처럼 다운로드 후 파싱
            if (DownloadPageSource(store.url, store.tempFile)) {
                current = store.parseFunc(store.tempFile);
            }
            else {
                downloadSuccess = false;
            }
        }

        if (!downloadSuccess) {
            cout << " - [Error] Failed to download page source for " << store.storeName << endl;
            continue;
        }

        if (current.name.empty()) {
            cout << " - [Error] Could not find asset info." << endl;
            continue;
        }

        // 해당 스토어 전용 캐시 파일 읽기
        string lastAssetName = "";
        ifstream fin(store.cacheFile);
        if (fin.is_open()) {
            getline(fin, lastAssetName);
            fin.close();
        }

        // 중복 확인 및 전송
        if (current.name != lastAssetName) {
            cout << " - New Asset Detected: " << current.name << endl;
            current.storeName = store.storeName;

            cout << " - Sending notifications..." << endl;
            SendDiscordNotification(webhookUrls, current);

            // 해당 스토어 전용 캐시 업데이트
            ofstream fout(store.cacheFile);
            if (fout.is_open()) {
                fout << current.name;
                fout.close();
            }
            cout << " - Successfully updated cache and sent notifications." << endl;
        }
        else {
            cout << " - Up to date. (Current: " << lastAssetName << ")" << endl;
        }
    }

    return 0;
}
