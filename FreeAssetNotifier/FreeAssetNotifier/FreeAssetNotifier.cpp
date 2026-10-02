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
    string error; // 파싱 실패 시 원인 (비어 있으면 성공)
    string fetchInfo; // 다운로드 진단 정보 (HTTP 코드, 크기 등)
};

// 상태 보고용: 스토어별 오늘의 확인 결과
struct StoreResult {
    string storeName;
    string status; // "NEW", "SAME", "FAIL"
    string assetName;
    string detail;
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

string Trim(const string& s) {
    size_t start = s.find_first_not_of(" \n\r\t");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \n\r\t");
    return s.substr(start, end - start + 1);
}

// ASCII 문자만 소문자로 변환 (UTF-8 멀티바이트는 그대로 두므로 위치가 원본과 일치)
string ToLowerAscii(string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    return s;
}

// 페이지 소스 다운로드 (Windows 내장 curl 사용)
// diag에는 HTTP 상태 코드와 다운로드 크기를 기록 (예: "HTTP 200, 512345 bytes")
bool DownloadPageSource(const string& url, const string& filename, string& diag) {
    // 헤더를 너무 많이 넣기보다, 가장 일반적인 크롬 브라우저 정보 하나만 사용해봅니다.
    string command = "curl -s -L -k "; // -k는 SSL 인증서 무시 (혹시 모를 에러 방지)
    command += "-A \"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\" ";
    command += "-w \"%{http_code} %{size_download}\" ";
    command += "\"" + url + "\" -o " + filename;

    string output = Trim(exec(command.c_str()));
    int httpCode = 0;
    long long size = 0;
    stringstream ss(output);
    ss >> httpCode >> size;

    if (httpCode == 0) diag = "연결 실패 (HTTP 응답 없음)";
    else diag = "HTTP " + to_string(httpCode) + ", " + to_string(size) + " bytes";
    cout << " - Download: " << diag << endl;
    return httpCode >= 200 && httpCode < 300 && size > 0;
}

// 한국 시간(KST) 기준 현재 시각 문자열
string GetKstNow() {
    time_t t = time(NULL) + 9 * 3600;
    struct tm tm;
    gmtime_s(&tm, &t);
    stringstream ss;
    ss << put_time(&tm, "%Y/%m/%d %H:%M") << " KST";
    return ss.str();
}

// 상태 보고 웹훅으로 오늘의 확인 결과 전송 (공지 채널과 별개)
void SendStatusReport(const string& webhookUrl, const vector<StoreResult>& results) {
    if (webhookUrl.empty()) {
        cout << "[Status] No status webhook configured. Skipping report." << endl;
        return;
    }

    try {
        bool anyFail = false;
        json fields = json::array();
        for (const auto& r : results) {
            string icon = "✅ 변경 없음";
            if (r.status == "NEW") icon = "🆕 새 에셋 알림 전송";
            else if (r.status == "FAIL") { icon = "❌ 확인 실패"; anyFail = true; }

            string value = icon;
            if (!r.assetName.empty()) value += "\n에셋: " + r.assetName;
            if (!r.detail.empty()) value += "\n" + r.detail;
            if (value.size() > 1000) value = value.substr(0, 1000) + "...";

            fields.push_back({ {"name", r.storeName}, {"value", value}, {"inline", false} });
        }

        json embed = json::object();
        embed["title"] = anyFail ? "⚠️ 기간 한정 무료 에셋 확인 결과" : "📋 기간 한정 무료 에셋 확인 결과";
        embed["description"] = GetKstNow();
        embed["color"] = anyFail ? 15158332 : 3066993;
        embed["fields"] = fields;

        // GitHub Actions 실행 시 해당 실행 로그 링크를 제목에 연결
        size_t len = 0;
        char* serverUrl = nullptr;
        char* repo = nullptr;
        char* runId = nullptr;
        _dupenv_s(&serverUrl, &len, "GITHUB_SERVER_URL");
        _dupenv_s(&repo, &len, "GITHUB_REPOSITORY");
        _dupenv_s(&runId, &len, "GITHUB_RUN_ID");
        if (serverUrl && repo && runId) {
            embed["url"] = string(serverUrl) + "/" + repo + "/actions/runs/" + runId;
        }
        free(serverUrl); free(repo); free(runId);

        json payload;
        payload["embeds"] = json::array({ embed });

        string tempJsonFile = "temp_status.json";
        ofstream o(tempJsonFile);
        o << payload.dump(-1, ' ', false, json::error_handler_t::replace);
        o.close();

        string cmd = "curl -s -o nul -w \"%{http_code}\" -X POST \"" + webhookUrl + "\" "
            "-H \"Content-Type: application/json\" "
            "-d @\"" + tempJsonFile + "\"";
        string code = Trim(exec(cmd.c_str()));
        remove(tempJsonFile.c_str());

        cout << "[Status] Report sent (HTTP " << code << ")" << endl;
    }
    catch (const exception& e) {
        cout << "[Status] Failed to send report: " << e.what() << endl;
    }
}

bool SendAndPublishDiscord(const string& token, const string& channelId, const AssetInfo& info) {
    if (info.name.empty()) return false;

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
            return true;
        }
        else {
            cout << " - [Error] Failed to get message ID. Response: " << response << endl;
        }
    }
    catch (const exception& e) {
        cout << " - [Critical Error] Discord Process: " << e.what() << endl;
    }
    return false;
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
    AssetInfo info;
    info.storeName = "Unity Asset Store";

    ifstream file(filename);
    if (!file.is_open()) {
        info.error = "다운로드한 HTML 파일을 열 수 없음";
        return info;
    }
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    string lowerContent = ToLowerAscii(content);

    // 기준점이 되는 문구 찾기
    string anchor = "ASSET GIVEAWAY";
    size_t anchorPos = content.find(anchor);
    if (anchorPos == string::npos) {
        anchor = "asset giveaway";
        anchorPos = content.find(anchor);
    }
    if (anchorPos == string::npos) {
        // "Asset Giveaway" 등 대소문자 표기가 바뀐 경우 대비
        anchorPos = lowerContent.find("asset giveaway");
        if (anchorPos != string::npos) cout << " - [Warn] Anchor found only by case-insensitive search." << endl;
    }
    if (anchorPos == string::npos) {
        // 원인 추정: 봇 차단 페이지인지, 문구 자체가 사라진 것인지 구분
        string reason = "'ASSET GIVEAWAY' 문구를 찾지 못함";
        if (lowerContent.find("just a moment") != string::npos || lowerContent.find("cf-chl") != string::npos ||
            lowerContent.find("captcha") != string::npos || lowerContent.find("access denied") != string::npos ||
            lowerContent.find("_incapsula_") != string::npos) {
            reason += " (봇 차단/챌린지 페이지로 추정)";
        }
        else if (lowerContent.find("giveaway") != string::npos) {
            reason += " ('giveaway' 단어는 있음 → 문구 변경 추정)";
        }
        else {
            reason += " (giveaway 관련 텍스트 없음 → 섹션 제거 또는 동적 로딩 추정)";
        }
        info.error = reason;
        return info;
    }

    // 에셋 이름 추출 (<h2> 태그)
    size_t h2Start = content.find("<h2", anchorPos);
    if (h2Start != string::npos) {
        size_t nameStart = content.find(">", h2Start) + 1;
        size_t h2End = content.find("</h2>", nameStart);
        if (h2End != string::npos) {
            info.name = Trim(content.substr(nameStart, h2End - nameStart));
        }
    }
    if (info.name.empty()) {
        info.error = "기준 문구는 찾았으나 그 뒤에서 에셋 이름(<h2>)을 찾지 못함";
        return info;
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
    info.storeName = "Fab";
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
        if (info.name.rfind("ERROR", 0) == 0) {
            // fetch_fab.py가 실패 원인을 "ERROR: ..." 형태로 기록함 → 에셋 이름으로 취급하지 않음
            info.error = info.name;
            info.name.clear();
        }
        else if (info.name.empty() || info.name == "Unknown Asset") {
            info.error = "에셋 제목 요소를 찾지 못함 (값: '" + info.name + "')";
            info.name.clear();
        }
        else {
            cout << " - [Success] Captured Asset: " << info.name << endl;
            if (!info.imageUrl.empty()) cout << " - [Success] Image URL found." << endl;
            if (!info.endDate.empty()) cout << " - [Success] End Date: " << info.endDate << endl;
        }
        remove("temp_fab.txt");
    }
    else {
        info.error = "fetch_fab.py 결과 파일(temp_fab.txt)이 생성되지 않음 (Python 실행 실패 추정)";
    }
    return info;
}

