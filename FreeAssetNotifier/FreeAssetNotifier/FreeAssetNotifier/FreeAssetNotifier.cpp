#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <functional>
#include <iomanip>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

struct AssetInfo {
    string name;
    string storeName;
    string coupon;
    string link;
    string imageUrl;
    string endDate;
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

// curl의 실행 결과를 string으로 받아오기 위한 헬퍼 함수
string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    auto pipe = _popen(cmd, "r"); // Windows 환경: _popen, 리눅스: popen
    if (!pipe) throw runtime_error("_popen() failed!");
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    _pclose(pipe);
    return result;
}

void SendAndPublishDiscord(const string& token, const string& channelId, const AssetInfo& info) {
    if (info.name.empty()) return;

    try {
        // 1. 메시지 페이로드 생성
        json payload;
        payload["content"] = "🔔 **새로운 무료 에셋 알림!**";

        json embed = json::object();
        embed["title"] = info.name;
        embed["url"] = info.link;
        embed["color"] = (info.storeName.find("Fab") != string::npos) ? 3066993 : 2236962;
        embed["fields"] = json::array({
            { {"name", "🎁 쿠폰 코드"}, {"value", "`" + info.coupon + "`"}, {"inline", true} },
            { {"name", "⏰ 종료 예정일"}, {"value", info.endDate}, {"inline", false} },
            { {"name", "🛒 스토어"}, {"value", info.storeName}, {"inline", true} }
            });
        if (!info.imageUrl.empty()) embed["image"] = { {"url", info.imageUrl} };
        payload["embeds"] = json::array({ embed });

        string jsonString = payload.dump(-1, ' ', false, json::error_handler_t::replace);

        // 임시 파일 생성 (JSON 전달용)
        string tempJsonFile = "temp_payload.json";
        ofstream o(tempJsonFile);
        o << jsonString;
        o.close();

        // 2. 메시지 전송 API 호출 (POST)
        string sendUrl = "https://discord.com/api/v10/channels/" + channelId + "/messages";
        string sendCmd = "curl -s -X POST \"" + sendUrl + "\" "
            "-H \"Authorization: Bot " + token + "\" "
            "-H \"Content-Type: application/json\" "
            "-d @\"" + tempJsonFile + "\"";

        cout << " - Sending message to Discord..." << endl;
        string response = exec(sendCmd.c_str());
        remove(tempJsonFile.c_str());

        // 3. 응답에서 message_id 추출 및 게시(Crosspost)
        auto resJson = json::parse(response);
        if (resJson.contains("id")) {
            string messageId = resJson["id"];
            cout << " - Message sent (ID: " << messageId << "). Publishing..." << endl;

            string publishUrl = "https://discord.com/api/v10/channels/" + channelId + "/messages/" + messageId + "/crosspost";
            string publishCmd = "curl -s -X POST \"" + publishUrl + "\" "
                "-H \"Authorization: Bot " + token + "\" "
                "-H \"Content-Type: application/json\"";

            exec(publishCmd.c_str());
            cout << " - Successfully published to followers!" << endl;
        }
        else {
            cout << " - [Error] Failed to get message ID. Response: " << response << endl;
        }
    }
    catch (const exception& e) {
        cout << " - [Critical Error] Discord Process: " << e.what() << endl;
    }
}

string ConvertUnityDate(string rawDate) {
    // "end " 이후의 텍스트만 추출 (예: "March 19, 2026 at 7:59am PT.")
    size_t startPos = rawDate.find("end ");
    if (startPos == string::npos) return rawDate;
    string target = rawDate.substr(startPos + 4);

    // 월 이름 매핑 테이블
    map<string, string> months = {
        {"January", "01"}, {"February", "02"}, {"March", "03"}, {"April", "04"},
        {"May", "05"}, {"June", "06"}, {"July", "07"}, {"August", "08"},
        {"September", "09"}, {"October", "10"}, {"November", "11"}, {"December", "12"}
    };

    try {
        stringstream ss(target);
        string monthName, dayStr, yearStr;

        // "March 19, 2026" 순서로 읽기
        ss >> monthName >> dayStr >> yearStr;

        // 쉼표(,) 제거
        if (!dayStr.empty() && dayStr.back() == ',') dayStr.pop_back();
        if (!yearStr.empty() && yearStr.back() == '.') yearStr.pop_back();

        // 한 자리 숫자 날짜 앞에 0 붙이기 (예: 9 -> 09)
        if (dayStr.length() == 1) dayStr = "0" + dayStr;

        // 최종 변환: YYYY/MM/DD
        if (months.count(monthName)) {
            return yearStr + "/" + months[monthName] + "/" + dayStr;
        }
    }
    catch (...) {
        return rawDate; // 변환 실패 시 원본 반환
    }
    return rawDate;
}