int main() {
    // 설정 로드 (Bot Token & Channel ID, 선택: 상태 보고 웹훅 URL)
    ifstream configFile("config.txt");
    string botToken, channelId, statusWebhook;
    if (!getline(configFile, botToken) || !getline(configFile, channelId)) {
        cout << "[Error] Invalid config.txt. Need Token on line 1 and Channel ID on line 2." << endl;
        return 1;
    }
    botToken = Trim(botToken);
    channelId = Trim(channelId);
    // 3번째 줄은 선택 사항 (없거나 비어 있으면 상태 보고 생략)
    if (getline(configFile, statusWebhook)) statusWebhook = Trim(statusWebhook);
    if (statusWebhook.find("http") != 0) statusWebhook.clear();
    configFile.close();

    vector<StoreConfig> stores = {
        { "Unity Asset Store", "https://assetstore.unity.com/ko-KR/publisher-sale", "unity_source.html", "last_unity.txt", ParseUnityAsset },
        { "Fab", "https://www.fab.com/ko/limited-time-free", "fab_source.html", "last_fab.txt", ParseFabAsset }
    };

    vector<StoreResult> results;

    for (const auto& store : stores) {
        cout << "[" << store.storeName << "] Checking..." << endl;
        StoreResult result;
        result.storeName = store.storeName;

        AssetInfo current;
        string fetchDiag;
        if (store.storeName == "Fab") {
            current = store.parseFunc("");
        }
        else if (DownloadPageSource(store.url, store.tempFile, fetchDiag)) {
            current = store.parseFunc(store.tempFile);
        }
        else {
            current.error = "페이지 다운로드 실패";
        }

        if (current.name.empty()) {
            if (current.error.empty()) current.error = "에셋 이름을 추출하지 못함 (원인 미상)";
            cout << " - [Error] " << current.error << endl;
            result.status = "FAIL";
            result.detail = current.error;
            if (!fetchDiag.empty()) result.detail += "\n" + fetchDiag;
            results.push_back(result);
            continue;
        }

        string lastAssetName = "";
        ifstream fin(store.cacheFile);
        if (fin.is_open()) { getline(fin, lastAssetName); fin.close(); }

        result.assetName = current.name;
        if (!fetchDiag.empty()) result.detail = fetchDiag;

        if (current.name != lastAssetName) {
            cout << " - New Asset: " << current.name << endl;
            if (SendAndPublishDiscord(botToken, channelId, current)) {
                result.status = "NEW";
                if (!lastAssetName.empty()) result.detail += (result.detail.empty() ? "" : "\n") + string("이전: ") + lastAssetName;
                // 전송에 성공했을 때만 캐시 갱신 (실패 시 다음 실행에서 재시도)
                ofstream fout(store.cacheFile);
                if (fout.is_open()) { fout << current.name; fout.close(); }
            }
            else {
                result.status = "FAIL";
                result.detail += (result.detail.empty() ? "" : "\n") + string("새 에셋을 찾았으나 디스코드 전송 실패 (다음 실행에서 재시도)");
            }
        }
        else {
            cout << " - Up to date." << endl;
            result.status = "SAME";
        }
        results.push_back(result);
    }

    SendStatusReport(statusWebhook, results);
    return 0;
}