string ConvertFabDate(string rawDate) {
    // 예: "기간 한정 무료 (3월 24일 오후 10시 59분까지)"
    try {
        size_t start = rawDate.find("(");
        size_t end = rawDate.find(")");
        if (start == string::npos || end == string::npos) return rawDate;

        string target = rawDate.substr(start + 1, end - start - 1); // "3월 24일 ..."

        // 현재 연도 구하기 (Fab은 연도가 안 나오므로 현재 연도 기준)
        time_t t = time(NULL);
        struct tm tm;
        localtime_s(&tm, &t);
        int currentYear = tm.tm_year + 1900;

        int month, day;
        // "3월 24일" 패턴 추출
        if (sscanf_s(target.c_str(), "%d월 %d일", &month, &day) == 2) {
            stringstream ss;
            ss << currentYear << "/" << setfill('0') << setw(2) << month << "/" << setw(2) << day;
            return ss.str(); // 결과: 2026/03/24
        }
    }
    catch (...) {
        return rawDate;
    }
    return rawDate;
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

    // 유니티 HTML에서 기간 텍스트 추출 부분
    // p class="truncate text-sm" 타겟팅
    size_t datePos = content.find("p class=\"truncate text-sm\">");
    if (datePos != string::npos) {
        size_t start = content.find(">", datePos) + 1;
        size_t end = content.find("</p>", start);
        string rawDateText = content.substr(start, end - start);

        // 변환 함수 호출 (예: 2026/03/19)
        info.endDate = ConvertUnityDate(rawDateText);
    }

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
        string nameLine, imgLine, dateLine;

        // 에셋 이름
        if (getline(resFile, nameLine)) {
            if (nameLine.size() >= 3 && (unsigned char)nameLine[0] == 0xEF) nameLine.erase(0, 3);
            info.name = nameLine;
        }

        // 이미지 URL
        if (getline(resFile, imgLine)) {
            info.imageUrl = imgLine;
        }

        // 종료 날짜 (추가됨)
        if (getline(resFile, dateLine)) {
            info.endDate = ConvertFabDate(dateLine); // 정제 후 저장
        }

        resFile.close();
        if (info.name.find("ERROR") == string::npos) {
            cout << " - [Success] Captured Asset: " << info.name << endl;
            if (!info.imageUrl.empty()) cout << " - [Success] Image URL found." << endl;
            if (!info.endDate.empty()) cout << " - [Success] End Date: " << info.endDate << endl;
        }
        remove("temp_fab.txt");
    }
    return info;
}

int main() {
    // 설정 로드 (Bot Token & Channel ID)
    ifstream configFile("config.txt");
    string botToken, channelId;
    if (!getline(configFile, botToken) || !getline(configFile, channelId)) {
        cout << "[Error] Invalid config.txt. Need Token on line 1 and Channel ID on line 2." << endl;
        return 1;
    }
    configFile.close();

    vector<StoreConfig> stores = {
        { "Unity Asset Store", "https://assetstore.unity.com/ko-KR/publisher-sale", "unity_source.html", "last_unity.txt", ParseUnityAsset },
        { "Fab", "https://www.fab.com/ko/limited-time-free", "fab_source.html", "last_fab.txt", ParseFabAsset }
    };

    for (const auto& store : stores) {
        cout << "[" << store.storeName << "] Checking..." << endl;
        AssetInfo current;
        if (store.storeName == "Fab") current = store.parseFunc("");
        else if (DownloadPageSource(store.url, store.tempFile)) current = store.parseFunc(store.tempFile);

        if (current.name.empty()) continue;

        string lastAssetName = "";
        ifstream fin(store.cacheFile);
        if (fin.is_open()) { getline(fin, lastAssetName); fin.close(); }

        if (current.name != lastAssetName) {
            cout << " - New Asset: " << current.name << endl;
            SendAndPublishDiscord(botToken, channelId, current);
            ofstream fout(store.cacheFile);
            if (fout.is_open()) { fout << current.name; fout.close(); }
        }
        else {
            cout << " - Up to date." << endl;
        }
    }
    return 0;
}
